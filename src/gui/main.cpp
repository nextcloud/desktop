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
#include "common/restartmanager.h"
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
#include "socketapi/socketapi.h"

#include <kdsingleapplication.h>

#ifdef WITH_AUTO_UPDATER
#include "updater/updater.h"
#endif

#include <QApplication>
#include <QCommandLineParser>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QNetworkInformation>
#include <QProcess>
#include <QTimer>
#include <QTranslator>
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
    QMessageBox::information(nullptr, Theme::instance()->appNameGUI(), text);
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

    QString fileToOpen;
};

CommandLineOptions parseOptions(const QStringList &arguments)
{
    QCommandLineParser parser;

    QString descriptionText;
    QTextStream descriptionTextStream(&descriptionText);

    descriptionTextStream << QApplication::translate("CommandLine", "%1 version %2\r\nFile synchronization desktop utility.")
                                 .arg(Theme::instance()->appName(), OCC::Version::displayString())
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
        QApplication::translate("CommandLine",
            "Start with the main window visible, or if it is already running, bring it to the front. By default, the client launches in the background.")});
    auto quitInstanceOption = addOption({{QStringLiteral("q"), QStringLiteral("quit")}, QApplication::translate("CommandLine", "Quit the running instance.")});
    auto logFileOption = addOption(
        {QStringLiteral("logfile"), QApplication::translate("CommandLine", "Write log to file (use - to write to stdout)."), QStringLiteral("filename")});
    auto logDirOption = addOption(
        {QStringLiteral("logdir"), QApplication::translate("CommandLine", "Write each sync log output in a new file in folder."), QStringLiteral("name")});
    auto logFlushOption = addOption({QStringLiteral("logflush"), QApplication::translate("CommandLine", "Flush the log file after every write.")});
    auto logDebugOption = addOption({QStringLiteral("logdebug"), QApplication::translate("CommandLine", "Output debug-level messages in the log.")});
    auto debugOption = addOption({QStringLiteral("debug"), QApplication::translate("CommandLine", "Enable debug mode.")});
    addOption({QStringLiteral("cmd"), QApplication::translate("CommandLine", "Forward all arguments to the cmd client. This argument must be the first.")});

    // virtual file system parameters (optional)
    parser.addPositionalArgument(QStringLiteral("vfs file"), QApplication::translate("CommandLine", "Virtual file system file to be opened (optional)."),
        {QStringLiteral("[<vfs file>]")});

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
            displayHelpText(QApplication::translate("CommandLine", "--logfile and --logdir are mutually exclusive"));
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
    if (parser.isSet(debugOption)) {
        out.logDebug = true;
        out.debugMode = true;
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

    if (configVersion == clientVersion) {
        // no config backup needed
        return true;
    }

    // We allow downgrades as long as the major version stays the same.
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

void loadQNetworkInformationBackend()
{
    if (!QNetworkInformation::loadDefaultBackend()) {
        qCWarning(lcMain) << "Failed to load default backend of QNetworkInformation.";
        if (!QNetworkInformation::loadBackendByFeatures(QNetworkInformation::Feature::Metered)) {
            qCWarning(lcMain) << "Failed to load backend of QNetworkInformation by metered feature.";
            if (!QNetworkInformation::loadBackendByFeatures(QNetworkInformation::Feature::Reachability)) {
                qCWarning(lcMain) << "Failed to load backend of QNetworkInformation by reachability feature.";
                qCWarning(lcMain) << "Available backends:" << QNetworkInformation::availableBackends().join(QStringLiteral(", "));
                return;
            }
        }
    }
    qCDebug(lcMain) << "Loaded network information backend:" << QNetworkInformation::instance()->backendName();
    qCDebug(lcMain) << "Supported features:" << QNetworkInformation::instance()->supportedFeatures();
    qCDebug(lcMain) << "Available backends:" << QNetworkInformation::availableBackends().join(QStringLiteral(", "));
    if (auto qni = QNetworkInformation::instance()) {
        QObject::connect(qni, &QNetworkInformation::reachabilityChanged,
            [](QNetworkInformation::Reachability reachability) { qCInfo(lcMain) << "Connection Status changed to:" << reachability; });
    }
}

QString setupTranslations(QApplication *app)
{
    const auto trPath = Translations::translationsDirectoryPath();
    qCDebug(lcMain) << "Translations directory path:" << trPath;

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QStringList uiLanguages = QLocale::system().uiLanguages(QLocale::TagSeparator::Underscore);
#else
    QStringList uiLanguages;
    for (auto lang : QLocale::system().uiLanguages()) {
        uiLanguages << lang.replace(QLatin1Char('-'), QLatin1Char('_'));
    }
#endif
    qCDebug(lcMain) << "UI languages:" << uiLanguages;

    // the user can also set a locale in the settings, so we need to load the config file
    const ConfigFile cfg;

    // we need to track the enforced language separately, since we need to distinguish between locale-provided
    // and user-enforced one below
    const QString enforcedLocale = cfg.uiLanguage();
    qCDebug(lcMain) << "Enforced language:" << enforcedLocale;

    // note that user-enforced language are prioritized over the theme enforced one
    // to make testing easier.
    if (!enforcedLocale.isEmpty()) {
        uiLanguages.prepend(enforcedLocale);
    }

    QString displayLanguage;

    auto substLang = [](const QString &lang) {
        // Map the more appropriate script codes
        // to country codes as used by Qt and
        // transifex translation conventions.

        if (lang == QLatin1String("zh_Hans")) {
            // Simplified Chinese
            return QStringLiteral("zh_CN");
        } else if (lang == QLatin1String("zh_Hant")) {
            // Traditional Chinese
            return QStringLiteral("zh_TW");
        }

        return lang;
    };

    for (QString lang : qAsConst(uiLanguages)) {
        lang = substLang(lang);
        const QString trFile = Translations::translationsFilePrefix() + lang;
        if (QTranslator *translator = new QTranslator(app); translator->load(trFile, trPath) || lang.startsWith(QLatin1String("en"))) {
            // Permissive approach: Qt and keychain translations
            // may be missing, but Qt translations must be there in order
            // for us to accept the language. Otherwise, we try with the next.
            // "en" is an exception as it is the default language and may not
            // have a translation file provided.
            qCInfo(lcMain) << "Using" << lang << "translation" << translator->language();
            displayLanguage = lang;

            const QString qtTrPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
            qCDebug(lcMain) << "qtTrPath:" << qtTrPath;
            const QString qtTrFile = QLatin1String("qt_") + lang;
            qCDebug(lcMain) << "qtTrFile:" << qtTrFile;
            const QString qtBaseTrFile = QLatin1String("qtbase_") + lang;
            qCDebug(lcMain) << "qtBaseTrFile:" << qtBaseTrFile;

            QTranslator *qtTranslator = new QTranslator(app);
            QTranslator *qtkeychainTranslator = new QTranslator(app);

            if (!qtTranslator->load(qtTrFile, qtTrPath)) {
                if (!qtTranslator->load(qtTrFile, trPath)) {
                    if (!qtTranslator->load(qtBaseTrFile, qtTrPath)) {
                        if (!qtTranslator->load(qtBaseTrFile, trPath)) {
                            qCCritical(lcMain) << "Could not load Qt translations";
                        }
                    }
                }
            }

            const QString qtkeychainTrFile = QLatin1String("qtkeychain_") + lang;
            if (!qtkeychainTranslator->load(qtkeychainTrFile, qtTrPath)) {
                if (!qtkeychainTranslator->load(qtkeychainTrFile, trPath)) {
                    qCCritical(lcMain) << "Could not load qtkeychain translations";
                }
            }

            if (!translator->isEmpty() && !qApp->installTranslator(translator)) {
                qCCritical(lcMain) << "Failed to install translator";
                translator->deleteLater();
            }
            if (!qtTranslator->isEmpty() && !qApp->installTranslator(qtTranslator)) {
                qCCritical(lcMain) << "Failed to install Qt translator";
                qtTranslator->deleteLater();
            }
            if (!qtkeychainTranslator->isEmpty() && !qApp->installTranslator(qtkeychainTranslator)) {
                qCCritical(lcMain) << "Failed to install qtkeychain translator";
                qtkeychainTranslator->deleteLater();
            }

            // makes sure widgets with locale-dependent formatting, e.g., QDateEdit, display the correct formatting
            // if the language is provided by the system locale anyway (i.e., coming from QLocale::system().uiLanguages()), we should
            // not mess with the system locale, though
            // if we did, we would enforce a locale for no apparent reason
            // see https://github.com/owncloud/client/issues/8608 for more information
            if (enforcedLocale == lang) {
                QLocale newLocale(lang);
                qCDebug(lcMain) << "language" << lang << "was enforced, changing default locale to" << newLocale;
                QLocale::setDefault(newLocale);
            }
            break;
        } else {
            translator->deleteLater();
        }
    }

    return displayLanguage;
}
} // Anonymous namespace

