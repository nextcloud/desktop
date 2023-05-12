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
#include <QTimer>

#include <iostream>

using namespace std::chrono_literals;

using namespace OCC;

Q_LOGGING_CATEGORY(lcMain, "gui.main", QtInfoMsg)

namespace {
inline auto msgParseOptionsC()
{
    return QStringLiteral("MSG_PARSEOPTIONS:");
}

void warnSystray()
{
    QMessageBox::critical(nullptr, qApp->translate("main.cpp", "System Tray not available"),
        qApp->translate("main.cpp",
                "%1 requires on a working system tray. "
                "If you are running XFCE, please follow "
                "<a href=\"http://docs.xfce.org/xfce/xfce4-panel/systray\">these instructions</a>. "
                "Otherwise, please install a system tray application such as 'trayer' and try again.")
            .arg(Theme::instance()->appNameGUI()));
}

// Helpers for displaying messages. Note that there is probably no console on Windows.
void displayHelpText(const QString &t, std::ostream &stream = std::cout)
{
    Logger::instance()->attacheToConsole();
    stream << qUtf8Printable(t) << std::endl;
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
    bool showSettings = false;
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

    auto showSettingsOption = addOption({{QStringLiteral("s"), QStringLiteral("showsettings")}, QStringLiteral("Show the settings dialog while starting.")});
    auto quitInstanceOption = addOption({{QStringLiteral("q"), QStringLiteral("quit")}, QStringLiteral("Quit the running instance.")});
    auto logFileOption = addOption({QStringLiteral("logfile"), QStringLiteral("Write log to file (use - to write to stdout)."), QStringLiteral("filename")});
    auto logDirOption = addOption({QStringLiteral("logdir"), QStringLiteral("Write each sync log output in a new file in folder."), QStringLiteral("name")});
    auto logFlushOption = addOption({QStringLiteral("logflush"), QStringLiteral("Flush the log file after every write.")});
    auto logDebugOption = addOption({QStringLiteral("logdebug"), QStringLiteral("Output debug-level messages in the log.")});
    auto languageOption = addOption({QStringLiteral("language"), QStringLiteral("Override UI language."), QStringLiteral("language")});
    auto listLanguagesOption = addOption({QStringLiteral("list-languages"), QStringLiteral("Lists available translations, see --language.")});
    auto confDirOption = addOption({QStringLiteral("confdir"), QStringLiteral("Use the given configuration folder."), QStringLiteral("dirname")});
    auto debugOption = addOption({QStringLiteral("debug"), QStringLiteral("Enable debug mode.")});

    // virtual file system parameters (optional)
    parser.addPositionalArgument(
        QStringLiteral("vfs file"), QStringLiteral("Virtual file system file to be opened (optional)."), {QStringLiteral("[<vfs file>]")});

    parser.process(arguments);

    CommandLineOptions out;
    // TODO: rename this option (see #8234 for more information)
    if (parser.isSet(showSettingsOption)) {
        out.showSettings = true;
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
    // load the resources
    const OCC::ResourcesLoader resource;
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);

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

    if (singleApplication.isPrimaryInstance()) {
        const auto options = parseOptions(app.arguments());

        setupLogging(options);

        platform->setApplication(&app);

        auto ocApp = new OCC::Application(platform.get(), options.debugMode, &app);
        QObject::connect(platform.get(), &Platform::requestAttention, ocApp->gui(), &ownCloudGui::slotShowSettings);

        QObject::connect(&singleApplication, &KDSingleApplication::messageReceived, ocApp, [&](const QByteArray &message) {
            const QString msg = QString::fromUtf8(message);
            qCInfo(lcMain) << Q_FUNC_INFO << msg;
            if (msg.startsWith(msgParseOptionsC())) {
                const QStringList optionsStrings = msg.mid(msgParseOptionsC().size()).split(QLatin1Char('|'));
                CommandLineOptions options = parseOptions(optionsStrings);
                if (options.showSettings) {
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

        if (options.showSettings) {
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
        // TODO: still needed? Move to platform
        // We can't call isSystemTrayAvailable with appmenu-qt5 begause it hides the systemtray
        // (issue #4693)
        if (qgetenv("QT_QPA_PLATFORMTHEME") != "appmenu-qt5") {
            if (!QSystemTrayIcon::isSystemTrayAvailable()) {
                // If the systemtray is not there, we will wait one second for it to maybe start
                // (eg boot time) then we show the settings dialog if there is still no systemtray.
                // On XFCE however, we show a message box with explainaition how to install a systemtray.
                qCInfo(lcMain) << "System tray is not available, waiting...";
                Utility::sleep(1);

                auto desktopSession = qgetenv("XDG_CURRENT_DESKTOP").toLower();
                if (desktopSession.isEmpty()) {
                    desktopSession = qgetenv("DESKTOP_SESSION").toLower();
                }
                if (desktopSession == "xfce") {
                    int attempts = 0;
                    while (!QSystemTrayIcon::isSystemTrayAvailable()) {
                        attempts++;
                        if (attempts >= 30) {
                            qCWarning(lcMain) << "System tray unavailable (xfce)";
                            warnSystray();
                            break;
                        }
                        Utility::sleep(1);
                    }
                }

                if (QSystemTrayIcon::isSystemTrayAvailable()) {
                    ocApp->tryTrayAgain();
                } else if (desktopSession != "ubuntu") {
                    qCInfo(lcMain) << "System tray still not available, showing window and trying again later";
                    QTimer::singleShot(10s, ocApp, &Application::tryTrayAgain);
                } else {
                    qCInfo(lcMain) << "System tray still not available, but assuming it's fine on 'ubuntu' desktop";
                }
            }
        }
    } else {
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

    return app.exec();
}
