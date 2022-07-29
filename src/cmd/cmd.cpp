/*
 * Copyright (C) by Olivier Goffart <ogoffart@owncloud.com>
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 * Copyright (C) by Daniel Heule <daniel.heule@gmail.com>
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

#include <iostream>
#include <random>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QStringList>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkProxy>
#include <QDebug>

#include "account.h"
#include "configfile.h" // ONLY ACCESS THE STATIC FUNCTIONS!
#ifdef TOKEN_AUTH_ONLY
# include "creds/tokencredentials.h"
#else
# include "creds/httpcredentials.h"
#endif
#include "simplesslerrorhandler.h"
#include "syncengine.h"
#include "common/syncjournaldb.h"
#include "config.h"
#include "csync_exclude.h"


#include "cmd.h"

#include "theme.h"
#include "netrcparser.h"
#include "libsync/logger.h"

#ifdef Q_OS_WIN32
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

using namespace OCC;


static void nullMessageHandler(QtMsgType, const QMessageLogContext &, const QString &)
{
}

class EchoDisabler
{
public:
    EchoDisabler()
    {
#ifdef Q_OS_WIN
        hStdin = GetStdHandle(STD_INPUT_HANDLE);
        GetConsoleMode(hStdin, &mode);
        SetConsoleMode(hStdin, mode & (~ENABLE_ECHO_INPUT));
#else
        tcgetattr(STDIN_FILENO, &tios);
        termios tios_new = tios;
        tios_new.c_lflag &= ~ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &tios_new);
#endif
    }

    ~EchoDisabler()
    {
#ifdef Q_OS_WIN
        SetConsoleMode(hStdin, mode);
#else
        tcsetattr(STDIN_FILENO, TCSANOW, &tios);
#endif
    }

private:
#ifdef Q_OS_WIN
    DWORD mode = 0;
    HANDLE hStdin;
#else
    termios tios;
#endif
};

QString queryPassword(const QString &user)
{
    EchoDisabler disabler;
    std::cout << "Password for account with username " << qPrintable(user) << ": ";
    std::string s;
    std::getline(std::cin, s);
    return QString::fromStdString(s);
}

#ifndef TOKEN_AUTH_ONLY
class HttpCredentialsText : public HttpCredentials
{
public:
    HttpCredentialsText(const QString &user, const QString &password)
        : HttpCredentials(user, password)
        , // FIXME: not working with client certs yet (qknight)
        _sslTrusted(false)
    {
    }

    void askFromUser() override
    {
        _password = ::queryPassword(user());
        _ready = true;
        persist();
        emit asked();
    }

    void setSSLTrusted(bool isTrusted)
    {
        _sslTrusted = isTrusted;
    }

    bool sslIsTrusted() override
    {
        return _sslTrusted;
    }

private:
    bool _sslTrusted;
};
#endif /* TOKEN_AUTH_ONLY */

/* If the selective sync list is different from before, we need to disable the read from db
  (The normal client does it in SelectiveSyncDialog::accept*)
 */
void selectiveSyncFixup(OCC::SyncJournalDb *journal, const QStringList &newList)
{
    SqlDatabase db;
    if (!db.openOrCreateReadWrite(journal->databaseFilePath())) {
        return;
    }

    bool ok = false;

    const auto selectiveSyncList = journal->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, &ok);
    const QSet<QString> oldBlackListSet(selectiveSyncList.begin(), selectiveSyncList.end());
    if (ok) {
        const QSet<QString> blackListSet(newList.begin(), newList.end());
        const auto changes = (oldBlackListSet - blackListSet) + (blackListSet - oldBlackListSet);
        for (const auto &it : changes) {
            journal->schedulePathForRemoteDiscovery(it);
        }

        journal->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, newList);
    }
}

int main(int argc, char **argv)
{
#ifdef Q_OS_WIN
    SetDllDirectory(L"");
#endif
    QCoreApplication app(argc, argv);
    app.setApplicationName(Theme::instance()->appName());
    app.setApplicationVersion(Theme::instance()->versionSwitchOutput());

    QCommandLineParser parser;
    QString binaryName = APPLICATION_EXECUTABLE "cmd";
    QString description = QString(R"(%1 - command line %2 client tool

Usage: %3 [OPTION] <source_dir> <server_url>
       A proxy can either be set manually using --httpproxy.
       Otherwise, the setting from a configured sync client will be used.
)").arg(binaryName).arg(APPLICATION_NAME).arg(binaryName);
    parser.setApplicationDescription(description);
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions({
        { {"u", "user"}, "Use [name] as the login name", "name" },
        { {"p", "password"}, "Use [pass] as password", "pass" },
        { "httpproxy", "Specify a http proxy to use. Proxy is http://server:port", "proxy" },
        { {"s", "silent"}, "Don't be so verbose" },
        { "trust", "Trust the SSL certification." },
        { {"n", "netrc"}, "Use netrc (5) for login" },
        { "non-interactive", "Do not block execution with interaction" },
        { {"i", "ignore-hidden"}, "Ignores hidden files, do not sync them" },
        { "exclude", "Exclude list file", "file" },
        { "unsyncedfolders", "File containing the list of unsynced remote folders (selective sync)", "file" },
        { "max-sync-retries", "Retries maximum n times (default to 3)", "n", "3" },
        { "uplimit", "Limit the upload speed of files to n KB/s", "n", "0" },
        { "downlimit", "Limit the download speed of files to n KB/s", "n", "0" },
        { "logdebug", "More verbose logging" },
        { "path", "Path to a folder on a remote server" }
    });
    parser.addPositionalArgument("source_dir", "Source directory");
    parser.addPositionalArgument("server_url", "Server url");
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.size() < 2 || parser.isSet("help")) {
        parser.showHelp();
    }

