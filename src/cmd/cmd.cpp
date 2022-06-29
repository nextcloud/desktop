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
#include <qcoreapplication.h>
#include <QStringList>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkProxy>
#include <qdebug.h>

#include "account.h"
#include "configfile.h" // ONLY ACCESS THE STATIC FUNCTIONS!
#ifdef TOKEN_AUTH_ONLY
# include "creds/tokencredentials.h"
#else
# include "creds/httpcredentials.h"
#endif
#include "common/syncjournaldb.h"
#include "config.h"
#include "csync_exclude.h"
#include "networkjobs/checkserverjobfactory.h"
#include "networkjobs/jsonjob.h"
#include "syncengine.h"

#include "theme.h"
#include "netrcparser.h"
#include "libsync/logger.h"

#include "config.h"

#ifdef Q_OS_WIN32
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

using namespace OCC;

namespace {

struct CmdOptions
{
    QString source_dir;
    QString target_url;
    QString config_directory;
    QString user;
    QString password;
    QString proxy;
    bool silent = false;
    bool trustSSL = false;
    bool useNetrc = false;
    bool interactive = true;
    bool ignoreHiddenFiles = true;
    QString exclude;
    QString unsyncedfolders;
    int restartTimes = 3;
    int downlimit = 0;
    int uplimit = 0;
    bool deltasync;
    qint64 deltasyncminfilesize;
};

struct SyncCTX
{
    explicit SyncCTX(const CmdOptions &cmdOptions)
        : options { cmdOptions }
    {
    }
    CmdOptions options;
    QUrl credentialFreeUrl;
    QString folder;
    AccountPtr account;
    QString user;
};


/* If the selective sync list is different from before, we need to disable the read from db
  (The normal client does it in SelectiveSyncDialog::accept*)
 */
void selectiveSyncFixup(OCC::SyncJournalDb *journal, const QStringList &newList)
{
    SqlDatabase db;
    if (!db.openOrCreateReadWrite(journal->databaseFilePath())) {
        return;
    }

    bool ok;

    auto oldBlackListSet = journal->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, &ok).toSet();
    if (ok) {
        auto blackListSet = newList.toSet();
        const auto changes = (oldBlackListSet - blackListSet) + (blackListSet - oldBlackListSet);
        for (const auto &it : changes) {
            journal->schedulePathForRemoteDiscovery(it);
        }

        journal->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, newList);
    }
}


