/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>
#include "syncenginetestutils.h"
#include <syncengine.h>

using namespace OCC;

class FakeAsyncReply : public FakeReply
{
    Q_OBJECT
    QByteArray _pollLocation;

public:
    FakeAsyncReply(const QByteArray &pollLocation, QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent)
        : FakeReply { parent }
        , _pollLocation(pollLocation)
    {
        setRequest(request);
        setUrl(request.url());
        setOperation(op);
        open(QIODevice::ReadOnly);

        QMetaObject::invokeMethod(this, "respond", Qt::QueuedConnection);
    }

    Q_INVOKABLE void respond()
    {
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 202);
        setRawHeader("OC-JobStatus-Location", _pollLocation);
        emit metaDataChanged();
        emit finished();
    }

    void abort() override {}
    qint64 readData(char *, qint64) override { return 0; }
};


class TestAsyncOp : public QObject
{
    Q_OBJECT

private slots:

    void asyncUploadOperations()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        fakeFolder.syncEngine().account()->setCapabilities({ { "dav", QVariantMap{ { "chunking", "1.0" } } } });
        // Reduce max chunk size a bit so we get more chunks
        SyncOptions options;
        options._maxChunkSize = 20 * 1000;
        fakeFolder.syncEngine().setSyncOptions(options);
        int nGET = 0;

