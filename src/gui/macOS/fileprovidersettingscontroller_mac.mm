/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fileprovidersettingscontroller.h"

#include <QFileDialog>
#include <QQmlApplicationEngine>

#include "gui/systray.h"
#include "gui/userinfo.h"
#include "gui/macOS/fileprovider.h"
#include "gui/macOS/fileprovideritemmetadata.h"
#include "gui/macOS/fileproviderutils.h"

// Objective-C imports
#import <Foundation/Foundation.h>
// End of Objective-C imports

namespace {
constexpr auto fpSettingsQmlPath = "qrc:/qml/src/gui/macOS/ui/FileProviderSettings.qml";

// QML properties -- make sure they match up in QML file!
constexpr auto fpAccountUserIdAtHostProp = "accountUserIdAtHost";

// NSUserDefaults entries
constexpr auto enabledAccountsSettingsKey = "enabledAccounts";

} // namespace

namespace OCC {

namespace Mac {

Q_LOGGING_CATEGORY(lcFileProviderSettingsController, "nextcloud.gui.mac.fileprovider.settingscontroller")

class FileProviderSettingsController::MacImplementation : public QObject
{
public:
    enum class VfsAccountsAction {
        VfsAccountsNoAction,
        VfsAccountsEnabledChanged,
    };

    explicit MacImplementation(FileProviderSettingsController *const parent)
    {
        q = parent;
        initialCheck();
    };

    ~MacImplementation() override = default;

    [[nodiscard]] QStringList enabledAccounts() const
    {
        QStringList qEnabledAccounts;
        NSArray<NSString *> *const enabledAccounts = nsEnabledAccounts();

        for (NSString *const userIdAtHostString in enabledAccounts) {
            qCDebug(lcFileProviderSettingsController) << "Found VFS-enabled account in user defaults:"
                                                      << userIdAtHostString;

            qEnabledAccounts.append(QString::fromNSString(userIdAtHostString));
        }

        return qEnabledAccounts;
    }

    [[nodiscard]] bool vfsEnabledForAccount(const QString &userIdAtHost) const
    {
        NSArray<NSString *> *const vfsEnabledAccounts = nsEnabledAccounts();
        return [vfsEnabledAccounts containsObject:userIdAtHost.toNSString()];
    }

    [[nodiscard]] VfsAccountsAction setVfsEnabledForAccount(const QString &userIdAtHost, const bool setEnabled) const
    {
        NSArray<NSString *> *vfsEnabledAccounts = nsEnabledAccounts();

        qCInfo(lcFileProviderSettingsController) << "Setting file provider-based vfs of account"
                                                 << userIdAtHost
                                                 << "to"
                                                 << setEnabled;

        if (vfsEnabledAccounts == nil) {
            qCDebug(lcFileProviderSettingsController) << "Received nil array for accounts, creating new array";
            vfsEnabledAccounts = NSArray.array;
        }

        NSString *const nsUserIdAtHost = userIdAtHost.toNSString();
        const BOOL accountEnabled = [vfsEnabledAccounts containsObject:nsUserIdAtHost];

        if (accountEnabled == setEnabled) {
            qCDebug(lcFileProviderSettingsController) << "VFS enablement status for"
                                                      << userIdAtHost
                                                      << "matches config.";
            return VfsAccountsAction::VfsAccountsNoAction;
        }

        NSMutableArray<NSString *> *const mutableVfsAccounts = vfsEnabledAccounts.mutableCopy;

        if (setEnabled) {
            [mutableVfsAccounts addObject:nsUserIdAtHost];
        } else {
            [mutableVfsAccounts removeObject:nsUserIdAtHost];
        }

        NSArray<NSString *> *const modifiedVfsAccounts = mutableVfsAccounts.copy;
        [_userDefaults setObject:modifiedVfsAccounts forKey:_accountsKey];

        Q_ASSERT(vfsEnabledForAccount(userIdAtHost) == setEnabled);

        return VfsAccountsAction::VfsAccountsEnabledChanged;
    }

    [[nodiscard]] VfsAccountsAction enableVfsForAllAccounts() const
    {
        const auto accManager = AccountManager::instance();
        const auto accountsList = accManager->accounts();

        if (accountsList.count() == 0) {
            return VfsAccountsAction::VfsAccountsNoAction;
        }

        auto overallActResult = VfsAccountsAction::VfsAccountsNoAction;

        for (const auto &account : accountsList) {
            const auto qAccountUserIdAtHost = account->account()->userIdAtHostWithPort();
            const auto accountActResult = setVfsEnabledForAccount(qAccountUserIdAtHost, true);

            if (accountActResult == VfsAccountsAction::VfsAccountsEnabledChanged) {
                overallActResult = accountActResult;
            }
        }

        return overallActResult;
    }

private:
    [[nodiscard]] NSArray<NSString *> *nsEnabledAccounts() const
    {
        return (NSArray<NSString *> *)[_userDefaults objectForKey:_accountsKey];
    }

