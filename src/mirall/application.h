/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef APPLICATION_H
#define APPLICATION_H

#include <QApplication>
#include <QNetworkReply>
#include <QSslError>
#include <QPointer>
#include <QQueue>

#include "qtsingleapplication.h"

#include "mirall/syncresult.h"
#include "mirall/logbrowser.h"
#include "mirall/systray.h"
#include "mirall/connectionvalidator.h"
#include "mirall/progressdispatcher.h"

class QAction;
class QMenu;
class QSystemTrayIcon;
class QNetworkConfigurationManager;
class QSignalMapper;
class QNetworkReply;

namespace Mirall {
class Theme;
class Folder;
class FolderWatcher;
class FolderWizard;
class ownCloudInfo;
class SslErrorDialog;
class SettingsDialog;
class ItemProgressDialog;

class Application : public SharedTools::QtSingleApplication
{
    Q_OBJECT
public:
    explicit Application(int &argc, char **argv);
    ~Application();

    bool giveHelp();
    void showHelp();

public slots:
    // TODO: this should not be public
    void slotownCloudWizardDone(int);

protected:
    void parseOptions(const QStringList& );
    void setupTranslations();
    void setupActions();
    void setupSystemTray();
    void setupContextMenu();
    void setupLogBrowser();
    void enterNextLogFile();

    //folders have to be disabled while making config changes
    void computeOverallSyncStatus();

    // reimplemented
#if defined(Q_WS_WIN)
    bool winEventFilter( MSG * message, long * result );
#endif

signals:
    void folderRemoved();
    void folderStateChanged(Folder*);

protected slots:
    void slotFoldersChanged();
    void slotCheckConfig();
    void slotSettings();
    void slotItemProgressDialog();
    void slotParseOptions( const QString& );
    void slotShowTrayMessage(const QString&, const QString&);
    void slotShowOptionalTrayMessage(const QString&, const QString&);
    void slotCheckConnection();
    void slotConnectionValidatorResult(ConnectionValidator::Status);
    void slotSyncStateChange( const QString& );
    void slotTrayClicked( QSystemTrayIcon::ActivationReason );
    void slotFolderOpenAction(const QString & );
    void slotOpenOwnCloud();
    void slotOpenLogBrowser();
    void slotSSLFailed( QNetworkReply *reply, QList<QSslError> errors );
    void slotStartUpdateDetector();
    void slotSetupProxy();
    void slotRefreshQuotaDisplay( qint64 total, qint64 used );
    void slotUseMonoIconsChanged( bool );
    void slotUpdateProgress(const QString&, const Progress::Info&);
    void slotProgressSyncProblem(const QString& folder, const Progress::SyncProblem &problem);
    void slotDisplayIdle();
    void slotHelp();
private:
    void setHelp();
    void raiseDialog( QWidget* );
    void rebuildRecentMenus();

    Systray *_tray;
    QAction *_actionOpenoC;
    QAction *_actionSettings;
    QAction *_actionQuota;
    QAction *_actionStatus;
    QAction *_actionRecent;
    QAction *_actionHelp;
    QAction *_actionQuit;

    QNetworkConfigurationManager *_networkMgr;

    QPointer<FolderWizard> _folderWizard;
    SslErrorDialog *_sslErrorDialog;
    ConnectionValidator *_conValidator;

    // tray's menu
    QMenu *_contextMenu;
    QMenu *_recentActionsMenu;

    Theme *_theme;
    QSignalMapper *_folderOpenActionMapper;
    LogBrowser *_logBrowser;
    QPointer<SettingsDialog> _settingsDialog;
    QPointer<ItemProgressDialog> _progressDialog;

    QString _logFile;
    QString _logDirectory;

    int _logExpire;
    bool _showLogWindow;
    bool _logFlush;
    bool _helpOnly;
};

} // namespace Mirall

#endif // APPLICATION_H
