/*
 *
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
#include <QtGlobal>

#include "accountmanager.h"
#include "common/utility.h"
#include "gui/application.h"
#include "gui/logbrowser.h"
#include "libsync/configfile.h"
#include "libsync/platform.h"
#include "libsync/theme.h"
#include "resources/loadresources.h"

#include "common/version.h"
#include "gui/translations.h"
#include "libsync/logger.h"

#include <kdsingleapplication.h>

#ifdef WITH_AUTO_UPDATER
#include "updater/updater.h"
#endif

#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
#include <QProcess>
#include <QTimer>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#include <iostream>

using namespace std::chrono_literals;

using namespace OCC;

Q_LOGGING_CATEGORY(lcMain, "gui.main", QtInfoMsg)

namespace {
inline auto msgParseOptionsC()
{
    return QStringLiteral("MSG_PARSEOPTIONS:");
}

// Helpers for displaying messages. Note that there is probably no console on Windows.
void displayHelpText(const QString &t)
{
    Logger::instance()->attacheToConsole();
    std::cout << qUtf8Printable(t) << std::endl;
#ifdef Q_OS_WIN
    // No console on Windows.
    QString spaces(80, QLatin1Char(' ')); // Add a line of non-wrapped space to make the messagebox wide enough.
    QString text =
        QStringLiteral("<qt><pre style='white-space:pre-wrap'>") + t.toHtmlEscaped() + QStringLiteral("</pre><pre>") + spaces + QStringLiteral("</pre></qt>");
    QMessageBox::information(0, Theme::instance()->appNameGUI(), text);
#endif
}

struct CommandLineOptions
{
    bool show = false;
    bool quitInstance = false;

    QString logDir;
    QString logFile;
    bool logFlush = false;
    bool logDebug = false;

    bool debugMode = false;

    QString userEnforcedLanguage;

    QString fileToOpen;
};

CommandLineOptions parseOptions(const QStringList &arguments)
{
    QCommandLineParser parser;

    QString descriptionText;
    QTextStream descriptionTextStream(&descriptionText);

    descriptionTextStream
        << QStringLiteral("%1 version %2\r\nFile synchronization desktop utility.").arg(Theme::instance()->appName(), OCC::Version::displayString())
        << Qt::endl;

    if (Theme::instance()->appName() == QLatin1String("ownCloud")) {
        descriptionTextStream
            << Qt::endl
            << Qt::endl
            << QApplication::translate("CommandLine", "For more information, see %1", "link to homepage").arg(QStringLiteral("https://www.owncloud.com"));
    }

    parser.setApplicationDescription(descriptionText);

    auto helpOption = parser.addHelpOption();
    auto versionOption = parser.addVersionOption();

    // this little snippet saves a few lines below
    auto addOption = [&parser](const QCommandLineOption &option) {
        parser.addOption(option);
        return option;
    };

    auto showSettingsLegacyOption = QCommandLineOption{{QStringLiteral("showsettings")}, QStringLiteral("Hidden legacy option")};
    showSettingsLegacyOption.setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOption(showSettingsLegacyOption);

    auto showOption = addOption({{QStringLiteral("s"), QStringLiteral("show")},
        QStringLiteral(
            "Start with the main window visible, or if it is already running, bring it to the front. By default, the client launches in the background.")});
    auto quitInstanceOption = addOption({{QStringLiteral("q"), QStringLiteral("quit")}, QStringLiteral("Quit the running instance.")});
    auto logFileOption = addOption({QStringLiteral("logfile"), QStringLiteral("Write log to file (use - to write to stdout)."), QStringLiteral("filename")});
    auto logDirOption = addOption({QStringLiteral("logdir"), QStringLiteral("Write each sync log output in a new file in folder."), QStringLiteral("name")});
    auto logFlushOption = addOption({QStringLiteral("logflush"), QStringLiteral("Flush the log file after every write.")});
    auto logDebugOption = addOption({QStringLiteral("logdebug"), QStringLiteral("Output debug-level messages in the log.")});
    auto languageOption = addOption({QStringLiteral("language"), QStringLiteral("Override UI language."), QStringLiteral("language")});
    auto listLanguagesOption = addOption({QStringLiteral("list-languages"), QStringLiteral("Lists available translations, see --language.")});
    auto confDirOption = addOption({QStringLiteral("confdir"), QStringLiteral("Use the given configuration folder."), QStringLiteral("dirname")});
    auto debugOption = addOption({QStringLiteral("debug"), QStringLiteral("Enable debug mode.")});
    addOption({QStringLiteral("cmd"), QStringLiteral("Forward all arguments to the cmd client. This argument must be the first.")});

    // virtual file system parameters (optional)
    parser.addPositionalArgument(
        QStringLiteral("vfs file"), QStringLiteral("Virtual file system file to be opened (optional)."), {QStringLiteral("[<vfs file>]")});

    parser.process(arguments);

    CommandLineOptions out;
    if (parser.isSet(showOption) || parser.isSet(showSettingsLegacyOption)) {
        out.show = true;
    }
    if (parser.isSet(quitInstanceOption)) {
        out.quitInstance = true;
    }
    if (parser.isSet(logFileOption)) {
        out.logFile = parser.value(logFileOption);
    }
    if (parser.isSet(logDirOption)) {
        if (parser.isSet(logFileOption)) {
            displayHelpText(QStringLiteral("--logfile and --logdir are mutually exclusive"));
            std::exit(1);
        }
        out.logDir = parser.value(logDirOption);
    }
    if (parser.isSet(logFlushOption)) {
        out.logFlush = true;
    }
    if (parser.isSet(logDebugOption)) {
        out.logDebug = true;
    }
    if (parser.isSet(confDirOption)) {
        const auto confDir = parser.value(confDirOption);
        if (!ConfigFile::setConfDir(confDir)) {
            displayHelpText(QStringLiteral("Invalid path passed to --confdir"));
            std::exit(1);
        }
    }
    if (parser.isSet(debugOption)) {
        out.logDebug = true;
        out.debugMode = true;
    }
    if (parser.isSet(languageOption)) {
        const auto languageValue = parser.value(languageOption);

        // fail if the language is unknown
        if (!Translations::listAvailableTranslations().contains(languageValue)) {
            displayHelpText(
                QStringLiteral("Error: unknown language \"%1\" (use --list-languages to get a complete list of supported translations)").arg(languageValue));
            std::exit(1);
        } else {
            out.userEnforcedLanguage = languageValue;
        }
    }
    if (parser.isSet(listLanguagesOption)) {
        const auto translationSet = Translations::listAvailableTranslations();
        auto availableTranslations = QStringList{translationSet.cbegin(), translationSet.cend()};
        availableTranslations.sort(Qt::CaseInsensitive);
        displayHelpText(QStringLiteral("Available translations: %1").arg(availableTranslations.join(QStringLiteral(", "))));
        std::exit(1);
    }

    auto positionalArguments = parser.positionalArguments();

    // ignore any positional arguments beyond the first one
    if (!positionalArguments.empty()) {
        out.fileToOpen = positionalArguments.front();
    }
    return out;
}

void showDowngradeDialog()
{
    QMessageBox box(QMessageBox::Warning, Theme::instance()->appNameGUI(),
        QCoreApplication::translate("version check",
            "Some settings were configured in newer versions of this client "
            "and use features that are not available in this version"));
    box.addButton(OCC::Application::tr("Quit"), QMessageBox::AcceptRole);
    box.exec();
    QTimer::singleShot(0, qApp, &QApplication::quit);
}

/**
 * Check if the last version used to write the config file differs from the current version.
 * If the current version is newer, update the config file with our current version. If the
 * current version is older, refuse to do anything: this is a downgrade, and it is too risky to
 * assume that things might work "just fine".
 */
