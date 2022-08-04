/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>
#include <QImage>
#include <QPainter>
#include "syncenginetestutils.h"
#include "common/vfs.h"
#include "common/shellextensionutils.h"
#include "config.h"
#include <syncengine.h>

#include "folderman.h"
#include "account.h"
#include "accountstate.h"
#include "accountmanager.h"
#include "testhelper.h"
#include "vfs/cfapi/shellext/thumbnailprovideripc.h"
#include "shellextensionsserver.h"

using namespace OCC;

class TestCfApiShellExtensionsIPC : public QObject
{
    Q_OBJECT

    FolderMan _fm;

    FakeFolder fakeFolder{FileInfo()};

    QScopedPointer<FakeQNAM> fakeQnam;
    OCC::AccountPtr account;
    OCC::AccountState* accountState;

    QScopedPointer<ShellExtensionsServer> _shellExtensionsServer;

    QStringList dummmyImageNames = {
        "A/photos/imageJpg.jpg",
        "A/photos/imagePng.png",
        "A/photos/imagePng.bmp",
    };
    QMap<QString, QByteArray> dummyImages;

    QString currentImage;

private slots:
    void initTestCase()
    {
        VfsShellExtensions::ThumbnailProviderIpc::overrideServerName = VfsShellExtensions::serverNameForApplicationNameDefault();

        _shellExtensionsServer.reset(new ShellExtensionsServer);

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

                const auto urlQuery = QUrlQuery(req.url());
                const auto fileId = urlQuery.queryItemValue(QStringLiteral("fileId"));
                const auto x = urlQuery.queryItemValue(QStringLiteral("x")).toInt();
                const auto y = urlQuery.queryItemValue(QStringLiteral("y")).toInt();
                const auto path = req.url().path();

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
                
                return reply;
            });
    };

    void testRequestThumbnails()
    {
        FolderMan *folderman = FolderMan::instance();
        QVERIFY(folderman);
        auto folder = FolderMan::instance()->folderForPath(fakeFolder.localPath());
        QVERIFY(folder);

        folder->setVirtualFilesEnabled(true);

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        ItemCompletedSpy completeSpy(fakeFolder);

        auto cleanup = [&]() {
            completeSpy.clear();
        };
        cleanup();

        // Create a virtual file for remote files
        fakeFolder.remoteModifier().mkdir("A");
        fakeFolder.remoteModifier().mkdir("A/photos");
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

    void cleanupTestCase()
    {
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

QTEST_GUILESS_MAIN(TestCfApiShellExtensionsIPC)
#include "testcfapishellextensionsipc.moc"
