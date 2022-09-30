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

static constexpr qint64 stopAfter = 3'123'668;

/** A FakeGetReply that sends max 'fakeSize' bytes, but whose ContentLength has the corect size */
class BrokenFakeGetReply : public FakeGetReply
{
    Q_OBJECT
public:
    using FakeGetReply::FakeGetReply;
    int fakeSize = stopAfter;

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

        if (isVfsPluginAvailable(Vfs::WindowsCfApi)) {
            QTest::newRow("Vfs::WindowsCfApi dehydrated") << Vfs::WindowsCfApi << true;

            // TODO: the hydrated version will fail due to an issue in the winvfs plugin, so leave it disabled for now.
            // QTest::newRow("Vfs::WindowsCfApi hydrated") << Vfs::WindowsCfApi << false;
        } else if (Utility::isWindows()) {
            QWARN("Skipping Vfs::WindowsCfApi");
        }
    }
    void testResume()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        fakeFolder.syncEngine().setIgnoreHiddenFiles(true);
        QSignalSpy completeSpy(&fakeFolder.syncEngine(), &SyncEngine::itemCompleted);
        auto size = 30_mb;
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
            QCOMPARE(getItem(completeSpy, "A/a0")->_status, SyncFileItem::SoftError);
            QCOMPARE(getItem(completeSpy, "A/a0")->_errorString, QString("The file could not be downloaded completely."));
            QVERIFY(fakeFolder.syncEngine().isAnotherSyncNeeded());

            // Now, we need to restart, this time, it should resume.
            QByteArray ranges;
            fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
                if (op == QNetworkAccessManager::GetOperation && request.url().path().endsWith("A/a0")) {
                    ranges = request.rawHeader("Range");
                }
                return nullptr;
            });
            fakeFolder.syncJournal().wipeErrorBlacklist();
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
        const auto size = 3_mb + 500_kb;
        fakeFolder.remoteModifier().insert(QStringLiteral("A/broken"), size);

        QByteArray serverMessage = "The file was not downloaded because the tests wants so!";

        // First, download only the first 3 MB of the file
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::GetOperation && request.url().path().endsWith(QLatin1String("A/broken"))) {
                return new FakeErrorReply(op, request, this, 400,
                    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                    "<d:error xmlns:d=\"DAV:\" xmlns:s=\"http://sabredav.org/ns\">\n"
                    "<s:exception>Sabre\\DAV\\Exception\\Forbidden</s:exception>\n"
                    "<s:message>"+serverMessage+"</s:message>\n"
                    "</d:error>");
            }
            return nullptr;
        });

        bool timedOut = false;
        QTimer::singleShot(10s, &fakeFolder.syncEngine(), [&]() { timedOut = true; fakeFolder.syncEngine().abort(); });
        if (filesAreDehydrated) {
            QVERIFY(fakeFolder.applyLocalModificationsAndSync()); // Success, because files are never downloaded
        } else {
            QVERIFY(!fakeFolder.applyLocalModificationsAndSync()); // Fail because A/broken
            QVERIFY(!timedOut);
            QCOMPARE(getItem(completeSpy, "A/broken")->_status, SyncFileItem::NormalError);
            QVERIFY(getItem(completeSpy, "A/broken")->_errorString.contains(serverMessage));
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
            QCOMPARE(getItem(completeSpy, "A/broken")->_status, SyncFileItem::FatalError);
            QVERIFY(getItem(completeSpy, "A/broken")->_errorString.contains("System in maintenance mode"));
        }
    }

    void testMoveFailsInAConflict() {
#ifdef Q_OS_WIN
        QSKIP("Not run on windows because permission on directory does not do what is expected");
#else
        if (getuid() == 0) {
            QSKIP("The permissions have no effect on the root user");
        }
#endif
        // Test for https://github.com/owncloud/client/issues/7015
        // We want to test the case in which the renaming of the original to the conflict file succeeds,
        // but renaming the temporary file fails.
        // This tests uses the fact that a "touchedFile" notification will be sent at the right moment.
        // Note that there will be first a notification on the file and the conflict file before.

        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        fakeFolder.syncEngine().setIgnoreHiddenFiles(true);
        fakeFolder.account()->setCapabilities(TestUtils::testCapabilities(CheckSums::Algorithm::ADLER32));
        fakeFolder.remoteModifier().setContents(QStringLiteral("A/a1"), FileModifier::DefaultFileSize, 'A');
        fakeFolder.localModifier().setContents(QStringLiteral("A/a1"), FileModifier::DefaultFileSize, 'B');

        bool propConnected = false;
        QString conflictFile;
        auto transProgress = connect(&fakeFolder.syncEngine(), &SyncEngine::transmissionProgress,
                                     [&](const ProgressInfo &pi) {
            auto propagator = fakeFolder.syncEngine().getPropagator();
            if (pi.status() != ProgressInfo::Propagation || propConnected || !propagator)
                return;
            propConnected = true;
            connect(propagator.data(), &OwncloudPropagator::touchedFile, [&](const QString &s) {
                if (s.contains(QLatin1String("conflicted copy"))) {
                    QCOMPARE(conflictFile, QString());
                    conflictFile = s;
                    return;
                }
                if (!conflictFile.isEmpty()) {
                    // Check that the temporary file is still there
                    QCOMPARE(QDir(fakeFolder.localPath() + "A/").entryList({"*.~*"}, QDir::Files | QDir::Hidden).count(), 1);
                    // Set the permission to read only on the folder, so the rename of the temporary file will fail
                    QFile folderA(fakeFolder.localPath() + "A/");
                    QVERIFY(folderA.setPermissions(QFile::Permissions(0x5555)));
                    // For some reason, on some linux system the setPermissions above succeeds, but the directory is still 755.
                    // So double check it here, otherwise the move will succeed, and the sync will succeed (while it should fail).
                    QCOMPARE(folderA.permissions(), QFile::Permissions(0x5555));
                }
            });
        });

        QVERIFY(!fakeFolder.applyLocalModificationsAndSync()); // The sync must fail because the rename failed
        QVERIFY(!conflictFile.isEmpty());

        // restore permissions
        QFile(fakeFolder.localPath() + "A/").setPermissions(QFile::Permissions(0x7777));

        QObject::disconnect(transProgress);
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &, QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::GetOperation)
                QTest::qFail("There shouldn't be any download", __FILE__, __LINE__);
            return nullptr;
        });
        fakeFolder.syncJournal().wipeErrorBlacklist();
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        // The a1 file is still tere and have the right content
        QVERIFY(fakeFolder.currentRemoteState().find("A/a1"));
        QCOMPARE(fakeFolder.currentRemoteState().find("A/a1")->contentChar, 'A');

        QVERIFY(QFile::remove(conflictFile)); // So the comparison succeeds;
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testHttp2Resend()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        fakeFolder.remoteModifier().insert(QStringLiteral("A/resendme"), 300_b);

        QByteArray serverMessage = "Needs to be resend on a new connection!";
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
            QCOMPARE(getItem(completeSpy, "A/resendme")->_status, SyncFileItem::NormalError);
            QVERIFY(getItem(completeSpy, "A/resendme")->_errorString.contains(serverMessage));
        }
    }
};

QTEST_GUILESS_MAIN(TestDownload)
#include "testdownload.moc"