        // This test is made of several testcases.
        // the testCases maps a filename to a couple of callback.
        // When a file is uploaded, the fake server will always return the 202 code, and will set
        // the `perform` functor to what needs to be done to complete the transaction.
        // The testcase consist of the `pollRequest` which will be called when the sync engine
        // calls the poll url.
        struct TestCase
        {
            using PollRequest_t = std::function<QNetworkReply *(TestCase *, const QNetworkRequest &request)>;
            PollRequest_t pollRequest;
            std::function<FileInfo *()> perform = nullptr;
        };
        QHash<QString, TestCase> testCases;

        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *outgoingData) -> QNetworkReply * {
            auto path = request.url().path();

            if (op == QNetworkAccessManager::GetOperation && path.startsWith("/async-poll/")) {
                auto file = path.mid(sizeof("/async-poll/") - 1);
                Q_ASSERT(testCases.contains(file));
                auto &testCase = testCases[file];
                return testCase.pollRequest(&testCase, request);
            }

            if (op == QNetworkAccessManager::PutOperation && !path.contains("/uploads/")) {
                // Not chunking
                auto file = getFilePathFromUrl(request.url());
                Q_ASSERT(testCases.contains(file));
                auto &testCase = testCases[file];
                Q_ASSERT(!testCase.perform);
                auto putPayload = outgoingData->readAll();
                testCase.perform = [putPayload, request, &fakeFolder] {
                    return FakePutReply::perform(fakeFolder.remoteModifier(), request, putPayload);
                };
                return new FakeAsyncReply("/async-poll/" + file.toUtf8(), op, request, &fakeFolder.syncEngine());
            } else if (request.attribute(QNetworkRequest::CustomVerbAttribute) == "MOVE") {
                QString file = getFilePathFromUrl(QUrl::fromEncoded(request.rawHeader("Destination")));
                Q_ASSERT(testCases.contains(file));
                auto &testCase = testCases[file];
                Q_ASSERT(!testCase.perform);
                testCase.perform = [request, &fakeFolder] {
                    return FakeChunkMoveReply::perform(fakeFolder.uploadState(), fakeFolder.remoteModifier(), request);
                };
                return new FakeAsyncReply("/async-poll/" + file.toUtf8(), op, request, &fakeFolder.syncEngine());
            } else if (op == QNetworkAccessManager::GetOperation) {
                nGET++;
            }
            return nullptr;
        });


        // Callback to be used to finalize the transaction and return the success
        auto successCallback = [](TestCase *tc, const QNetworkRequest &request) {
            tc->pollRequest = [](TestCase *, const QNetworkRequest &) -> QNetworkReply * { std::abort(); }; // shall no longer be called
            FileInfo *info = tc->perform();
            QByteArray body = R"({ "status":"finished", "ETag":"\")" + info->etag + R"(\"", "fileId":")" + info->fileId + "\"}\n";
            return new FakePayloadReply(QNetworkAccessManager::GetOperation, request, body, nullptr);
        };
        // Callback that never finishes
        auto waitForeverCallback = [](TestCase *, const QNetworkRequest &request) {
            QByteArray body = "{\"status\":\"started\"}\n";
            return new FakePayloadReply(QNetworkAccessManager::GetOperation, request, body, nullptr);
        };
        // Callback that simulate an error.
        auto errorCallback = [](TestCase *tc, const QNetworkRequest &request) {
            tc->pollRequest = [](TestCase *, const QNetworkRequest &) -> QNetworkReply * { std::abort(); }; // shall no longer be called;
            QByteArray body = "{\"status\":\"error\",\"errorCode\":500,\"errorMessage\":\"TestingErrors\"}\n";
            return new FakePayloadReply(QNetworkAccessManager::GetOperation, request, body, nullptr);
        };
        // This lambda takes another functor as a parameter, and returns a callback that will
        // tell the client needs to poll again, and further call to the poll url will call the
        // given callback
        auto waitAndChain = [](const TestCase::PollRequest_t &chain) {
            return [chain](TestCase *tc, const QNetworkRequest &request) {
                tc->pollRequest = chain;
                QByteArray body = "{\"status\":\"started\"}\n";
                return new FakePayloadReply(QNetworkAccessManager::GetOperation, request, body, nullptr);
            };
        };

        // Create a testcase by creating a file of a given size locally and assigning it a callback
        auto insertFile = [&](const QString &file, qint64 size, TestCase::PollRequest_t cb) {
            fakeFolder.localModifier().insert(file, size);
            testCases[file] = { std::move(cb) };
        };
        fakeFolder.localModifier().mkdir("success");
        insertFile("success/chunked_success", options._maxChunkSize * 3, successCallback);
        insertFile("success/single_success", 300, successCallback);
        insertFile("success/chunked_patience", options._maxChunkSize * 3,
            waitAndChain(waitAndChain(successCallback)));
        insertFile("success/single_patience", 300,
            waitAndChain(waitAndChain(successCallback)));
        fakeFolder.localModifier().mkdir("err");
        insertFile("err/chunked_error", options._maxChunkSize * 3, errorCallback);
        insertFile("err/single_error", 300, errorCallback);
        insertFile("err/chunked_error2", options._maxChunkSize * 3, waitAndChain(errorCallback));
        insertFile("err/single_error2", 300, waitAndChain(errorCallback));

        // First sync should finish by itself.
        // All the things in "success/" should be transferred, the things in "err/" not
        QVERIFY(!fakeFolder.syncOnce());
        QCOMPARE(nGET, 0);
        QCOMPARE(*fakeFolder.currentLocalState().find("success"),
            *fakeFolder.currentRemoteState().find("success"));
        testCases.clear();
        testCases["err/chunked_error"] = { successCallback };
        testCases["err/chunked_error2"] = { successCallback };
        testCases["err/single_error"] = { successCallback };
        testCases["err/single_error2"] = { successCallback };

        fakeFolder.localModifier().mkdir("waiting");
        insertFile("waiting/small", 300, waitForeverCallback);
        insertFile("waiting/willNotConflict", 300, waitForeverCallback);
        insertFile("waiting/big", options._maxChunkSize * 3,
            waitAndChain(waitAndChain([&](TestCase *tc, const QNetworkRequest &request) {
                QTimer::singleShot(0, &fakeFolder.syncEngine(), &SyncEngine::abort);
                return waitAndChain(waitForeverCallback)(tc, request);
            })));

        QVERIFY(fakeFolder.syncJournal().wipeErrorBlacklist() != -1);

        // This second sync will redo the files that had errors
        // But the waiting folder will not complete before it is aborted.
        QVERIFY(!fakeFolder.syncOnce());
        QCOMPARE(nGET, 0);
        QCOMPARE(*fakeFolder.currentLocalState().find("err"),
            *fakeFolder.currentRemoteState().find("err"));

        testCases["waiting/small"].pollRequest = waitAndChain(waitAndChain(successCallback));
        testCases["waiting/big"].pollRequest = waitAndChain(successCallback);
        testCases["waiting/willNotConflict"].pollRequest =
            [&fakeFolder, &successCallback](TestCase *tc, const QNetworkRequest &request) {
                auto &remoteModifier = fakeFolder.remoteModifier(); // successCallback destroys the capture
                auto reply = successCallback(tc, request);
                // This is going to succeed, and after we just change the file.
                // This should not be a conflict, but this should be downloaded in the
                // next sync
                remoteModifier.appendByte("waiting/willNotConflict");
                return reply;
            };


        int nPUT = 0;
        int nMOVE = 0;
        int nDELETE = 0;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            auto path = request.url().path();
            if (op == QNetworkAccessManager::GetOperation && path.startsWith("/async-poll/")) {
                auto file = path.mid(sizeof("/async-poll/") - 1);
                Q_ASSERT(testCases.contains(file));
                auto &testCase = testCases[file];
                return testCase.pollRequest(&testCase, request);
            } else if (op == QNetworkAccessManager::PutOperation) {
                nPUT++;
            } else if (op == QNetworkAccessManager::GetOperation) {
                nGET++;
            } else if (op == QNetworkAccessManager::DeleteOperation) {
                nDELETE++;
            } else if (request.attribute(QNetworkRequest::CustomVerbAttribute) == "MOVE") {
                nMOVE++;
            }
            return nullptr;
        });

        // This last sync will do the waiting stuff
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(nGET, 1); // "waiting/willNotConflict"
        QCOMPARE(nPUT, 0);
        QCOMPARE(nMOVE, 0);
        QCOMPARE(nDELETE, 0);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }
};

QTEST_GUILESS_MAIN(TestAsyncOp)
#include "testasyncop.moc"
