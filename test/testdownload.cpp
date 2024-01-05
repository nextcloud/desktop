/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */


#include "owncloudpropagator.h"
#include "syncengine.h"
#include "testutils/syncenginetestutils.h"

#include <QtTest>

#ifndef Q_OS_WIN
#include <unistd.h>
#endif

using namespace std::chrono_literals;
using namespace OCC;

static constexpr auto stopAfter = 3_MiB;

/** A FakeGetReply that sends max 'fakeSize' bytes, but whose ContentLength has the corect size */
class BrokenFakeGetReply : public FakeGetReply
{
    Q_OBJECT
public:
    using FakeGetReply::FakeGetReply;
    qint64 fakeSize = stopAfter;

    qint64 bytesAvailable() const override
    {
        switch (state) {
        case State::Ok:
            return std::min(size, fakeSize) + QIODevice::bytesAvailable();
        default:
            return FakeGetReply::bytesAvailable();
        }
    }

    qint64 readData(char *data, qint64 maxlen) override
    {
        qint64 len = std::min(qint64{ fakeSize }, maxlen);
        std::fill_n(data, len, payload);
        size -= len;
        fakeSize -= len;
        return len;
    }
};


SyncFileItemPtr getItem(const QSignalSpy &spy, const QString &path)
{
    for (const QList<QVariant> &args : spy) {
        auto item = args[0].value<SyncFileItemPtr>();
        if (item->destination() == path)
            return item;
    }
    return {};
}


