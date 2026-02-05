/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ACCOUNTSETTINGS_H
#define ACCOUNTSETTINGS_H

#include <QWidget>
#include <QUrl>
#include <QPointer>
#include <QHash>
#include <QTimer>

#include "folder.h"
#include "userinfo.h"
#include "progressdispatcher.h"
#include "owncloudgui.h"
#include "folderstatusmodel.h"

#ifdef BUILD_FILE_PROVIDER_MODULE
#include "macOS/fileprovidersettingscontroller.h"
#endif

class QModelIndex;
class QNetworkReply;
class QListWidgetItem;
class QLabel;
class QPushButton;
class QIcon;

namespace OCC {

namespace Ui {
    class AccountSettings;
}

class FolderMan;

class Account;
class AccountState;
class FolderStatusModel;

/**
 * @brief The AccountSettings class
 * @ingroup gui
 */
class AccountSettings : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(AccountState* accountState MEMBER _accountState)

public:
    explicit AccountSettings(AccountState *accountState, QWidget *parent = nullptr);
    ~AccountSettings() override;
    [[nodiscard]] QSize sizeHint() const override
    {
        return {
            ownCloudGui::settingsDialogSize().width(),
            QWidget::sizeHint().height()
        };
    }
    bool canEncryptOrDecrypt(const FolderStatusModel::SubFolderInfo* folderInfo);
    [[nodiscard]] OCC::AccountState *accountsState() const { return _accountState; }

signals:
    void folderChanged();
    void openFolderAlias(const QString &);
    void showIssuesList(OCC::AccountState *account);
    void requestMnemonic();
    void removeAccountFolders(OCC::AccountState *account);
    void styleChanged();

public slots:
    void slotOpenOC();
    void slotUpdateQuota(qint64 total, qint64 used);
    void slotAccountStateChanged();
    void slotStyleChanged();
    void slotHideSelectiveSyncWidget();

protected slots:
    void slotAddFolder();
    void slotEnableCurrentFolder(bool terminate = false);
    void slotScheduleCurrentFolder();
    void slotScheduleCurrentFolderForceRemoteDiscovery();
    void slotForceSyncCurrentFolder();
    void slotRemoveCurrentFolder();
    void slotOpenCurrentFolder(); // sync folder
    void slotOpenCurrentLocalSubFolder(); // selected subfolder in sync folder
    void slotEditCurrentIgnoredFiles();
    void slotOpenMakeFolderDialog();
    void slotEditCurrentLocalIgnoredFiles();
    void slotEnableVfsCurrentFolder();
    void slotDisableVfsCurrentFolder();
    void slotSetCurrentFolderAvailability(OCC::PinState state);
    void slotSetSubFolderAvailability(OCC::Folder *folder, const QString &path, OCC::PinState state);
    void slotFolderWizardAccepted();
    void slotFolderWizardRejected();
    void slotToggleSignInState();
    void refreshSelectiveSyncStatus();
    void slotMarkSubfolderEncrypted(OCC::FolderStatusModel::SubFolderInfo *folderInfo);
    void slotSubfolderContextMenuRequested(const QModelIndex& idx, const QPoint& point);
    void slotCustomContextMenuRequested(const QPoint &);
    void slotFolderListClicked(const QModelIndex &indx);
    void doExpand();
    void slotLinkActivated(const QString &link);

    // Encryption Related Stuff.
    void slotE2eEncryptionMnemonicReady();
    void slotE2eEncryptionGenerateKeys();
    void slotE2eEncryptionInitializationFinished(bool isNewMnemonicGenerated);
    void slotEncryptFolderFinished(int status);

    void slotSelectiveSyncChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight,
                                  const QVector<int> &roles);
    void slotPossiblyUnblacklistE2EeFoldersAndRestartSync();

    void slotE2eEncryptionCertificateNeedMigration();

private slots:
    void updateBlackListAndScheduleFolderSync(const QStringList &blackList, OCC::Folder *folder, const QStringList &foldersToRemoveFromBlacklist) const;
    void folderTerminateSyncAndUpdateBlackList(const QStringList &blackList, OCC::Folder *folder, const QStringList &foldersToRemoveFromBlacklist);

private slots:
    void displayMnemonic(const QString &mnemonic);
    void forgetEncryptionOnDeviceForAccount(const OCC::AccountPtr &account) const;
    void migrateCertificateForAccount(const OCC::AccountPtr &account);
    void showConnectionLabel(const QString &message, QStringList errors = QStringList());
    void openIgnoredFilesDialog(const QString & absFolderPath);
    void customizeStyle();

    void setupE2eEncryption();
    void forgetE2eEncryption();
    void checkClientSideEncryptionState();
    void removeActionFromEncryptionMessage(const QString &actionId);

private:
    bool event(QEvent *) override;
    QAction *addActionToEncryptionMessage(const QString &actionTitle, const QString &actionId);

    void setupE2eEncryptionMessage();
    void setEncryptionMessageIcon(const QIcon &icon);
    void updateEncryptionMessageActions();

    /// Returns the alias of the selected folder, empty string if none
    [[nodiscard]] QString selectedFolderAlias() const;

    Ui::AccountSettings *_ui;

    FolderStatusModel *_model;
    QUrl _OCUrl;
    bool _wasDisabledBefore = false;
    AccountState *_accountState;
    UserInfo _userInfo;
    QAction *_toggleSignInOutAction = nullptr;
    QAction *_addAccountAction = nullptr;

    bool _menuShown = false;

    QHash<QString, QMetaObject::Connection> _folderConnections;
    QHash<QAction *, QPushButton *> _encryptionMessageButtons;

    QString _spaceUsageText;
};

} // namespace OCC

#endif // ACCOUNTSETTINGS_H
