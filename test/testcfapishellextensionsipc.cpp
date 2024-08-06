/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include "configfile.h"
#include "account.h"
#include "accountstate.h"
#include "accountmanager.h"
#include "common/vfs.h"
#include "common/shellextensionutils.h"
#include "config.h"
#include "folderman.h"
#include "libsync/vfs/cfapi/shellext/configvfscfapishellext.h"
#include "ocssharejob.h"
#include "shellextensionsserver.h"
#include "syncengine.h"
#include "syncenginetestutils.h"
#include "testhelper.h"
#include "vfs/cfapi/shellext/customstateprovideripc.h"
#include "vfs/cfapi/shellext/thumbnailprovideripc.h"

#include <QTemporaryDir>
#include <QtTest>
#include <QImage>
#include <QPainter>

namespace {
static constexpr auto roootFolderName = "A";
static constexpr auto imagesFolderName = "photos";
static constexpr auto filesFolderName = "files";
static constexpr auto shellExtensionServerOverrideIntervalMs = 1000LL * 2LL;
}

using namespace OCC;

class TestCfApiShellExtensionsIPC : public QObject
{
    Q_OBJECT

    FolderMan _fm;

    FakeFolder fakeFolder{FileInfo()};

    QScopedPointer<FakeQNAM> fakeQnam;
    OCC::AccountPtr account;
    OCC::AccountState* accountState = nullptr;

    QScopedPointer<ShellExtensionsServer> _shellExtensionsServer;

    const QStringList dummmyImageNames = {
        { QString(QString(roootFolderName) + QLatin1Char('/') + QString(imagesFolderName) + QLatin1Char('/') + QStringLiteral("imageJpg.jpg")) },
        { QString(QString(roootFolderName) + QLatin1Char('/') + QString(imagesFolderName) + QLatin1Char('/') + QStringLiteral("imagePng.png")) },
        { QString(QString(roootFolderName) + QLatin1Char('/') + QString(imagesFolderName) + QLatin1Char('/') + QStringLiteral("imagePng.bmp")) }
    };
    QMap<QString, QByteArray> dummyImages;

    QString currentImage;

    struct FileStates
    {
        bool _isShared = false;
        bool _isLocked = false;
    };

