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
#include <QNetworkConfigurationManager>

#include "3rdparty/qtsingleapplication/qtsingleapplication.h"

#include "clientproxy.h"
#include "connectionvalidator.h"
#include "folderman.h"
#include "logbrowser.h"
#include "owncloudgui.h"
#include "platform.h"
#include "progressdispatcher.h"
#include "syncresult.h"

class QMessageBox;
class QSystemTrayIcon;
class QSocket;

namespace CrashReporter {
class Handler;
}

namespace OCC {

Q_DECLARE_LOGGING_CATEGORY(lcApplication)

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
    explicit Application(int &argc, char **argv, Platform *platform);
    ~Application() override;

    bool debugMode();

    Q_INVOKABLE void showSettingsDialog();

    ownCloudGui *gui() const;

    QString displayLanguage() const;

    AccountStatePtr addNewAccount(AccountPtr newAccount);

public slots:
    void slotCrash();
    void slotCrashEnforce();
    void slotCrashFatal();
    void slotShowGuiMessage(const QString &title, const QString &message);
    /**
     * Will download a virtual file, and open the result.
     * The argument is the filename of the virtual file (including the extension)
     */
    void openVirtualFile(const QString &filename);

    /// Attempt to show() the tray icon again. Used if no systray was available initially.
    void tryTrayAgain();

protected:
    void parseOptions(const QStringList &);
    void setupTranslations();
    void setupLogging();
    bool event(QEvent *event) override;

signals:
    void folderRemoved();
    void folderStateChanged(Folder *);

protected slots:
    void slotParseMessage(const QString &, QObject *);
    void slotCheckConnection();
    void slotUseMonoIconsChanged(bool);
    void slotCleanup();
    void slotAccountStateAdded(AccountStatePtr accountState) const;
    void slotAccountStateRemoved() const;
    void slotSystemOnlineConfigurationChanged(const QNetworkConfiguration &);

private:
    /**
     * Maybe a newer version of the client was used with this config file:
     * if so, backup, confirm with user and remove the config that can't be read.
     */
    bool configVersionMigration();

    QPointer<ownCloudGui> _gui;

    Theme *_theme;

    // options from command line:
    bool _showSettings = false;
    bool _quitInstance = false;
    QString _logFile;
    QString _logDir;
    bool _logFlush;
    bool _logDebug;
    bool _userTriggeredConnect;
    bool _debugMode;
    QString _userEnforcedLanguage;
    QString _displayLanguage;

    ClientProxy _proxy;

    QNetworkConfigurationManager _networkConfigurationManager;
    QTimer _checkConnectionTimer;

#if defined(WITH_CRASHREPORTER)
    QScopedPointer<CrashReporter::Handler> _crashHandler;
#endif
    QScopedPointer<FolderMan> _folderManager;
};

inline Application *ocApp()
{
    auto instance = qobject_cast<Application *>(qApp);
    OC_ENFORCE(instance);
    return instance;
}

} // namespace OCC

#endif // APPLICATION_H