void sync(const SyncCTX &ctx)
{
    QStringList selectiveSyncList;
    if (!ctx.options.unsyncedfolders.isEmpty()) {
        QFile f(ctx.options.unsyncedfolders);
        if (!f.open(QFile::ReadOnly)) {
            qCritical() << "Could not open file containing the list of unsynced folders: " << ctx.options.unsyncedfolders;
        } else {
            // filter out empty lines and comments
            selectiveSyncList = QString::fromUtf8(f.readAll()).split('\n').filter(QRegExp("\\S+")).filter(QRegExp("^[^#]"));

            for (int i = 0; i < selectiveSyncList.count(); ++i) {
                if (!selectiveSyncList.at(i).endsWith(QLatin1Char('/'))) {
                    selectiveSyncList[i].append(QLatin1Char('/'));
                }
            }
        }
    }

    const QString dbPath = ctx.options.source_dir + SyncJournalDb::makeDbName(ctx.options.source_dir);
    auto db = new SyncJournalDb(dbPath, qApp);
    if (!selectiveSyncList.empty()) {
        selectiveSyncFixup(db, selectiveSyncList);
    }

    SyncOptions opt { QSharedPointer<Vfs>(createVfsFromPlugin(Vfs::Off).release()) };
    opt.fillFromEnvironmentVariables();
    opt.verifyChunkSizes();
    auto engine = new SyncEngine(
        ctx.account, ctx.account->davUrl(), ctx.options.source_dir, ctx.folder, db);
    engine->setSyncOptions(opt);
    engine->setParent(db);

    QObject::connect(engine, &SyncEngine::finished, engine, [engine, ctx, restartCount = std::make_shared<int>(0)](bool result) {
        if (!result) {
            qWarning() << "Failed to sync";
            qApp->exit(EXIT_FAILURE);
        } else {
            if (engine->isAnotherSyncNeeded() != NoFollowUpSync) {
                if (*restartCount < ctx.options.restartTimes) {
                    (*restartCount)++;
                    qDebug() << "Restarting Sync, because another sync is needed" << *restartCount;
                    engine->startSync();
                    return;
                }
                qWarning() << "Another sync is needed, but not done because restart count is exceeded" << *restartCount;
            } else {
                qApp->quit();
            }
        }
    });
    QObject::connect(engine, &SyncEngine::aboutToRemoveAllFiles, engine, [ctx](OCC::SyncFileItem::Direction dir, std::function<void(bool)> abort) {
        if (!ctx.options.interactive) {
            abort(false);
        } else {
            std::cout << (dir == SyncFileItem::Down ? "All files in the sync folder '%1' folder were deleted on the server.\n"
                                                      "These deletes will be synchronized to your local sync folder, making such files "
                                                      "unavailable unless you have a right to restore. \n"
                                                      "If you decide to keep the files, they will be re-synced with the server if you have rights to do so.\n"
                                                      "If you decide to delete the files, they will be unavailable to you, unless you are the owner."
                                                    : "All the files in your local sync folder '%1' were deleted. These deletes will be "
                                                      "synchronized with your server, making such files unavailable unless restored.\n"
                                                      "Are you sure you want to sync those actions with the server?\n"
                                                      "If this was an accident and you decide to keep your files, they will be re-synced from the server.")
                      << std::endl;
            std::string s;
            while (true) {
                std::cout << "Remove all files?[y,n]";
                std::getline(std::cin, s);
                if (s == "y") {
                    abort(false);
                } else if (s == "n") {
                    abort(true);
                } else {
                    continue;
                }
                return;
            }
        }
    });
    QObject::connect(engine, &SyncEngine::syncError, engine,
        [](const QString &error) { qWarning() << "Sync error:" << error; });
    engine->setIgnoreHiddenFiles(ctx.options.ignoreHiddenFiles);
    engine->setNetworkLimits(ctx.options.uplimit, ctx.options.downlimit);


    // Exclude lists

    bool hasUserExcludeFile = !ctx.options.exclude.isEmpty();
    QString systemExcludeFile = ConfigFile::excludeFileFromSystem();

    // Always try to load the user-provided exclude list if one is specified
    if (hasUserExcludeFile) {
        engine->excludedFiles().addExcludeFilePath(ctx.options.exclude);
    }
    // Load the system list if available, or if there's no user-provided list
    if (!hasUserExcludeFile || QFile::exists(systemExcludeFile)) {
        engine->excludedFiles().addExcludeFilePath(systemExcludeFile);
    }

    if (!engine->excludedFiles().reloadExcludeFiles()) {
        qFatal("Cannot load system exclude list or list supplied via --exclude");
    }
    engine->startSync();
}

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
        tios_new.c_lflag &= ~static_cast<tcflag_t>(ECHO);
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
    std::cout << "Password for user " << qPrintable(user) << ": ";
    std::string s;
    std::getline(std::cin, s);
    return QString::fromStdString(s);
}

#ifndef TOKEN_AUTH_ONLY
class HttpCredentialsText : public HttpCredentials
{
public:
    HttpCredentialsText(const QString &user, const QString &password)
        : HttpCredentials(DetermineAuthTypeJob::AuthType::Basic, user, password)
    {
    }

    void askFromUser() override
    {
        _password = ::queryPassword(user());
        _ready = true;
        persist();
        emit asked();
    }
};
#endif /* TOKEN_AUTH_ONLY */