class TestDownload : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase_data()
    {
        QTest::addColumn<Vfs::Mode>("vfsMode");
        QTest::addColumn<bool>("filesAreDehydrated");

        QTest::newRow("Vfs::Off") << Vfs::Off << false;

        if (VfsPluginManager::instance().isVfsPluginAvailable(Vfs::WindowsCfApi)) {
            QTest::newRow("Vfs::WindowsCfApi dehydrated") << Vfs::WindowsCfApi << true;

            // TODO: the hydrated version will fail due to an issue in the winvfs plugin, so leave it disabled for now.
            // QTest::newRow("Vfs::WindowsCfApi hydrated") << Vfs::WindowsCfApi << false;
        } else if (Utility::isWindows()) {
            qWarning("Skipping Vfs::WindowsCfApi");
        }
    }

    void testResume()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        fakeFolder.syncEngine().setIgnoreHiddenFiles(true);
        QSignalSpy completeSpy(&fakeFolder.syncEngine(), &SyncEngine::itemCompleted);
        auto size = 30_MiB;
        fakeFolder.remoteModifier().insert(QStringLiteral("A/a0"), size);

        // First, download only the first 3 MB of the file
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::GetOperation && request.url().path().endsWith(QLatin1String("A/a0"))) {
                return new BrokenFakeGetReply(fakeFolder.remoteModifier(), op, request, this);
            }
            return nullptr;
        });

        if (filesAreDehydrated) {
            QVERIFY(fakeFolder.applyLocalModificationsAndSync()); // The sync should succeed, because there are no downloads, and only the placeholders get created
        } else {
            QVERIFY(!fakeFolder.applyLocalModificationsAndSync()); // The sync should fail because not all the files were downloaded
            QCOMPARE(getItem(completeSpy, QStringLiteral("A/a0"))->_status, SyncFileItem::SoftError);
            QCOMPARE(getItem(completeSpy, QStringLiteral("A/a0"))->_errorString, QStringLiteral("The file could not be downloaded completely."));
            QVERIFY(fakeFolder.syncEngine().isAnotherSyncNeeded());

            // Now, we need to restart, this time, it should resume.
            QByteArray rangeRequest;
            QByteArray rangeReply;
            OperationCounter counter;

            fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *device) -> QNetworkReply * {
                counter.serverOverride(op, request, device);
                if (op == QNetworkAccessManager::GetOperation && request.url().path().endsWith(QLatin1String("A/a0"))) {
                    rangeRequest = request.rawHeader("Range");
                    auto reply = new FakeGetReply(fakeFolder.remoteModifier(), op, request, this);
                    connect(reply, &FakeGetReply::metaDataChanged, reply, [&, reply] {
                        rangeReply = reply->rawHeader("Content-Range");
                    });
                    return reply;
                }
                return nullptr;
            });
            fakeFolder.syncJournal().wipeErrorBlacklist();
            QVERIFY(fakeFolder.applyLocalModificationsAndSync()); // now this should succeed
            QCOMPARE(rangeRequest, QByteArrayLiteral("bytes=") + QByteArray::number(stopAfter) + '-');
            QCOMPARE(rangeReply, QByteArrayLiteral("bytes ") + QByteArray::number(stopAfter) + '-');
            QCOMPARE(counter.nGET, 1);

            QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        }
    }


    void testResumeInNextSync()
    {
        /*
         * Same as testResume but we simulate a new sync from the Folder class, with a partial discovery.
         */
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        fakeFolder.syncEngine().setIgnoreHiddenFiles(true);
        QSignalSpy completeSpy(&fakeFolder.syncEngine(), &SyncEngine::itemCompleted);
        constexpr auto size = 30_MiB;
        fakeFolder.remoteModifier().insert(QStringLiteral("A/a0"), size);

        // First, download only the first 3 MB of the file
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::GetOperation && request.url().path().endsWith(QLatin1String("A/a0"))) {
                return new BrokenFakeGetReply(fakeFolder.remoteModifier(), op, request, this);
            }
            return nullptr;
        });

        if (filesAreDehydrated) {
            QVERIFY(fakeFolder.applyLocalModificationsAndSync()); // The sync should succeed, because there are no downloads, and only the placeholders get created
        } else {
            QVERIFY(!fakeFolder.applyLocalModificationsAndSync()); // The sync should fail because not all the files were downloaded
            QCOMPARE(getItem(completeSpy, QStringLiteral("A/a0"))->_status, SyncFileItem::SoftError);
            QCOMPARE(getItem(completeSpy, QStringLiteral("A/a0"))->_errorString, QStringLiteral("The file could not be downloaded completely."));
            QVERIFY(fakeFolder.syncEngine().isAnotherSyncNeeded());

            QByteArray ranges;
            fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
                if (op == QNetworkAccessManager::GetOperation && request.url().path().endsWith(QLatin1String("A/a0"))) {
                    ranges = request.rawHeader("Range");
                }
                return nullptr;
            });
            fakeFolder.syncJournal().wipeErrorBlacklist();
            // perform a partial sync
            fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem, {});
            QVERIFY(fakeFolder.applyLocalModificationsAndSync()); // now this should succeed
            QCOMPARE(ranges, QByteArray("bytes=" + QByteArray::number(stopAfter) + "-"));
            QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        }
    }

    void testErrorMessage () {
        // This test's main goal is to test that the error string from the server is shown in the UI

        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        fakeFolder.syncEngine().setIgnoreHiddenFiles(true);
        QSignalSpy completeSpy(&fakeFolder.syncEngine(), &SyncEngine::itemCompleted);
        const auto size = 3_MiB + 500_KiB;
        fakeFolder.remoteModifier().insert(QStringLiteral("A/broken"), size);

        QString serverMessage = QStringLiteral("The file was not downloaded because the tests wants so!");

        // First, download only the first 3 MB of the file
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::GetOperation && request.url().path().endsWith(QLatin1String("A/broken"))) {
                return new FakeErrorReply(op, request, this, 400,
                    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                    "<d:error xmlns:d=\"DAV:\" xmlns:s=\"http://sabredav.org/ns\">\n"
                    "<s:exception>Sabre\\DAV\\Exception\\Forbidden</s:exception>\n"
                    "<s:message>"
                        + serverMessage.toUtf8()
                        + "</s:message>\n"
                          "</d:error>");
            }
            return nullptr;
        });

        bool timedOut = false;
        QTimer::singleShot(10s, &fakeFolder.syncEngine(), [&]() {
            timedOut = true;
            fakeFolder.syncEngine().abort({});
        });
        if (filesAreDehydrated) {
            QVERIFY(fakeFolder.applyLocalModificationsAndSync()); // Success, because files are never downloaded
        } else {
            QVERIFY(!fakeFolder.applyLocalModificationsAndSync()); // Fail because A/broken
            QVERIFY(!timedOut);
            QCOMPARE(getItem(completeSpy, QStringLiteral("A/broken"))->_status, SyncFileItem::NormalError);
            QVERIFY(getItem(completeSpy, QStringLiteral("A/broken"))->_errorString.contains(serverMessage));
        }
    }

    void serverMaintenence() {
        // Server in maintenance must abort the sync.

        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        fakeFolder.remoteModifier().insert(QStringLiteral("A/broken"));
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::GetOperation) {
                return new FakeErrorReply(op, request, this, 503,
                    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                    "<d:error xmlns:d=\"DAV:\" xmlns:s=\"http://sabredav.org/ns\">\n"
                    "<s:exception>Sabre\\DAV\\Exception\\ServiceUnavailable</s:exception>\n"
                    "<s:message>System in maintenance mode.</s:message>\n"
                    "</d:error>");
            }
            return nullptr;
        });

        QSignalSpy completeSpy(&fakeFolder.syncEngine(), &SyncEngine::itemCompleted);
        if (filesAreDehydrated) {
            QVERIFY(fakeFolder.applyLocalModificationsAndSync()); // Success, because files are never downloaded
        } else {
            QVERIFY(!fakeFolder.applyLocalModificationsAndSync()); // Fail because A/broken
            // FatalError means the sync was aborted, which is what we want
            QCOMPARE(getItem(completeSpy, QStringLiteral("A/broken"))->_status, SyncFileItem::FatalError);
            QVERIFY(getItem(completeSpy, QStringLiteral("A/broken"))->_errorString.contains(QStringLiteral("System in maintenance mode")));
        }
    }

    void testHttp2Resend()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        fakeFolder.remoteModifier().insert(QStringLiteral("A/resendme"), 300_B);

        QString serverMessage = QStringLiteral("Needs to be resend on a new connection!");
        int resendActual = 0;
        int resendExpected = 2;

        // First, download only the first 3 MB of the file
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::GetOperation && request.url().path().endsWith(QLatin1String("A/resendme")) && resendActual < resendExpected) {
                auto errorReply = new FakeErrorReply(op, request, this, 400, "ignore this body");
                errorReply->setError(QNetworkReply::ContentReSendError, serverMessage);
                errorReply->setAttribute(QNetworkRequest::Http2WasUsedAttribute, true);
                errorReply->setAttribute(QNetworkRequest::HttpStatusCodeAttribute, QVariant());
                resendActual += 1;
                return errorReply;
            }
            return nullptr;
        });

        if (filesAreDehydrated) {
            QVERIFY(fakeFolder.applyLocalModificationsAndSync()); // Success, because files are never downloaded
        } else {
            QVERIFY(fakeFolder.applyLocalModificationsAndSync());
            QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
            QCOMPARE(resendActual, 2);

            fakeFolder.remoteModifier().appendByte(QStringLiteral("A/resendme"));
            resendActual = 0;
            resendExpected = 10;

            QSignalSpy completeSpy(&fakeFolder.syncEngine(), &SyncEngine::itemCompleted);
            QVERIFY(!fakeFolder.applyLocalModificationsAndSync());
            QCOMPARE(resendActual, 6); // AbstractNetworkJob::MaxRetryCount + 1
            QCOMPARE(getItem(completeSpy, QStringLiteral("A/resendme"))->_status, SyncFileItem::NormalError);
            QVERIFY(getItem(completeSpy, QStringLiteral("A/resendme"))->_errorString.contains(serverMessage));
        }
    }
};

QTEST_GUILESS_MAIN(TestDownload)
#include "testdownload.moc"