int main(int argc, char **argv)
{
    return RestartManager([](int argc, char **argv) {
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

        // Load the translations before option parsing, so we can localize help text and error messages.
        const QString displayLanguage = setupTranslations(&app);

        // parse the arguments before we handle singleApplication
        // errors and help/version need to be handled in this instance
        const auto options = parseOptions(app.arguments());

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

        setupLogging(options);
        loadQNetworkInformationBackend();

        platform->setApplication(&app);

        auto folderManager = FolderMan::createInstance();

        if (!AccountManager::instance()->restore()) {
            qCCritical(lcMain) << "Could not read the account settings, quitting";
            QMessageBox::critical(nullptr, QCoreApplication::translate("account loading", "Error accessing the configuration file"),
                QCoreApplication::translate("account loading", "There was an error while accessing the configuration file at %1.")
                    .arg(ConfigFile::configFile()),
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

        folderManager->setSyncEnabled(true);

        auto ocApp = Application::createInstance(platform.get(), displayLanguage, options.debugMode);

        QObject::connect(platform.get(), &Platform::requestAttention, ocApp->gui(), &ownCloudGui::slotShowSettings);

        QObject::connect(&singleApplication, &KDSingleApplication::messageReceived, ocApp.get(), [&](const QByteArray &message) {
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
                    QTimer::singleShot(0, ocApp.get(), [ocApp = ocApp.get(), fileToOpen = options.fileToOpen] { ocApp->openVirtualFile(fileToOpen); });
                }
            }
        });

        platform->startServices();

        // Handle user requests from the command-line first, before checking for updates. Because, if
        // the user explicitly requested an action, then quiting because of an update will not be
        // appreciated.
        if (options.show) {
            ocApp->gui()->slotShowSettings();
            // The user explicitly requested the settings dialog, so don't start the new-account wizard.
        } else if (!options.fileToOpen.isEmpty() && !AccountManager::instance()->accounts().isEmpty()) {
            // Only try to open a file when accounts have been configured.
            QTimer::singleShot(0, ocApp.get(), [ocApp = ocApp.get(), fileToOpen = options.fileToOpen] { ocApp->openVirtualFile(fileToOpen); });
        } else {
            // No user-requested action, check for updates.
#ifdef WITH_AUTO_UPDATER
            // if handleStartup returns true, main()
            // needs to terminate here, e.g. because
            // the updater is triggered
            Updater *updater = Updater::instance();
            if (updater && updater->handleStartup()) {
                return 1;
            }
#endif
        }

        // Display the wizard if we don't have an account yet, and no other UI is showing.
        if (AccountManager::instance()->accounts().isEmpty()) {
            QTimer::singleShot(0, ocApp->gui(), &ownCloudGui::runNewAccountWizard);
        }

        // Now that everything is up and running, start accepting connections/requests from the shell integration.
        folderManager->socketApi()->startShellIntegration();

        return app.exec();
    }).exec(argc, argv);
}