[[noreturn]] void help()
{
    const char *binaryName = APPLICATION_EXECUTABLE "cmd";

    std::cout << binaryName << " - command line " APPLICATION_NAME " client tool" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Usage: " << binaryName << " [OPTION] <source_dir> <server_url>" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "A proxy can either be set manually using --httpproxy." << std::endl;
    std::cout << "Otherwise, the setting from a configured sync client will be used." << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --silent, -s           Don't be so verbose" << std::endl;
    std::cout << "  --httpproxy [proxy]    Specify a http proxy to use." << std::endl;
    std::cout << "                         Proxy is http://server:port" << std::endl;
    std::cout << "  --trust                Trust the SSL certification." << std::endl;
    std::cout << "  --exclude [file]       Exclude list file" << std::endl;
    std::cout << "  --unsyncedfolders [file]    File containing the list of unsynced remote folders (selective sync)" << std::endl;
    std::cout << "  --user, -u [name]      Use [name] as the login name" << std::endl;
    std::cout << "  --password, -p [pass]  Use [pass] as password" << std::endl;
    std::cout << "  -n                     Use netrc (5) for login" << std::endl;
    std::cout << "  --non-interactive      Do not block execution with interaction" << std::endl;
    std::cout << "  --max-sync-retries [n] Retries maximum n times (default to 3)" << std::endl;
    std::cout << "  --uplimit [n]          Limit the upload speed of files to n KB/s" << std::endl;
    std::cout << "  --downlimit [n]        Limit the download speed of files to n KB/s" << std::endl;
    std::cout << "  -h                     Sync hidden files,do not ignore them" << std::endl;
    std::cout << "  --version, -v          Display version and exit" << std::endl;
    std::cout << "  --logdebug             More verbose logging" << std::endl;
    std::cout << "" << std::endl;
    exit(0);
}

[[noreturn]] void showVersion()
{
    std::cout << qPrintable(Theme::instance()->versionSwitchOutput());
    exit(0);
}

