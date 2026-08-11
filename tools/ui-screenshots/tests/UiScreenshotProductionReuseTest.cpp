/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 */

#include "account.h"
#include "accountsettings.h"
#include "accountstate.h"
#include "advancedsettings.h"
#include "configfile.h"
#include "creds/dummycredentials.h"
#include "foldermantestutils.h"
#include "generalsettings.h"
#include "ignorelisteditor.h"
#include "infosettings.h"
#include "settingsdialog.h"
#include "userstatusselectormodel.h"
#include "uiscreenshots/nativescreenshotcaptureutils.h"
#include "uiscreenshots/screenshotsyncstatussummary.h"
#include "uiscreenshots/screenshotuserstatusselectormodel.h"
#include "uiscreenshots/screenshotwizardcontroller.h"
#include "wizard/accountwizardcontroller.h"

#include <QDialog>
#include <QMetaObject>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <memory>

using namespace OCC;

class UiScreenshotProductionReuseTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        Q_INIT_RESOURCE(resources);
        Q_INIT_RESOURCE(theme);
        qputenv("NEXTCLOUD_UI_SCREENSHOTS", QByteArrayLiteral("native"));
        _profile = std::make_unique<QTemporaryDir>();
        QVERIFY(_profile->isValid());
        QVERIFY(ConfigFile::setConfDir(_profile->path()));
        _folderManHelper = std::make_unique<FolderManTestHelper>();
    }

    void cleanupTestCase()
    {
        _folderManHelper.reset();
        _profile.reset();
        qunsetenv("NEXTCLOUD_UI_SCREENSHOTS");
    }

    void opensChildDialogsThroughProductionSettingsSlots()
    {
        auto settingsDialog = std::make_unique<SettingsDialog>(nullptr);
        const auto account = Account::create();
        account->setUrl(QUrl(QStringLiteral("https://cloud.example.com")));
        account->setDavUser(QStringLiteral("alex"));
        account->setDavDisplayName(QStringLiteral("Alex Morgan"));
        auto *credentials = new DummyCredentials;
        credentials->_user = QStringLiteral("alex");
        account->setCredentials(credentials);

        auto accountState = AccountState(account);
        accountState.signOutByUi();
        QVERIFY(QMetaObject::invokeMethod(settingsDialog.get(),
            "accountAdded",
            Qt::DirectConnection,
            Q_ARG(OCC::AccountState*, &accountState)));

        auto error = QString{};
        QVERIFY2(UiScreenshots::selectSettingsPage(
                     settingsDialog.get(), UiScreenshots::SettingsPage::User, &error),
            qPrintable(error));
        QVERIFY(qobject_cast<AccountSettings *>(settingsDialog->currentPage()));
        QVERIFY2(UiScreenshots::selectSettingsPage(
                     settingsDialog.get(), UiScreenshots::SettingsPage::General, &error),
            qPrintable(error));
        QVERIFY(qobject_cast<GeneralSettings *>(settingsDialog->currentPage()));
        QVERIFY2(UiScreenshots::selectSettingsPage(
                     settingsDialog.get(), UiScreenshots::SettingsPage::Advanced, &error),
            qPrintable(error));
        QVERIFY(qobject_cast<AdvancedSettings *>(settingsDialog->currentPage()));
        QVERIFY2(UiScreenshots::selectSettingsPage(
                     settingsDialog.get(), UiScreenshots::SettingsPage::Info, &error),
            qPrintable(error));
        QVERIFY(qobject_cast<InfoSettings *>(settingsDialog->currentPage()));

        auto *networkDialog = UiScreenshots::openNetworkSettingsDialog(settingsDialog.get(), &error);
        QVERIFY2(networkDialog, qPrintable(error));
        QVERIFY(qobject_cast<AccountSettings *>(networkDialog->parentWidget()));
        delete networkDialog;

        auto *ignoreEditor = UiScreenshots::openIgnoreListEditor(settingsDialog.get(), &error);
        QVERIFY2(ignoreEditor, qPrintable(error));
        QVERIFY(qobject_cast<AdvancedSettings *>(ignoreEditor->parentWidget()));
        delete ignoreEditor;
        settingsDialog.reset();
    }

    void usesProductionOwnedVisibleWording()
    {
        const auto wizardController = ScreenshotWizardController{};
        const auto productionWizardController = AccountWizardController{};
        QCOMPARE_EQ(wizardController.property("serverDisplayName").toString(), QStringLiteral("cloud.example.com"));
        QCOMPARE_EQ(wizardController.property("syncEverythingDescription").toString(),
            productionWizardController.syncEverythingDescription());

        const auto syncStatus = ScreenshotSyncStatusSummary{};
        QVERIFY(syncStatus.syncStatusDetailString().isEmpty());

        const auto screenshotStatus = ScreenshotUserStatusSelectorModel{};
        const auto productionStatus = UserStatusSelectorModel{};
        QCOMPARE_EQ(screenshotStatus.clearStageTypes(), productionStatus.clearStageTypes());
    }

private:
    std::unique_ptr<QTemporaryDir> _profile;
    std::unique_ptr<FolderManTestHelper> _folderManHelper;
};

QTEST_MAIN(UiScreenshotProductionReuseTest)

#include "UiScreenshotProductionReuseTest.moc"
