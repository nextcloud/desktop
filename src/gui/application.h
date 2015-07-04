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
#include <QTimer>

#include "qtsingleapplication.h"

#include "syncresult.h"
#include "logbrowser.h"
#include "owncloudgui.h"
#include "connectionvalidator.h"
#include "progressdispatcher.h"
#include "clientproxy.h"
#include "folderman.h"

class QMessageBox;
class QSystemTrayIcon;
class QSocket;

namespace CrashReporter {
class Handler;
}

namespace OCC {
class Theme;
class Folder;
class SslErrorDialog;

/**
 * @brief The Application class
 * @ingroup gui
 */
class Application : public SharedTools::QtSingleApplication
{
    Q_OBJECT
public:
    explicit Application(int &argc, char **argv);
    ~Application();

    bool giveHelp();
    void showHelp();
    void showHint(std::string errorHint);
    bool debugMode();

    void showSettingsDialog();

public slots:
    // TODO: this should not be public
    void slotownCloudWizardDone(int);

protected:
    void parseOptions(const QStringList& );
    void setupTranslations();
    void setupLogging();
    void enterNextLogFile();
    bool checkConfigExists(bool openSettings);

signals:
    void folderRemoved();
    void folderStateChanged(Folder*);

protected slots:
    void slotParseMessage(const QString&, QObject*);
    void slotCheckConnection();
    void slotStartUpdateDetector();
    void slotUseMonoIconsChanged( bool );
    void slotCleanup();
    void slotAccountStateAdded(AccountState *accountState);
    void slotAccountStateRemoved(AccountState *accountState);
    void slotCrash();

private:
    void setHelp();

    QPointer<ownCloudGui> _gui;

    Theme *_theme;

    bool _helpOnly;

    // options from command line:
    bool _showLogWindow;
    QString _logFile;
    QString _logDir;
    int     _logExpire;
    bool    _logFlush;
    bool    _userTriggeredConnect;
    bool    _debugMode;

    ClientProxy _proxy;

    QTimer _checkConnectionTimer;

#if defined(WITH_CRASHREPORTER)
    QScopedPointer<CrashReporter::Handler> _crashHandler;
#endif
    QScopedPointer<FolderMan> _folderManager;
};

} // namespace OCC

#endif // APPLICATION_H