CmdOptions parseOptions(const QStringList &app_args)
{
    CmdOptions options;
    QStringList args(app_args);

    int argCount = args.count();

    if (argCount < 3) {
        if (argCount >= 2) {
            const QString option = args.at(1);
            if (option == "-v" || option == "--version") {
                showVersion();
            }
        }
        help();
    }

    options.target_url = args.takeLast();
    // check if the webDAV path was added to the url and append if not.
    if (!options.target_url.endsWith("/")) {
        options.target_url.append("/");
    }

    options.source_dir = args.takeLast();
    if (!options.source_dir.endsWith('/')) {
        options.source_dir.append('/');
    }
    QFileInfo fi(options.source_dir);
    if (!fi.exists()) {
        std::cerr << "Source dir '" << qPrintable(options.source_dir) << "' does not exist." << std::endl;
        exit(1);
    }
    options.source_dir = fi.absoluteFilePath();

    QStringListIterator it(args);
    // skip file name;
    if (it.hasNext())
        it.next();

    while (it.hasNext()) {
        const QString option = it.next();

        if (option == "--httpproxy" && !it.peekNext().startsWith("-")) {
            options.proxy = it.next();
        } else if (option == "-s" || option == "--silent") {
            options.silent = true;
        } else if (option == "--trust") {
            options.trustSSL = true;
        } else if (option == "-n") {
            options.useNetrc = true;
        } else if (option == "-h") {
            options.ignoreHiddenFiles = false;
        } else if (option == "--non-interactive") {
            options.interactive = false;
        } else if ((option == "-u" || option == "--user") && !it.peekNext().startsWith("-")) {
            options.user = it.next();
        } else if ((option == "-p" || option == "--password") && !it.peekNext().startsWith("-")) {
            options.password = it.next();
        } else if (option == "--exclude" && !it.peekNext().startsWith("-")) {
            options.exclude = it.next();
        } else if (option == "--unsyncedfolders" && !it.peekNext().startsWith("-")) {
            options.unsyncedfolders = it.next();
        } else if (option == "--max-sync-retries" && !it.peekNext().startsWith("-")) {
            options.restartTimes = it.next().toInt();
        } else if (option == "--uplimit" && !it.peekNext().startsWith("-")) {
            options.uplimit = it.next().toInt() * 1000;
        } else if (option == "--downlimit" && !it.peekNext().startsWith("-")) {
            options.downlimit = it.next().toInt() * 1000;
        } else if (option == "--logdebug") {
            Logger::instance()->setLogFile("-");
            Logger::instance()->setLogDebug(true);
        } else {
            help();
        }
    }

    if (options.target_url.isEmpty() || options.source_dir.isEmpty()) {
        help();
    }
    return options;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

#ifdef Q_OS_WIN
    // Ensure OpenSSL config file is only loaded from app directory
    QString opensslConf = QCoreApplication::applicationDirPath() + QString("/openssl.cnf");
    qputenv("OPENSSL_CONF", opensslConf.toLocal8Bit());
#endif
    SyncCTX ctx { parseOptions(app.arguments()) };

    if (ctx.options.silent) {
        qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &) {});
    } else {
        qSetMessagePattern(Logger::loggerPattern());
    }

    ctx.account = Account::create();

    if (!ctx.account) {
        qFatal("Could not initialize account!");
    }

    if (!ctx.options.target_url.contains(ctx.account->davPath())) {
        ctx.options.target_url.append(ctx.account->davPath());
    }

    const QUrl url = [&ctx] {
        auto tmp = QUrl::fromUserInput(ctx.options.target_url);
        // Find the folder and the original owncloud url
        QStringList splitted = tmp.path().split("/" + ctx.account->davPath());
        tmp.setPath(splitted.value(0));
        tmp.setScheme(tmp.scheme().replace("owncloud", "http"));

        // Remote folders typically start with a / and don't end with one
        ctx.folder = "/" + splitted.value(1);
        if (ctx.folder.endsWith("/") && ctx.folder != "/") {
            ctx.folder.chop(1);
        }
        return tmp;
    }();
    ctx.credentialFreeUrl = url.adjusted(QUrl::RemoveUserInfo);

    // Order of retrieval attempt (later attempts override earlier ones):
    // 1. From URL
    // 2. From options
    // 3. From netrc (if enabled)
    // 4. From prompt (if interactive)

    ctx.user = url.userName();
    QString password = url.password();

    if (!ctx.options.user.isEmpty()) {
        ctx.user = ctx.options.user;
    }

    if (!ctx.options.password.isEmpty()) {
        password = ctx.options.password;
    }

    if (ctx.options.useNetrc) {
        NetrcParser parser;
        if (parser.parse()) {
            NetrcParser::LoginPair pair = parser.find(url.host());
            ctx.user = pair.first;
            password = pair.second;
        }
    }

    if (ctx.options.interactive) {
        if (ctx.user.isEmpty()) {
            std::cout << "Please enter user name: ";
            std::string s;
            std::getline(std::cin, s);
            ctx.user = QString::fromStdString(s);
        }
        if (password.isEmpty()) {
            password = queryPassword(ctx.user);
        }
    }

    if (!ctx.options.proxy.isNull()) {
        QString host;
        uint32_t port = 0;
        bool ok;

        QStringList pList = ctx.options.proxy.split(':');
        if (pList.count() == 3) {
            // http: //192.168.178.23 : 8080
            //  0            1            2
            host = pList.at(1);
            if (host.startsWith("//"))
                host.remove(0, 2);

            port = pList.at(2).toUInt(&ok);
            if (!ok || port > std::numeric_limits<uint16_t>::max()) {
                qFatal("Invalid port number");
            }

            QNetworkProxyFactory::setUseSystemConfiguration(false);
            QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy, host, static_cast<uint16_t>(port)));
        } else {
            qFatal("Could not read httpproxy. The proxy should have the format \"http://hostname:port\".");
        }
    }

    // Pre-flight check: verify that the file specified by --unsyncedfolders can be read by us:
    if (!ctx.options.unsyncedfolders.isNull()) { // yes, isNull and not isEmpty because...:
        // ... if the user entered "--unsyncedfolders ''" on the command-line, opening that will
        // also fail
        QFile f(ctx.options.unsyncedfolders);
        if (!f.open(QFile::ReadOnly)) {
            qFatal("Cannot read unsyncedfolders file '%s': %s",
                qPrintable(ctx.options.unsyncedfolders),
                qPrintable(f.errorString()));
        }
        f.close();
    }

