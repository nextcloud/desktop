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
#include "mirall/owncloudgui.h"
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
class SslErrorDialog;
class SettingsDialog;
class SocketApi;

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
    void setupContextMenu();
    void setupLogging();
    void enterNextLogFile();
    bool checkConfigExists(bool openSettings);

    // reimplemented
#if defined(Q_WS_WIN)
    bool winEventFilter( MSG * message, long * result );
#endif

signals:
    void folderRemoved();
    void folderStateChanged(Folder*);

protected slots:
    void slotParseOptions( const QString& );
    void slotCheckConnection();
    void slotConnectionValidatorResult(ConnectionValidator::Status);
    void slotSSLFailed( QNetworkReply *reply, QList<QSslError> errors );
    void slotStartUpdateDetector();
    void slotSetupProxy();
    void slotUseMonoIconsChanged( bool );
    void slotCredentialsFetched();
    void slotCleanup();
private:
    void setHelp();
    void runValidator();

    QPointer<ownCloudGui> _gui;
    QPointer<SocketApi> _socketApi;
    // QNetworkConfigurationManager *_networkMgr;

    SslErrorDialog      *_sslErrorDialog;
    ConnectionValidator *_conValidator;

    Theme *_theme;

    bool _helpOnly;
    bool _startupNetworkError;

    // options from command line:
    bool _showLogWindow;
    QString _logFile;
    QString _logDir;
    int     _logExpire;
    bool    _logFlush;

    friend class ownCloudGui; // for _startupNetworkError
};

} // namespace Mirall

#endif // APPLICATION_H
