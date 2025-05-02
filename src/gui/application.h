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
#include <QElapsedTimer>
#include <QNetworkInformation>

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

Q_DECLARE_LOGGING_CATEGORY(lcApplication)

class Theme;
class Folder;
class ShellExtensionsServer;
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
    ~Application() override;

    bool giveHelp();
    void showHelp();
    void showHint(std::string errorHint);
    bool debugMode();
    [[nodiscard]] bool backgroundMode() const;
    bool versionOnly(); // only display the version?
    void showVersion();

    void showMainDialog();

    [[nodiscard]] ownCloudGui *gui() const;

    bool event(QEvent *event) override;

public slots:
    // TODO: this should not be public
    void slotownCloudWizardDone(int);
    void slotCrash();
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

signals:
    void folderRemoved();
    void folderStateChanged(OCC::Folder *);
    void isShowingSettingsDialog();
    void systemPaletteChanged();

protected slots:
    void slotParseMessage(const QString &, QObject *);
    void slotCheckConnection();
    void slotCleanup();
    void slotAccountStateAdded(OCC::AccountState *accountState);
    void slotAccountStateRemoved(OCC::AccountState *accountState);
    void slotSystemOnlineConfigurationChanged();
    void slotGuiIsShowingSettings();

private:
    void setHelp();

    void handleEditLocallyFromOptions();

    AccountManager::AccountsRestoreResult restoreLegacyAccount();
    void setupConfigFile();
    void setupAccountsAndFolders();

    /**
     * Maybe a newer version of the client was used with this config file:
     * if so, backup, confirm with user and remove the config that can't be read.
     */
    bool configVersionMigration();

    QPointer<ownCloudGui> _gui;

    Theme *_theme;

    bool _helpOnly = false;
    bool _versionOnly = false;

    QElapsedTimer _startedAt;

    // options from command line:
    bool _showLogWindow = false;
    bool _quitInstance = false;
    QString _logFile;
    QString _logDir;
    int _logExpire = 0;
    bool _logFlush = false;
    bool _logDebug = false;
    bool _userTriggeredConnect = false;
    bool _debugMode = false;
    bool _backgroundMode = false;
    QUrl _editFileLocallyUrl;

    ClientProxy _proxy;

    QTimer _checkConnectionTimer;

    QString _overrideServerUrl;
    QString _overrideLocalDir;

#if defined(WITH_CRASHREPORTER)
    QScopedPointer<CrashReporter::Handler> _crashHandler;
#endif
    QScopedPointer<FolderMan> _folderManager;
#if defined(Q_OS_WIN)
    QScopedPointer<ShellExtensionsServer> _shellExtensionsServer;
#endif
};

} // namespace OCC

#endif // APPLICATION_H