#ifdef TOKEN_AUTH_ONLY
    TokenCredentials *cred = new TokenCredentials(ctx.user, password, "");
    account->setCredentials(cred);
#else
    HttpCredentialsText *cred = new HttpCredentialsText(ctx.user, password);
    ctx.account->setCredentials(cred);
    if (ctx.options.trustSSL) {
        QObject::connect(ctx.account->accessManager(), &QNetworkAccessManager::sslErrors, [](QNetworkReply *reply, const QList<QSslError> &errors) {
            reply->ignoreSslErrors(errors);
        });
    } else {
        QObject::connect(ctx.account->accessManager(), &QNetworkAccessManager::sslErrors, [](QNetworkReply *reply, const QList<QSslError> &errors) {
            qCritical() << "SSL error encountered";
            for (auto e : errors) {
                qCritical() << e.errorString();
            }
            qFatal("If you trust the certificate and want to ignore the errors, use the --trust option.");
        });
    }
#endif

    ctx.account->setUrl(ctx.credentialFreeUrl);

    auto *checkServerJob = CheckServerJobFactory(ctx.account->accessManager()).startJob(ctx.account->url());

    QObject::connect(checkServerJob, &CoreJob::finished, [ctx, checkServerJob] {
        if (checkServerJob->success()) {
            // Perform a call to get the capabilities.
            auto *capabilitiesJob = new JsonApiJob(ctx.account, QStringLiteral("ocs/v1.php/cloud/capabilities"), {}, {}, nullptr);
            QObject::connect(capabilitiesJob, &JsonApiJob::finishedSignal, qApp, [capabilitiesJob, ctx] {
                auto caps = capabilitiesJob->data().value("ocs").toObject().value("data").toObject().value("capabilities").toObject();
                qDebug() << "Server capabilities" << caps;
                ctx.account->setCapabilities(caps.toVariantMap());

                if (ctx.account->serverVersionUnsupported()) {
                    qFatal("Error unsupported server");
                }

                if (capabilitiesJob->reply()->error() != QNetworkReply::NoError) {
                    qFatal("Error connecting to server");
                }

                auto userJob = new JsonApiJob(ctx.account, QLatin1String("ocs/v1.php/cloud/user"), {}, {}, nullptr);
                QObject::connect(userJob, &JsonApiJob::finishedSignal, qApp, [userJob, ctx] {
                    const QJsonObject data = userJob->data().value("ocs").toObject().value("data").toObject();
                    ctx.account->setDavUser(data.value("id").toString());
                    ctx.account->setDavDisplayName(data.value("display-name").toString());

                    // much lower age than the default since this utility is usually made to be run right after a change in the tests
                    SyncEngine::minimumFileAgeForUpload = std::chrono::milliseconds(0);
                    sync(ctx);
                });
                userJob->start();
            });
            capabilitiesJob->start();
        } else {
            switch (checkServerJob->reply()->error()) {
            case QNetworkReply::OperationCanceledError:
                qFatal("Looking up %s timed out.", qPrintable(ctx.account->url().toString()));
                break;
            default:
                qFatal("Failed to resolve %s.", qPrintable(ctx.account->url().toString()));
            }
        }
    });

    return app.exec();
}
