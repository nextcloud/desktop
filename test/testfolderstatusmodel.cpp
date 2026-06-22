/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtTest>
#include <QStandardPaths>
#include <QAbstractItemModelTester>

#include "accountstate.h"
#include "folderman.h"
#include "folderstatusmodel.h"
#include "logger.h"
#include "syncenginetestutils.h"
#include "testhelper.h"

using namespace OCC;
using namespace OCC::Utility;

class TestFolderStatusModel : public QObject
{
    Q_OBJECT

    std::unique_ptr<FolderMan> _folderMan;

public:

private Q_SLOTS:
    void initTestCase()
    {
        Logger::instance()->setLogFlush(true);
        Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);

        _folderMan.reset(new FolderMan{});
    }

    void startModel()
    {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};

        const auto account = fakeFolder.account();
        const auto capabilities = QVariantMap {
                                              {QStringLiteral("end-to-end-encryption"), QVariantMap {
                                                                                            {QStringLiteral("enabled"), true},
                                                                                            {QStringLiteral("api-version"), QString::number(2.0)},
                                                                                            }},
                                              };
        account->setCapabilities(capabilities);
        account->setCredentials(new FakeCredentials{fakeFolder.networkAccessManager()});
        account->setUrl(QUrl(("owncloud://somehost/owncloud")));
        auto accountState = new FakeAccountState(account);
        QVERIFY(accountState->isConnected());

        auto folderDef = folderDefinition(fakeFolder.localPath());
        folderDef.targetPath = "";
        const auto folder = FolderMan::instance()->addFolder(accountState, folderDef);
        QVERIFY(folder);

        FolderStatusModel test;
        test.setAccountState(accountState);

        QSKIP("Initial test implementation is known to be broken");
        QAbstractItemModelTester modeltester(&test);

        test.fetchMore({});
    }
};

QTEST_MAIN(TestFolderStatusModel)
#include "testfolderstatusmodel.moc"
