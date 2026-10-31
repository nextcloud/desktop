/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "syncenginetestutils.h"
#include "testhelper.h"

#include "../src/gui/invalidfilenamedialog.h"
#include "common/vfs.h"
#include "folder.h"

#include <QNetworkRequest>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace OCC;

class TestInvalidFilenameDialog : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void permissionCheckUsesSeparatorForFileAtSyncRoot()
    {
        QTemporaryDir localDir;
        QVERIFY(localDir.isValid());

        auto fakeQnam = std::make_unique<FakeQNAM>(FileInfo{});
        auto account = Account::create();
        account->setCredentials(new FakeCredentials{fakeQnam.get()});
        account->setUrl(QUrl(QStringLiteral("http://example.com/owncloud")));
        const auto accountState = AccountStatePtr(new FakeAccountState(account));

        FolderDefinition definition;
        definition.localPath = localDir.path();
        definition.journalPath = QStringLiteral("sync.db");
        definition.targetPath = QStringLiteral("/_NcBug");
        definition.alias = QStringLiteral("_NcBug");
        Folder folder(definition, accountState.data(), createVfsFromPlugin(Vfs::Off));

        QUrl requestedUrl;
        auto *fakeQnamPointer = fakeQnam.get();
        fakeQnam->setOverride([&requestedUrl, fakeQnamPointer](const QNetworkAccessManager::Operation operation, const QNetworkRequest &request, QIODevice *) {
            if (operation == QNetworkAccessManager::CustomOperation
                && request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("PROPFIND")) {
                requestedUrl = request.url();
                return static_cast<QNetworkReply *>(new FakePropfindReply(fakeQnamPointer->currentRemoteState(), operation, request, fakeQnamPointer));
            }
            return static_cast<QNetworkReply *>(nullptr);
        });

        {
            const auto invalidFilePath = folder.path() + QStringLiteral("trailing-period.");
            InvalidFilenameDialog dialog(account,
                                         &folder,
                                         invalidFilePath,
                                         InvalidFilenameDialog::FileLocation::Default,
                                         InvalidFilenameDialog::InvalidMode::SystemInvalid);

            QTRY_VERIFY(requestedUrl.isValid());

            const auto expectedPath = Utility::concatUrlPath(account->davUrl(), QStringLiteral("/_NcBug/trailing-period.")).path();
            QCOMPARE(requestedUrl.path(), expectedPath);
        }

        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
        fakeQnam->setOverride({});
    }
};

QTEST_MAIN(TestInvalidFilenameDialog)
#include "testinvalidfilenamedialog.moc"
