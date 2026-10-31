/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QDir>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "account.h"
#include "configfile.h"
#include "folder.h"
#include "folderman.h"
#include "macOS/findersyncservice.h"
#include "socketapi/socketapi.h"
#include "syncenginetestutils.h"
#include "testhelper.h"

using namespace Qt::StringLiterals;
using namespace OCC;

class TestFinderSyncPathNormalization : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void testDecomposedFinderPaths()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        ConfigFile::setConfDir(dir.path());

        const auto root = QString{dir.path() + u"/geschäftlich"_s};
        const auto child = QString{root + u"/Fotos"_s};
        QVERIFY(QDir().mkpath(child));

        FolderMan::resetInstance();
        auto folderMan = FolderMan::instance();
        auto account = Account::create();
        account->setCredentials(new FakeCredentials{new FakeQNAM({})});
        account->setUrl(QUrl(u"http://example.test"_s));
        auto accountState = new FakeAccountState(account);
        const auto folder = folderMan->addFolder(accountState, folderDefinition(root));
        QVERIFY(folder);

        Mac::FinderSyncService service;
        service.setSocketApi(folderMan->socketApi());

        // NFD Finder paths must resolve inside the NFC sync root without matching a sibling.
        const auto canonicalChild = QString{folder->path() + u"Fotos"_s};
        const auto decomposedChild = canonicalChild.normalized(QString::NormalizationForm_D);
        QVERIFY(decomposedChild != canonicalChild);
        QVERIFY(service.getFileStatus(decomposedChild).first);
        QVERIFY(!service.getMenuItems({decomposedChild}).isEmpty());

        const auto outside = QString{folder->cleanPath() + u"-other/Fotos"_s};
        QVERIFY(!service.getFileStatus(outside.normalized(QString::NormalizationForm_D)).first);
        QVERIFY(service.getMenuItems({outside.normalized(QString::NormalizationForm_D)}).isEmpty());

        folderMan->unloadAndDeleteAllFolders();
    }
};

QTEST_GUILESS_MAIN(TestFinderSyncPathNormalization)
#include "testfindersyncpathnormalization.moc"