#ifdef Q_OS_WIN
    // Ensure OpenSSL config file is only loaded from app directory
    QString opensslConf = QCoreApplication::applicationDirPath() + QString("/openssl.cnf");
    qputenv("OPENSSL_CONF", opensslConf.toLocal8Bit());
#endif

    QString source_dir = args.at(0);
    const QString target_url = args.at(1);
    const QString options_user = parser.value("user");
    const QString options_password = parser.value("password");
    const QString proxy = parser.value("httpproxy");
    const bool silent = parser.isSet("silent");
    const bool trustSSL = parser.isSet("trust");
    const bool useNetrc = parser.isSet("netrc");
    const bool interactive = !parser.isSet("non-interactive");
    const bool ignoreHiddenFiles = parser.isSet("ignore-hidden");
    const QString exclude = parser.value("exclude");
    const QString unsyncedfolders = parser.value("unsyncedfolders");
    const int restartTimes = parser.value("max-sync-retries").toInt();
    const int uplimit = parser.value("uplimit").toInt() * 1000;
    const int downlimit = parser.value("downlimit").toInt() * 1000;
    const bool logdebug = parser.isSet("logdebug");
    const QString remotePath = parser.value("path");

    if (logdebug) {
        Logger::instance()->setLogFile("-");
        Logger::instance()->setLogDebug(true);
    }

    if (!source_dir.endsWith('/')) {
        source_dir.append('/');
    }
    QFileInfo fi(source_dir);
    if (!fi.exists()) {
        std::cerr << "Source dir '" << qPrintable(source_dir) << "' does not exist." << std::endl;
        exit(1);
    }
    source_dir = fi.absoluteFilePath();

    if (silent) {
        qInstallMessageHandler(nullMessageHandler);
    } else {
        qSetMessagePattern("%{time MM-dd hh:mm:ss:zzz} [ %{type} %{category} ]%{if-debug}\t[ %{function} ]%{endif}:\t%{message}");
    }

    AccountPtr account = Account::create();

    if (!account) {
        qFatal("Could not initialize account!");
        return EXIT_FAILURE;
    }

    if (target_url.contains("/webdav", Qt::CaseInsensitive) || target_url.contains("/dav", Qt::CaseInsensitive)) {
        qWarning("Dav or webdav in server URL.");
        std::cerr << "Error! Please specify only the base URL of your host with username and password. Example:" << std::endl
                  << "http(s)://username:password@cloud.example.com" << std::endl;
        return EXIT_FAILURE;
    }

    QUrl hostUrl = QUrl::fromUserInput((target_url.endsWith(QLatin1Char('/')) || target_url.endsWith(QLatin1Char('\\'))) ? target_url.chopped(1) : target_url);

    // Order of retrieval attempt (later attempts override earlier ones):
    // 1. From URL
    // 2. From options
    // 3. From netrc (if enabled)
    // 4. From prompt (if interactive)

    QString user = hostUrl.userName();
    QString password = hostUrl.password();

    if (!options_user.isEmpty()) {
        user = options_user;
    }

    if (!options_password.isEmpty()) {
        password = options_password;
    }

    if (useNetrc) {
        NetrcParser parser;
        if (parser.parse()) {
            NetrcParser::LoginPair pair = parser.find(hostUrl.host());
            user = pair.first;
            password = pair.second;
        }
    }

    if (interactive) {
        if (user.isEmpty()) {
            std::cout << "Please enter username: ";
            std::string s;
            std::getline(std::cin, s);
            user = QString::fromStdString(s);
        }
        if (password.isEmpty()) {
            password = queryPassword(user);
        }
    }

    // Find the folder and the original owncloud url

    hostUrl.setScheme(hostUrl.scheme().replace("owncloud", "http"));

    QUrl credentialFreeUrl = hostUrl;
    credentialFreeUrl.setUserName(QString());
    credentialFreeUrl.setPassword(QString());

    const QString folder = remotePath;

    if (!proxy.isEmpty()) {
        QString host;
        int port = 0;
        bool ok = false;

        QStringList pList = proxy.split(':');
        if (pList.count() == 3) {
            // http: //192.168.178.23 : 8080
            //  0            1            2
            host = pList.at(1);
            if (host.startsWith("//"))
                host.remove(0, 2);

            port = pList.at(2).toInt(&ok);

            QNetworkProxyFactory::setUseSystemConfiguration(false);
            QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy, host, port));
        } else {
            qFatal("Could not read httpproxy. The proxy should have the format \"http://hostname:port\".");
        }
    }

    auto *sslErrorHandler = new SimpleSslErrorHandler;

