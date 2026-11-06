/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "account.h"
#include "accountmanager.h"
#include "activitylistmodeltestutils.h"
#include "configfile.h"
#include "foldermantestutils.h"
#include "syncenginetestutils.h"
#include "systray.h"
#include "tray/usermodel.h"

#include <QApplication>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace OCC;
using namespace Qt::StringLiterals;

class TestUserModel : public QObject
{
    Q_OBJECT

    QTemporaryDir _configDir;
    FolderManTestHelper _folderManHelper;
    AccountState *_accountState = nullptr;

private Q_SLOTS:
    void initTestCase()
    {
        Q_INIT_RESOURCE(resources);
        Q_INIT_RESOURCE(theme);
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(_configDir.isValid());
        ConfigFile::setConfDir(_configDir.path());
        Systray::instance()->create();
        auto account = Account::create();
        account->setUrl(QUrl(u"https://cloud.example.com"_s));
        account->setDavUser(u"alice"_s);
        account->setCapabilities({{u"activity"_s, QVariantMap{}}});
        const auto network = new FakeQNAM({});
        network->setOverride([network](QNetworkAccessManager::Operation operation, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            return new FakeErrorReply(operation, request, network, 404);
        });
        account->setCredentials(new FakeCredentials(network));
        _accountState = AccountManager::instance()->addAccount(account);
        QVERIFY(_accountState);
        Q_EMIT AccountManager::instance()->accountListInitialized();
        QCOMPARE(UserModel::instance()->rowCount(), 1);
    }

    void cleanupTestCase()
    {
        AccountManager::instance()->removeAccountState(_accountState);
        _accountState = nullptr;
    }

    void ignoredFileItemDoesNotRaiseAccountAlert()
    {
        auto user = UserModel::instance()->findUserForAccount(_accountState);
        QVERIFY(user);
        const auto modelIndex = UserModel::instance()->index(0);
        QVERIFY(UserModel::instance()->data(modelIndex, UserModel::AccountAlertRole).toMap().isEmpty());

        user->getActivityModel()->addSyncFileItemToActivityList(ActivityListModelTestUtils::exampleFileIgnoredActivity(u"alice"_s));
        QVERIFY(!user->getActivityModel()->activityList().isEmpty());
        QVERIFY(UserModel::instance()->data(modelIndex, UserModel::AccountAlertRole).toMap().isEmpty());

        auto softErrorActivity = Activity{};
        softErrorActivity._id = 99;
        softErrorActivity._type = Activity::SyncFileItemType;
        softErrorActivity._syncFileItemStatus = SyncFileItem::SoftError;
        softErrorActivity._dateTime = QDateTime::currentDateTime();
        softErrorActivity._subject = u"Sample soft error"_s;
        softErrorActivity._accName = u"alice"_s;
        softErrorActivity._folder = u"thingy"_s;
        user->getActivityModel()->addSyncFileItemToActivityList(softErrorActivity);

        const auto alert = UserModel::instance()->data(modelIndex, UserModel::AccountAlertRole).toMap();
        QCOMPARE(alert.value(QStringLiteral("title")).toString(), QCoreApplication::translate("OCC::SyncStatusSummary", "Some files could not be synced!"));
    }
};

QTEST_MAIN(TestUserModel)
#include "testusermodel.moc"
