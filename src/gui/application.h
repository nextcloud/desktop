/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef APPLICATION_H
#define APPLICATION_H

#include "owncloudgui.h"
#include "progressdispatcher.h"
#include "clientproxy.h"
#include "folderman.h"

#include <KDSingleApplication>

#include <QApplication>
#include <QPointer>
#include <QQueue>
#include <QTimer>
#include <QElapsedTimer>
#include <QNetworkInformation>

class QMessageBox;
class QSystemTrayIcon;
class QSocket;

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
class Application : public QApplication
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

    [[nodiscard]] bool isRunning() const;

    bool sendMessage(const QString &message);

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
    void slotParseMessage(const QByteArray &msg);
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

    KDSingleApplication _singleApp;

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
    QString _setLanguage;

    QScopedPointer<FolderMan> _folderManager;
#if defined(Q_OS_WIN)
    QScopedPointer<ShellExtensionsServer> _shellExtensionsServer;
#endif
};

} // namespace OCC

#endif // APPLICATION_H