#ifdef TOKEN_AUTH_ONLY
    auto *cred = new TokenCredentials(user, password, "");
    account->setCredentials(cred);
#else
    auto *cred = new HttpCredentialsText(user, password);
    account->setCredentials(cred);
    if (trustSSL) {
        cred->setSSLTrusted(true);
    }
#endif

    account->setUrl(hostUrl);
    account->setSslErrorHandler(sslErrorHandler);

    QEventLoop loop;
    auto *job = new JsonApiJob(account, QLatin1String("ocs/v1.php/cloud/capabilities"));
    QObject::connect(job, &JsonApiJob::jsonReceived, [&](const QJsonDocument &json) {
        auto caps = json.object().value("ocs").toObject().value("data").toObject().value("capabilities").toObject();
        qDebug() << "Server capabilities" << caps;
        account->setCapabilities(caps.toVariantMap());
        account->setServerVersion(caps["core"].toObject()["status"].toObject()["version"].toString());
        loop.quit();
    });
    job->start();
    loop.exec();

    if (job->reply()->error() != QNetworkReply::NoError){
        std::cout<<"Error connecting to server\n";
        return EXIT_FAILURE;
    }

    job = new JsonApiJob(account, QLatin1String("ocs/v1.php/cloud/user"));
    QObject::connect(job, &JsonApiJob::jsonReceived, [&](const QJsonDocument &json) {
        const QJsonObject data = json.object().value("ocs").toObject().value("data").toObject();
        account->setDavUser(data.value("id").toString());
        account->setDavDisplayName(data.value("display-name").toString());
        loop.quit();
    });
    job->start();
    loop.exec();

    // much lower age than the default since this utility is usually made to be run right after a change in the tests
    SyncEngine::minimumFileAgeForUpload = std::chrono::milliseconds(0);

    int restartCount = 0;
restart_sync:

    QStringList selectiveSyncList;
    if (!unsyncedfolders.isEmpty()) {
        QFile f(unsyncedfolders);
        if (!f.open(QFile::ReadOnly)) {
            qCritical() << "Could not open file containing the list of unsynced folders: " << unsyncedfolders;
        } else {
            // filter out empty lines and comments
            selectiveSyncList = QString::fromUtf8(f.readAll()).split('\n').filter(QRegularExpression("\\S+")).filter(QRegularExpression("^[^#]"));

            for (int i = 0; i < selectiveSyncList.count(); ++i) {
                if (!selectiveSyncList.at(i).endsWith(QLatin1Char('/'))) {
                    selectiveSyncList[i].append(QLatin1Char('/'));
                }
            }
        }
    }

    Cmd cmd;
    QString dbPath = source_dir + SyncJournalDb::makeDbName(source_dir, credentialFreeUrl, folder, user);
    SyncJournalDb db(dbPath);

    if (!selectiveSyncList.empty()) {
        selectiveSyncFixup(&db, selectiveSyncList);
    }

    SyncOptions opt;
    opt.fillFromEnvironmentVariables();
    opt.verifyChunkSizes();
    SyncEngine engine(account, source_dir, folder, &db);
    engine.setIgnoreHiddenFiles(ignoreHiddenFiles);
    engine.setNetworkLimits(uplimit, downlimit);
    QObject::connect(&engine, &SyncEngine::finished,
        [&app](bool result) { app.exit(result ? EXIT_SUCCESS : EXIT_FAILURE); });
    QObject::connect(&engine, &SyncEngine::transmissionProgress, &cmd, &Cmd::transmissionProgressSlot);
    QObject::connect(&engine, &SyncEngine::syncError,
        [](const QString &error) { qWarning() << "Sync error:" << error; });


    // Exclude lists

    bool hasUserExcludeFile = !exclude.isEmpty();
    QString systemExcludeFile = ConfigFile::excludeFileFromSystem();

    // Always try to load the user-provided exclude list if one is specified
    if (hasUserExcludeFile) {
        engine.excludedFiles().addExcludeFilePath(exclude);
    }
    // Load the system list if available, or if there's no user-provided list
    if (!hasUserExcludeFile || QFile::exists(systemExcludeFile)) {
        engine.excludedFiles().addExcludeFilePath(systemExcludeFile);
    }

    if (!engine.excludedFiles().reloadExcludeFiles()) {
        qFatal("Cannot load system exclude list or list supplied via --exclude");
        return EXIT_FAILURE;
    }


    // Have to be done async, else, an error before exec() does not terminate the event loop.
    QMetaObject::invokeMethod(&engine, "startSync", Qt::QueuedConnection);

    int resultCode = app.exec();

    if (engine.isAnotherSyncNeeded() != NoFollowUpSync) {
        if (restartCount < restartTimes) {
            restartCount++;
            qDebug() << "Restarting Sync, because another sync is needed" << restartCount;
            goto restart_sync;
        }
        qWarning() << "Another sync is needed, but not done because restart count is exceeded" << restartCount;
    }

    return resultCode;
}
