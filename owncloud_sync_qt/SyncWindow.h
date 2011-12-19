/******************************************************************************
 *    Copyright 2011 Juan Carlos Cornejo jc2@paintblack.com
 *
 *    This file is part of owncloud_sync_qt.
 *
 *    owncloud_sync is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    owncloud_sync is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with owncloud_sync.  If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/
#ifndef SYNCWINDOW_H
#define SYNCWINDOW_H

#include <QMainWindow>
#include "QWebDAV.h"
#include <QSqlDatabase>
#include <QQueue>
#include <QSystemTrayIcon>
#include <QIcon>
#include <QSet>
#include <QModelIndex>
#include <QItemSelection>

class OwnPasswordManager;
class QTimer;
class OwnCloudSync;
class QSignalMapper;
class QMenu;
class QListWidgetItem;

namespace Ui {
    class SyncWindow;
}

struct SyncIncludedFilterList {
    QString name; // No spaces, must be unique!
    QString filter;
    QString description;
    bool enabled;
    bool canBeDisabled;
    SyncIncludedFilterList(QString Name, QString Filter,
                                QString Description,
                           bool Disable = false)
    {
        name = Name;
        filter = Filter;
        description = Description;
        canBeDisabled = Disable;
    }
};

class SyncWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit SyncWindow(QWidget *parent = 0);
    ~SyncWindow();
    QSet<QString> *mSharedFilters;

private:
    Ui::SyncWindow *ui;
    QList<SyncIncludedFilterList> mIncludedFilters;
    QSet<QString> mGlobalFilters;
    QSystemTrayIcon *mSystemTray;
    QMenu *mSystemTrayMenu;
    QList<OwnCloudSync*> mAccounts;
    OwnCloudSync *mCurrentAccountEdit;
    QStringList mAccountNames;
    qint64 mTotalSyncs;
    bool mBusy;
    int mCurrentAccount;
    int mEditingConfig;
    qint64 mTotalToDownload;
    qint64 mTotalToUpload;
    qint64 mTotalToTransfer;
    qint64 mTotalTransfered;
    qint64 mTotalDownloaded;
    qint64 mTotalUploaded;
    qint64 mCurrentFileSize;
    QString mCurrentFile;
    QString mTransferState;
    bool mConflictsExist;
    QString mConfigDirectory;
    QSignalMapper *mAccountsSignalMapper;
    QQueue<OwnCloudSync*> mAccountsReadyToSync;
    bool mQuitAction;
    bool mDisplayDebug;
    bool mHideOnStart;
    bool mHideOnClose;
    qint64 mSaveLogCounter;
    qint64 mSaveDBTime;

    QIcon mDefaultIcon;
    QIcon mSyncIcon;
    QIcon mDefaultConflictIcon;
    QIcon mSyncConflictIcon;

    OwnPasswordManager *mPasswordManager;

    void processNextStep();
    void saveLogs();
    void rebuildAccountsTable();
    OwnCloudSync* addAccount(QString name);
    OwnCloudSync* getAccount(QString name);
    void accountEnabledChanged(int row);
    void editConfig(int row);
    void listFilters(int row);
    void saveApplicationSettings();
    void loadApplicationSettings();
    void deleteAccount();
    void updateSharedFilterList();
    void displayWhatsNew();
    void listGlobalFilters();
    void importGlobalFilters(bool isDefault = false);
    void exportGlobalFilters(bool isDefault = false);

public slots:
    //void timeToSync();
    void updateStatus();
    void transferProgress(qint64 current,qint64 total);
    void systemTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void closeEvent(QCloseEvent *event);

    void listFiltersSelectionChanged(QItemSelection selected,
                                     QItemSelection deselected);
    void listGlobalFiltersSelectionChanged(QItemSelection selected,
                                     QItemSelection deselected);
    void slotAccountsSignalMapper(int row);
    void processDebugMessage(const QString msg);
    void includedFilterListItemChanged(QListWidgetItem* item);
    void passwordManagerReady();

    // GUI related slots
    void on_buttonSave_clicked();
    void on_buttonSyncDir_clicked();
    void on_linePassword_textEdited(QString text);
    void on_lineHost_textEdited(QString text);
    void on_lineLocalDir_textEdited(QString text);
    void on_lineRemoteDir_textEdited(QString text);
    void on_lineUser_textEdited(QString text);
    void on_lineName_textEdited(QString text);
    void on_time_valueChanged(int value);
    void on_conflict_clicked();
    void on_buttonBox_accepted();
    void on_buttonBox_rejected();
    void on_buttonCancel_clicked();
    void on_buttonNewAccount_clicked();
    void on_lineFilter_textEdited(QString text);
    void on_buttonFilterRemove_clicked();
    void on_buttonFilterInsert_clicked();


    // Owncloud related signals
    void slotToLog(QString text);
    void slotToStatus(QString text);
    void slotConflictExists(OwnCloudSync*);
    void slotConflictResolved(OwnCloudSync*);
    void slotProgressFile(qint64 value);
    void slotProgressTotal(qint64 value);
    void slotReadyToSync(OwnCloudSync*);
    void slotFinishedSync(OwnCloudSync*);
    void slotToMessage(QString caption, QString body,
    QSystemTrayIcon::MessageIcon icon);

private slots:
    void on_action_Quit_triggered();
    void on_actionEnable_Delete_Account_triggered();
    void on_buttonDeleteAccount_clicked();
    void on_pushButton_clicked();
    void on_pushButton_2_clicked();
    void on_checkBoxHostnameEncryption_stateChanged(int arg1);
    void on_actionDisplay_Debug_Messages_toggled(bool arg1);
    void on_buttonResume_clicked();
    void on_buttonPause_clicked();
    void on_actionConfigure_triggered();
    void on_buttonEnableAllIncludedFilters_clicked();
    void on_buttonDisableAllIncludedFilters_clicked();
    void on_buttonReturn_clicked();
    void on_lineGlobalFilter_textEdited(const QString &arg1);
    void on_buttonGlobalFilterInsert_clicked();
    void on_buttonGlobalFilterRemove_clicked();
    void on_buttonImport_clicked();
    void on_buttonExport_clicked();
    void on_configurationBox_accepted();
    void on_configurationBox_rejected();
    void on_checkBoxHostnameEncryption_clicked();
};

// Now create a global filter list
inline QList<SyncIncludedFilterList> g_GetIncludedFilterList()
{
    QList<SyncIncludedFilterList> list;
    // VIM
    list.append(SyncIncludedFilterList("vim_tmp",".*.swp","Vim temporary file",true));
    list.append(SyncIncludedFilterList("vim_tmp2",".*.swo","Vim temporary binary file",true));

    // LibreOffice/OpenOffice lock files
    list.append(SyncIncludedFilterList("libreoffice_lock",".~lock.*#","LibreOffice/OpenOffice lock files",true));

    // Kate swap files
    list.append(SyncIncludedFilterList("kate_tmp","*.kate-swp", "Kate temporary file", true));

    // General temporary files that I know of
    list.append(SyncIncludedFilterList("tmp1","~*","General Temporary Files",true));
    list.append(SyncIncludedFilterList("android_syncfolders","*.tacit.fs.part","Android Folder Sync temporary file",true));

    // Internal Files (User cannot disable!!)
    list.append(SyncIncludedFilterList("ocs_conflict","_ocs_serverconflict.*","Internal Conflict Resolution File",false));
        list.append(SyncIncludedFilterList("ocs_uploading","_ocs_uploading.*","Internal Uploading File",false));
            list.append(SyncIncludedFilterList("ocs_download","_ocs_downloading.*","Internal Downloading File",false));
    return list;
}

#endif // SYNCWINDOW_H