bool checkClientVersion()
{
    ConfigFile configFile;

    // Did the client version change?
    // (The client version is adjusted further down)
    auto configVersion = QVersionNumber::fromString(configFile.clientVersionWithBuildNumberString());
    auto clientVersion = OCC::Version::versionWithBuildNumber();

    if (configVersion.majorVersion() == clientVersion.majorVersion()) {
        // no migration needed
        return true;
    }

    if (clientVersion.majorVersion() < configVersion.majorVersion()) {
        // We refuse to downgrade, too much can go wrong.
        showDowngradeDialog();
        return false;
    }

    // We're okay to continue. The settings will be updated in other parts, but here we bump the
    // version we store in the config file.
    configFile.backup();
    configFile.setClientVersionWithBuildNumberString(OCC::Version::versionWithBuildNumber().toString());
    return true;
}

void setupLogging(const CommandLineOptions &options)
{
    // might be called from second instance
    auto logger = Logger::instance();
    // call setLogFlush first, other log settings might already imply flushing
    // so setting it false in the end will have undesired results.
    logger->setLogFlush(options.logFlush);

    if (!options.logDir.isEmpty()) {
        logger->setLogDir(options.logDir);
    }
    if (!options.logFile.isEmpty()) {
        Q_ASSERT(options.logDir.isEmpty());
        logger->setLogFile(options.logFile);
    }
    logger->setLogDebug(options.logDebug);

    // Possibly configure logging from config file
    LogBrowser::setupLoggingFromConfig();

    qCInfo(lcMain) << "##################" << Theme::instance()->appName() << "locale:" << QLocale::system().name()
                   << "version:" << Theme::instance()->aboutVersions(Theme::VersionFormat::OneLiner);
    qCInfo(lcMain) << "Arguments:" << qApp->arguments();
}
}