    void initialCheck()
    {
        qCInfo(lcFileProviderSettingsController) << "Running initial checks for file provider settings controller.";
        NSArray<NSString *> *const vfsEnabledAccounts = nsEnabledAccounts();

        if (vfsEnabledAccounts != nil) {
            return;
        }

        qCInfo(lcFileProviderSettingsController) << "Initial check for file provider settings found nil enabled vfs accounts array."
                                                  << "Enabling all accounts on initial setup.";

        [[maybe_unused]] const auto result = enableVfsForAllAccounts();
    }

    FileProviderSettingsController *q = nullptr;
    NSUserDefaults *_userDefaults = NSUserDefaults.standardUserDefaults;
    NSString *_accountsKey = [NSString stringWithUTF8String:enabledAccountsSettingsKey];
    QHash<QString, unsigned long long> _storageUsage;
};

FileProviderSettingsController *FileProviderSettingsController::instance()
{
    static FileProviderSettingsController controller;
    return &controller;
}

FileProviderSettingsController::FileProviderSettingsController(QObject *parent)
    : QObject{parent}
    , d(new FileProviderSettingsController::MacImplementation(this))
{
    const auto accManager = AccountManager::instance();
    const auto accountsList = accManager->accounts();

    for (const auto &accountState : accountsList) {
        const auto userInfo = new UserInfo(accountState.data(), false, false, this);
        const auto account = accountState->account();
        const auto accountUserIdAtHost = account->userIdAtHostWithPort();

        _userInfos.insert(accountUserIdAtHost, userInfo);
        userInfo->setActive(true);
    }
}

QQuickWidget *FileProviderSettingsController::settingsViewWidget(const QString &accountUserIdAtHost,
                                                                 QWidget *const parent,
                                                                 const QQuickWidget::ResizeMode resizeMode)
{
    const auto settingsViewWidget = new QQuickWidget(Systray::instance()->trayEngine(), parent);
    settingsViewWidget->setResizeMode(resizeMode);
    settingsViewWidget->setSource(QUrl(fpSettingsQmlPath));
    settingsViewWidget->rootObject()->setProperty(fpAccountUserIdAtHostProp, accountUserIdAtHost);
    return settingsViewWidget;
}

QStringList FileProviderSettingsController::vfsEnabledAccounts() const
{
    return d->enabledAccounts();
}

bool FileProviderSettingsController::vfsEnabledForAccount(const QString &userIdAtHost) const
{
    return d->vfsEnabledForAccount(userIdAtHost);
}

void FileProviderSettingsController::setVfsEnabledForAccount(const QString &userIdAtHost, const bool setEnabled, const bool showInformationDialog)
{
    const auto enabledAccountsAction = d->setVfsEnabledForAccount(userIdAtHost, setEnabled);
    if (enabledAccountsAction == MacImplementation::VfsAccountsAction::VfsAccountsEnabledChanged) {
        emit vfsEnabledAccountsChanged();

        if (setEnabled && showInformationDialog) {
            QMessageBox::information(nullptr, tr("Virtual files enabled"),
                                     tr("Virtual files have been enabled for this account.\n\n"
                                        "Your files are now accessible in Finder under the \"Locations\" section."));
        }
    }
}

bool FileProviderSettingsController::trashDeletionEnabledForAccount(const QString &userIdAtHost) const
{
    if (userIdAtHost.isEmpty()) {
        return false;
    }

    const auto xpc = FileProvider::instance()->xpc();

    if (!xpc) {
        return true;
    }

    const auto domainId = FileProviderUtils::domainIdentifierForAccountIdentifier(userIdAtHost);

    if (const auto trashDeletionState = xpc->trashDeletionEnabledStateForFileProviderDomain(domainId)) {
        return trashDeletionState->first;
    }

    return true;
}

bool FileProviderSettingsController::trashDeletionSetForAccount(const QString &userIdAtHost) const
{
    const auto xpc = FileProvider::instance()->xpc();

    if (!xpc) {
        return false;
    }

    const auto domainId = FileProviderUtils::domainIdentifierForAccountIdentifier(userIdAtHost);

    if (const auto state = xpc->trashDeletionEnabledStateForFileProviderDomain(domainId)) {
        return state->second;
    }

    return false;
}

void FileProviderSettingsController::setTrashDeletionEnabledForAccount(const QString &userIdAtHost, const bool setEnabled)
{
    const auto xpc = FileProvider::instance()->xpc();

    if (!xpc) {
        // Reset state of UI elements
        emit trashDeletionEnabledForAccountChanged(userIdAtHost);
        emit trashDeletionSetForAccountChanged(userIdAtHost);
        return;
    }

    const auto domainId = FileProviderUtils::domainIdentifierForAccountIdentifier(userIdAtHost);

    xpc->setTrashDeletionEnabledForFileProviderDomain(domainId, setEnabled);

    emit trashDeletionEnabledForAccountChanged(userIdAtHost);
    emit trashDeletionSetForAccountChanged(userIdAtHost);
}

} // namespace Mac

} // namespace OCC
