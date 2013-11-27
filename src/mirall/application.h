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
#include <QPointer>
#include <QQueue>

#include "qtsingleapplication.h"

#include "mirall/syncresult.h"
#include "mirall/logbrowser.h"
#include "mirall/owncloudgui.h"
#include "mirall/connectionvalidator.h"
#include "mirall/progressdispatcher.h"

class QMessageBox;
class QSystemTrayIcon;

namespace Mirall {
class Theme;
class Folder;
class SslErrorDialog;
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
    void slotStartUpdateDetector();
    void slotSetupProxy();
    void slotUseMonoIconsChanged( bool );
    void slotCredentialsFetched();
    void slotLogin();
    void slotLogout();
    void slotCleanup();
    void slotAccountChanged(Account *newAccount, Account *oldAccount);

private:
    void setHelp();
    void runValidator();

    QPointer<ownCloudGui> _gui;
    QPointer<SocketApi> _socketApi;

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
    bool    _userTriggeredConnect;
    QPointer<QMessageBox> _connectionMsgBox;

    friend class ownCloudGui; // for _startupNetworkError
};

} // namespace Mirall

#endif // APPLICATION_H