    const QMap<QString, FileStates> dummyFileStates = {
        { QString(QString(roootFolderName) + QLatin1Char('/') + QString(filesFolderName) + QLatin1Char('/') + QStringLiteral("test_locked_file.txt")), { false, true } },
        { QString(QString(roootFolderName) + QLatin1Char('/') + QString(filesFolderName) + QLatin1Char('/') + QStringLiteral("test_shared_file.txt")), { true, false } },
        { QString(QString(roootFolderName) + QLatin1Char('/') + QString(filesFolderName) + QLatin1Char('/') + QStringLiteral("test_shared_and_locked_file.txt")), { true, true }},
        { QString(QString(roootFolderName) + QLatin1Char('/') + QString(filesFolderName) + QLatin1Char('/') + QStringLiteral("test_non_shared_and_non_locked_file.txt")), { false, false }}
    };

public:
    static bool replyWithNoShares;

signals:
    void propfindRequested();

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);

        QTemporaryDir dir;
        ConfigFile::setConfDir(dir.path());

        VfsShellExtensions::ThumbnailProviderIpc::overrideServerName = VfsShellExtensions::serverNameForApplicationNameDefault();
        VfsShellExtensions::CustomStateProviderIpc::overrideServerName = VfsShellExtensions::serverNameForApplicationNameDefault();

        _shellExtensionsServer.reset(new ShellExtensionsServer);
        _shellExtensionsServer->setIsSharedInvalidationInterval(shellExtensionServerOverrideIntervalMs);

        for (const auto &dummyImageName : dummmyImageNames) {
            const auto extension = dummyImageName.split(".").last();
            const auto format = dummyImageName.endsWith("PNG", Qt::CaseInsensitive) ? QImage::Format_ARGB32 : QImage::Format_RGB32;
            QImage image(QSize(640, 480), format);
            QPainter painter(&image);
            painter.setBrush(QBrush(Qt::red));
            painter.fillRect(QRectF(0, 0, 640, 480), Qt::red);
            QByteArray byteArray;
            QBuffer buffer(&byteArray);
            buffer.open(QIODevice::WriteOnly);
            image.save(&buffer, extension.toStdString().c_str());
            dummyImages.insert(dummyImageName, byteArray);
        }

        fakeFolder.remoteModifier().mkdir(roootFolderName);

        fakeFolder.remoteModifier().mkdir(QString(roootFolderName) + QLatin1Char('/') + QString(filesFolderName));

        fakeFolder.remoteModifier().mkdir(QString(roootFolderName) + QLatin1Char('/') + QString(imagesFolderName));

        for (const auto &fileStateKey : dummyFileStates.keys()) {
            fakeFolder.remoteModifier().insert(fileStateKey, 256);
        }

        fakeQnam.reset(new FakeQNAM({}));
        account = OCC::Account::create();
        account->setCredentials(new FakeCredentials{fakeQnam.data()});
        account->setUrl(QUrl(("http://example.de")));

        accountState = new OCC::AccountState(account);

        OCC::AccountManager::instance()->addAccount(account);

        FolderMan *folderman = FolderMan::instance();
        QCOMPARE(folderman, &_fm);
        QVERIFY(folderman->addFolder(accountState, folderDefinition(fakeFolder.localPath())));

        fakeQnam->setOverride(
            [this](QNetworkAccessManager::Operation op, const QNetworkRequest &req, QIODevice *device) {
                Q_UNUSED(device);
                QNetworkReply *reply = nullptr;

                const auto path = req.url().path();

                const auto customOperation = req.attribute(QNetworkRequest::CustomVerbAttribute);

                if (customOperation == QStringLiteral("PROPFIND")) {
                    emit propfindRequested();
                    reply = new FakePayloadReply(op, req, {}, nullptr);
                } else if (path.endsWith(ShellExtensionsServer::getFetchThumbnailPath())) {
                    const auto urlQuery = QUrlQuery(req.url());
                    const auto fileId = urlQuery.queryItemValue(QStringLiteral("fileId"));
                    const auto x = urlQuery.queryItemValue(QStringLiteral("x")).toInt();
                    const auto y = urlQuery.queryItemValue(QStringLiteral("y")).toInt();
                    if (fileId.isEmpty() || x <= 0 || y <= 0) {
                        reply = new FakePayloadReply(op, req, {}, nullptr);
                    } else {
                        const auto foundImageIt = dummyImages.find(currentImage);

                        QByteArray byteArray;
                        if (foundImageIt != dummyImages.end()) {
                            byteArray = foundImageIt.value();
                        }

                        currentImage.clear();

                        auto fakePayloadReply = new FakePayloadReply(op, req, byteArray, nullptr);

                        QMap<QNetworkRequest::KnownHeaders, QByteArray> additionalHeaders = {
                            {QNetworkRequest::KnownHeaders::ContentTypeHeader, "image/jpeg"}};
                        fakePayloadReply->_additionalHeaders = additionalHeaders;

                        reply = fakePayloadReply;
                    }
                } else {
                    reply = new FakePayloadReply(op, req, {}, nullptr);
                }
                
                return reply;
            });
    };

    void testRequestThumbnails()
    {
        QTemporaryDir dir;
        ConfigFile::setConfDir(dir.path());

        FolderMan *folderman = FolderMan::instance();
        QVERIFY(folderman);
        auto folder = FolderMan::instance()->folderForPath(fakeFolder.localPath());
        QVERIFY(folder);

        folder->setVirtualFilesEnabled(true);

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        ItemCompletedSpy completeSpy(fakeFolder);

        auto cleanup = [&]() {
            completeSpy.clear();
        };
        cleanup();

        // Create a virtual file for remote files
        for (const auto &dummyImageName : dummmyImageNames) {
            fakeFolder.remoteModifier().insert(dummyImageName, 256);
        }
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        cleanup();
        // just add records from fake folder's journal to real one's to make test work
        SyncJournalFileRecord record;
        auto realFolder = FolderMan::instance()->folderForPath(fakeFolder.localPath());
        QVERIFY(realFolder);
        for (const auto &dummyImageName : dummmyImageNames) {
            if (fakeFolder.syncJournal().getFileRecord(dummyImageName, &record)) {
                QVERIFY(realFolder->journalDb()->setFileRecord(record));
            }
        }

        // #1 Test every fake image fetching. Everything must succeed.
        for (const auto &dummyImageName : dummmyImageNames) {
            QEventLoop loop;
            QByteArray thumbnailReplyData;
            currentImage = dummyImageName;
            // emulate thumbnail request from a separate thread (just like the real shell extension does)
            std::thread t([&] {
                VfsShellExtensions::ThumbnailProviderIpc thumbnailProviderIpc;
                thumbnailReplyData = thumbnailProviderIpc.fetchThumbnailForFile(
                    fakeFolder.localPath() + dummyImageName, QSize(256, 256));
                QMetaObject::invokeMethod(&loop, &QEventLoop::quit, Qt::QueuedConnection);
            });
            loop.exec();
            t.detach();
            QVERIFY(!thumbnailReplyData.isEmpty());
            const auto imageFromData = QImage::fromData(thumbnailReplyData);
            QVERIFY(!imageFromData.isNull());
        }

        // #2 Test wrong image fetching. It must fail.
        QEventLoop loop;
        QByteArray thumbnailReplyData;
        std::thread t1([&] {
            VfsShellExtensions::ThumbnailProviderIpc thumbnailProviderIpc;
            thumbnailReplyData = thumbnailProviderIpc.fetchThumbnailForFile(
                fakeFolder.localPath() + QString("A/photos/wrong.jpg"), QSize(256, 256));
            QMetaObject::invokeMethod(&loop, &QEventLoop::quit, Qt::QueuedConnection);
        });
        loop.exec();
        t1.detach();
        QVERIFY(thumbnailReplyData.isEmpty());

        // #3 Test one image fetching, but set incorrect size. It must fail.
        currentImage = dummyImages.keys().first();
        std::thread t2([&] {
            VfsShellExtensions::ThumbnailProviderIpc thumbnailProviderIpc;
            thumbnailReplyData = thumbnailProviderIpc.fetchThumbnailForFile(fakeFolder.localPath() + currentImage, {});
            QMetaObject::invokeMethod(&loop, &QEventLoop::quit, Qt::QueuedConnection);
        });
        loop.exec();
        t2.detach();
        QVERIFY(thumbnailReplyData.isEmpty());
    }

    void testRequestCustomStates()
    {
        QTemporaryDir dir;
        ConfigFile::setConfDir(dir.path());

        FolderMan *folderman = FolderMan::instance();
        QVERIFY(folderman);
        auto folder = FolderMan::instance()->folderForPath(fakeFolder.localPath());
        QVERIFY(folder);

        folder->setVirtualFilesEnabled(true);

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // just add records from fake folder's journal to real one's to make test work
        SyncJournalFileRecord record;
        auto realFolder = FolderMan::instance()->folderForPath(fakeFolder.localPath());
        QVERIFY(realFolder);
        for (auto it = std::begin(dummyFileStates); it != std::end(dummyFileStates); ++it) {
            if (fakeFolder.syncJournal().getFileRecord(it.key(), &record)) {
                record._isShared = it.value()._isShared;
                if (record._isShared) {
                    record._remotePerm.setPermission(OCC::RemotePermissions::Permissions::IsShared);
                }
                record._lockstate._locked = it.value()._isLocked;
                if (record._lockstate._locked) {
                    record._lockstate._lockOwnerId = "admin@example.cloud.com";
                    record._lockstate._lockOwnerDisplayName = "Admin";
                    record._lockstate._lockOwnerType = static_cast<int>(SyncFileItem::LockOwnerType::UserLock);
                    record._lockstate._lockTime = QDateTime::currentMSecsSinceEpoch();
                    record._lockstate._lockTimeout = 1000 * 60 * 60;
                }
                QVERIFY(fakeFolder.syncJournal().setFileRecord(record));
                QVERIFY(realFolder->journalDb()->setFileRecord(record));
            }
        }

        QSignalSpy propfindRequestedSpy(this, &TestCfApiShellExtensionsIPC::propfindRequested);

        // #1 Test every file's states fetching. Everything must succeed.
        for (auto it = std::cbegin(dummyFileStates); it != std::cend(dummyFileStates); ++it) {
            QEventLoop loop;
            QVariantList customStates;
            std::thread t([&] {
                VfsShellExtensions::CustomStateProviderIpc customStateProviderIpc;
                customStates = customStateProviderIpc.fetchCustomStatesForFile(fakeFolder.localPath() + it.key());
                QMetaObject::invokeMethod(&loop, &QEventLoop::quit, Qt::QueuedConnection);
            });
            loop.exec();
            t.detach();

            if (!customStates.isEmpty()) {
                const auto lockedIndex = QString(CUSTOM_STATE_ICON_LOCKED_INDEX).toInt() - QString(CUSTOM_STATE_ICON_INDEX_OFFSET).toInt();
                const auto sharedIndex = QString(CUSTOM_STATE_ICON_SHARED_INDEX).toInt() - QString(CUSTOM_STATE_ICON_INDEX_OFFSET).toInt();

                if (customStates.contains(lockedIndex) && customStates.contains(sharedIndex)) {
                    QVERIFY(it.value()._isLocked && it.value()._isShared);
                }
                if (customStates.contains(lockedIndex)) {
                    QVERIFY(it.value()._isLocked);
                }
                if (customStates.contains(sharedIndex)) {
                    QVERIFY(it.value()._isShared);
                }
            }

            QVERIFY(!customStates.isEmpty() || (!it.value()._isLocked && !it.value()._isShared));
        }
        QVERIFY(propfindRequestedSpy.isEmpty());

        // #2 Test wrong file's states fetching. It must fail.
        QEventLoop loop;
        QVariantList customStates;
        std::thread t1([&] {
            VfsShellExtensions::CustomStateProviderIpc customStateProviderIpc;
            customStates = customStateProviderIpc.fetchCustomStatesForFile(fakeFolder.localPath() + QStringLiteral("A/files/wrong.jpg"));
            QMetaObject::invokeMethod(&loop, &QEventLoop::quit, Qt::QueuedConnection);
        });
        loop.exec();
        t1.detach();
        QVERIFY(customStates.isEmpty());
        QVERIFY(propfindRequestedSpy.isEmpty());

        // #3 Test wrong file states fetching. It must fail.
        customStates.clear();
        std::thread t2([&] {
            VfsShellExtensions::CustomStateProviderIpc customStateProviderIpc;
            customStates = customStateProviderIpc.fetchCustomStatesForFile(fakeFolder.localPath() + QStringLiteral("A/files/test_non_shared_and_non_locked_file.txt"));
            QMetaObject::invokeMethod(&loop, &QEventLoop::quit, Qt::QueuedConnection);
        });
        loop.exec();
        t2.detach();
        QVERIFY(customStates.isEmpty());
        QVERIFY(propfindRequestedSpy.isEmpty());

        // reset all share states to make sure we'll get new states when fetching
        for (auto it = std::begin(dummyFileStates); it != std::end(dummyFileStates); ++it) {
            if (fakeFolder.syncJournal().getFileRecord(it.key(), &record)) {
                record._remotePerm.unsetPermission(OCC::RemotePermissions::Permissions::IsShared);
                record._isShared = false;
                QVERIFY(fakeFolder.syncJournal().setFileRecord(record));
                QVERIFY(realFolder->journalDb()->setFileRecord(record));
            }
        }
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        //

        // wait enough time to make shares' state invalid
        QTest::qWait(shellExtensionServerOverrideIntervalMs + 1000);

        // #4 Test every file's states fetching. Everything must succeed.
        for (auto it = std::cbegin(dummyFileStates); it != std::cend(dummyFileStates); ++it) {
            QEventLoop loop;
            QVariantList customStates;
            std::thread t([&] {
                VfsShellExtensions::CustomStateProviderIpc customStateProviderIpc;
                customStates = customStateProviderIpc.fetchCustomStatesForFile(fakeFolder.localPath() + it.key());
                QMetaObject::invokeMethod(&loop, &QEventLoop::quit, Qt::QueuedConnection);
            });
            loop.exec();
            t.detach();
        }
        QEXPECT_FAIL("", "", Continue);
        QVERIFY(propfindRequestedSpy.count() == dummyFileStates.size());
    }

    void cleanupTestCase()
    {
        QTemporaryDir dir;
        ConfigFile::setConfDir(dir.path());

        VfsShellExtensions::ThumbnailProviderIpc::overrideServerName.clear();

        if (auto folder = FolderMan::instance()->folderForPath(fakeFolder.localPath())) {
            folder->setVirtualFilesEnabled(false);
        }
        FolderMan::instance()->unloadAndDeleteAllFolders();
        if (auto accountToDelete = OCC::AccountManager::instance()->accounts().first()) {
            OCC::AccountManager::instance()->deleteAccount(accountToDelete.data());
        }
    }
};

bool TestCfApiShellExtensionsIPC::replyWithNoShares = false;

QTEST_GUILESS_MAIN(TestCfApiShellExtensionsIPC)
#include "testcfapishellextensionsipc.moc"