int main(int argc, char **argv)
{
    // when called with --cmd we run the cmd client in a sub process and forward everything
    if (argc > 1 && argv[1] == QByteArrayLiteral("--cmd")) {
#ifdef Q_OS_WIN
        // On Windows ui applications don't have console access by default
        // We can't use our normal workaround to attach to the parent console as it breaks the stdin handling.
        // Therefore, we create a new console and redirect our streams.
        AllocConsole();
        freopen("CONIN$", "r", stdin);
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
#endif
        QCoreApplication cmdApp(argc, argv);
        QProcess cmd;
        cmd.setProcessChannelMode(QProcess::ForwardedChannels);
        cmd.setInputChannelMode(QProcess::ForwardedInputChannel);

        const QString app = []() -> QString {
#ifdef Q_OS_WIN
            return QCoreApplication::applicationFilePath().chopped(4) + QStringLiteral("cmd.exe");
#else
            return QCoreApplication::applicationFilePath() + QStringLiteral("cmd");
#endif
        }();
        cmd.start(app, cmdApp.arguments().mid(2));
        if (!cmd.waitForFinished(-1)) {
            std::cout << "Failed to start" << qPrintable(cmd.program()) << std::endl;
        }
#ifdef Q_OS_WIN
        // readline to keep the console window open until closed by the user
        std::string dummy;
        std::cout << "Press enter to close";
        std::getline(std::cin, dummy);
#endif
        return cmd.exitCode();
    }

    // load the resources
    const OCC::ResourcesLoader resource;

    // Create a `Platform` instance so it can set-up/tear-down stuff for us, and do any
    // initialisation that needs to be done before creating a QApplication
    const auto platform = Platform::create();

    // Create the (Q)Application instance:
    QApplication app(argc, argv);
    // TODO: Can't set this without breaking current config paths
    //    setOrganizationName(QLatin1String(APPLICATION_VENDOR));
    app.setOrganizationDomain(Theme::instance()->orgDomainName());
    app.setApplicationName(Theme::instance()->appName());
    app.setWindowIcon(Theme::instance()->applicationIcon());
    app.setApplicationVersion(Theme::instance()->versionSwitchOutput());

    KDSingleApplication singleApplication;

    if (!singleApplication.isPrimaryInstance()) {
        // if the application is already running, notify it.
        qCInfo(lcMain) << "Already running, exiting...";
        if (app.isSessionRestored()) {
            // This call is mirrored with the one in Application::slotParseMessage
            qCInfo(lcMain) << "Session was restored, don't notify app!";
            return -1;
        }

        QStringList args = app.arguments();
        if (args.size() > 1) {
            QString msg = args.join(QLatin1String("|"));
            if (!singleApplication.sendMessage((msgParseOptionsC() + msg).toUtf8()))
                return -1;
        }

        return 0;
    }

    // Check if the user upgraded or downgraded. We do this as early as possible, to detect
    // a possible downgrade.
    if (!checkClientVersion()) {
        return -1;
    }

    const auto options = parseOptions(app.arguments());

    setupLogging(options);

    platform->setApplication(&app);

    QScopedPointer<FolderMan> folderManager(new FolderMan);

    if (!AccountManager::instance()->restore()) {
        qCCritical(lcApplication) << "Could not read the account settings, quitting";
        QMessageBox::critical(nullptr, QCoreApplication::translate("account loading", "Error accessing the configuration file"),
            QCoreApplication::translate("account loading", "There was an error while accessing the configuration file at %1.").arg(ConfigFile::configFile()),
            QMessageBox::Close);
        return -1;
    }

    // Setup the folders. This includes a downgrade-detection, in which case the return value
    // is empty. Note that the value 0 (zero) is a valid return value (non-empty), in which case
    // the dialog is not shown.
    if (!FolderMan::instance()->setupFolders().has_value()) {
        // Empty return value: there was a downgrade detected on one of the databases
        showDowngradeDialog();
        return -1;
    }

    FolderMan::instance()->setSyncEnabled(true);

    auto ocApp = new OCC::Application(platform.get(), options.debugMode, &app);

    if (AccountManager::instance()->accounts().isEmpty()) {
        // display the wizard if we don't have an account yet
        QTimer::singleShot(0, ocApp->gui(), &ownCloudGui::runNewAccountWizard);
    }

    QObject::connect(platform.get(), &Platform::requestAttention, ocApp->gui(), &ownCloudGui::slotShowSettings);

    QObject::connect(&singleApplication, &KDSingleApplication::messageReceived, ocApp, [&](const QByteArray &message) {
        const QString msg = QString::fromUtf8(message);
        qCInfo(lcMain) << Q_FUNC_INFO << msg;
        if (msg.startsWith(msgParseOptionsC())) {
            const QStringList optionsStrings = msg.mid(msgParseOptionsC().size()).split(QLatin1Char('|'));
            CommandLineOptions options = parseOptions(optionsStrings);
            if (options.show) {
                ocApp->gui()->slotShowSettings();
            }
            if (options.quitInstance) {
                qApp->quit();
            }
            if (!options.fileToOpen.isEmpty()) {
                QTimer::singleShot(0, ocApp, [ocApp, fileToOpen = options.fileToOpen] { ocApp->openVirtualFile(fileToOpen); });
            }
        }
    });

    if (options.show) {
        ocApp->gui()->slotShowSettings();
    }
    if (!options.fileToOpen.isEmpty()) {
        QTimer::singleShot(0, ocApp, [ocApp, fileToOpen = options.fileToOpen] { ocApp->openVirtualFile(fileToOpen); });
    }

    platform->startServices();

#ifdef WITH_AUTO_UPDATER
    // if handleStartup returns true, main()
    // needs to terminate here, e.g. because
    // the updater is triggered
    Updater *updater = Updater::instance();
    if (updater && updater->handleStartup()) {
        return 1;
    }
#endif

    return app.exec();
}
