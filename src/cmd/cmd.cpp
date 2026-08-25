/*
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <cstdlib>
#include <iostream>
#include <qcoreapplication.h>
#include <QDir>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkProxy>
#include <qdebug.h>

#include "account.h"
#include "accountmanager.h"
#include "accountsetupcommandlinemanager.h"
#include "folderman.h"
#include "configfile.h" // ONLY ACCESS THE STATIC FUNCTIONS!
#ifdef TOKEN_AUTH_ONLY
# include "creds/tokencredentials.h"
#else
# include "creds/httpcredentials.h"
#endif
#include "creds/webflowcredentials.h"
#include "networkjobs.h"
#include "simplesslerrorhandler.h"
#include "syncengine.h"
#include "common/filesystembase.h"
#include "common/syncjournaldb.h"
#include "common/utility.h"
#include "common/vfs.h"
#include "config.h"
#include "csync_exclude.h"


#include "cmd.h"

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
using namespace Qt::StringLiterals;

static void nullMessageHandler(QtMsgType, const QMessageLogContext &, const QString &)
{
}

struct CmdOptions
{
    QString source_dir;
    QString target_url;
    QString remotePath = u"/"_s;
    QString config_directory;
    QString user;
    QString password;
    QString proxy;
    bool silent = false;
    bool trustSSL = false;
    bool useNetrc = false;
    bool interactive = false;
    bool ignoreHiddenFiles = false;
    QString exclude;
    QString excludeAnchored;
    QString unsyncedfolders;
    int restartTimes = 0;
    int downlimit = 0;
    int uplimit = 0;
};

// we can't use csync_set_userdata because the SyncEngine sets it already.
// So we have to use a global variable
CmdOptions *opts = nullptr;

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
    termios tios{};
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
    {
    }

    void askFromUser() override
    {
        _password = ::queryPassword(user());
        _ready = true;
        persist();
        Q_EMIT asked();
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
    // FIXME: not working with client certs yet (qknight)
    bool _sslTrusted{false};
};
#endif /* TOKEN_AUTH_ONLY */

void help()
{
    const char *binaryName = APPLICATION_EXECUTABLE "cmd";

    std::cout << binaryName << " - command line " APPLICATION_NAME " client tool" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Usage: " << binaryName << " [OPTION] <source_dir> <server_url>" << std::endl;
    std::cout << "       " << binaryName << " --userid <user> --serverurl <url> [--apppassword <pass>] [OPTION]" << std::endl;
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
    std::cout << "  --exclude-anchored [file]  Exclude list file, always anchored at the" << std::endl;
    std::cout << "                         sync root regardless of the file's own name" << std::endl;
    std::cout << "                         (use this if --exclude patterns aren't matching," << std::endl;
    std::cout << "                         see nextcloud/desktop#2916, #7682)" << std::endl;
    std::cout << "  --unsyncedfolders [file]    File containing the list of unsynced remote folders (selective sync)" << std::endl;
    std::cout << "  --user, -u [name]      Use [name] as the login name" << std::endl;
    std::cout << "  --password, -p [pass]  Use [pass] as password" << std::endl;
    std::cout << "  -n                     Use netrc (5) for login" << std::endl;
    std::cout << "  --non-interactive      Do not block execution with interaction and tries to read $NC_USER and $NC_PASSWORD if not set by other means" << std::endl;
    std::cout << "  --max-sync-retries [n] Retries maximum n times (default to 3)" << std::endl;
    std::cout << "  --uplimit [n]          Limit the upload speed of files to n KB/s" << std::endl;
    std::cout << "  --downlimit [n]        Limit the download speed of files to n KB/s" << std::endl;
    std::cout << "  -h                     Sync hidden files, do not ignore them" << std::endl;
    std::cout << "  --version, -v          Display version and exit" << std::endl;
    std::cout << "  --logdebug             More verbose logging" << std::endl;
    std::cout << "  --path                 Path to a folder on a remote server" << std::endl;
    std::cout << "  --confdir [dir]        Use the given configuration directory" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Account provisioning options (non-interactive setup):" << std::endl;
    std::cout << "  --userid [user]        The user ID to configure" << std::endl;
    std::cout << "  --apppassword [pass]   The app password for authentication (optional)" << std::endl;
    std::cout << "  --serverurl [url]      The base URL of the Nextcloud server" << std::endl;
    std::cout << "  --localdirpath [path]  Local folder path for sync (optional)" << std::endl;
    std::cout << "  --remotedirpath [path] Remote folder path to sync, default /" << std::endl;
    std::cout << "  --isvfsenabled [0|1]   Enable virtual files (1) or disable (0)" << std::endl;
    std::cout << "" << std::endl;
}

void showVersion()
{
    std::cout << qUtf8Printable(Theme::instance()->versionSwitchOutput());
    exit(0);
}

enum class CommandMode {
    HelpMode,
    SyncingMode,
    ProvisioningMode,
    UnknownMode,
};

CommandMode parseOptions(const QStringList &app_args, CmdOptions *options)
{
    auto result = CommandMode::UnknownMode;

    auto args(app_args);

    const auto argCount = args.count();

    // Detect provisioning mode: --userid flag present means no positional args required
    const auto provisionMode = args.contains(QStringLiteral("--userid"));
    if (provisionMode) {
        result = CommandMode::ProvisioningMode;
    }

    if (!provisionMode) {
        if (argCount < 3) {
            if (argCount >= 2) {
                const QString option = args.at(1);
                if (option == "-v" || option == "--version") {
                    showVersion();
                }
            }
            help();
            result = CommandMode::HelpMode;
            return result;
        }

        options->target_url = args.takeLast();

        options->source_dir = args.takeLast();
        if (!options->source_dir.endsWith('/')) {
            options->source_dir.append('/');
        }
        auto sourceDirFileInfo = QFileInfo{options->source_dir};
        if (!sourceDirFileInfo.exists()) {
            std::cerr << "Source dir '" << qPrintable(options->source_dir) << "' does not exist." << std::endl;
            exit(1);
        }
        options->source_dir = sourceDirFileInfo.absoluteFilePath();
    }

    auto it = QStringListIterator{args};
    // skip file name;
    if (it.hasNext()) {
        it.next();
    }

    while (it.hasNext()) {
        const auto option = it.next();

        if (option == u"--httpproxy"_s && it.hasNext() && !it.peekNext().startsWith(u"-"_s)) {
            options->proxy = it.next();
        } else if (option == u"-s"_s || option == u"--silent"_s) {
            options->silent = true;
        } else if (option == u"--trust"_s) {
            options->trustSSL = true;
        } else if (option == u"-n"_s) {
            options->useNetrc = true;
        } else if (option == u"-h"_s) {
            options->ignoreHiddenFiles = false;
        } else if (option == u"--non-interactive"_s) {
            options->interactive = false;
        } else if ((option == u"-u"_s || option == u"--user"_s) && it.hasNext() && !it.peekNext().startsWith(u"-"_s)) {
            options->user = it.next();
        } else if ((option == u"-p"_s || option == u"--password"_s) && it.hasNext() && !it.peekNext().startsWith(u"-"_s)) {
            options->password = it.next();
        } else if (option == u"--exclude"_s && it.hasNext() && !it.peekNext().startsWith(u"-"_s)) {
            options->exclude = it.next();
        } else if (option == u"--exclude-anchored"_s && !it.peekNext().startsWith(u"-"_s)) {
            options->excludeAnchored = it.next();
        } else if (option == u"--unsyncedfolders"_s && it.hasNext() && !it.peekNext().startsWith(u"-"_s)) {
            options->unsyncedfolders = it.next();
        } else if (option == u"--max-sync-retries"_s && it.hasNext() && !it.peekNext().startsWith(u"-"_s)) {
            options->restartTimes = it.next().toInt();
        } else if (option == u"--uplimit"_s && it.hasNext() && !it.peekNext().startsWith(u"-"_s)) {
            options->uplimit = it.next().toInt() * 1000;
        } else if (option == u"--downlimit"_s && it.hasNext() && !it.peekNext().startsWith(u"-"_s)) {
            options->downlimit = it.next().toInt() * 1000;
        } else if (option == u"--logdebug"_s) {
            Logger::instance()->setLogFile("-");
            Logger::instance()->setLogDebug(true);
        } else if (option == u"--path"_s && it.hasNext() && !it.peekNext().startsWith(u"-"_s)) {
            options->remotePath = it.next();
        } else if (option == u"--confdir"_s && it.hasNext() && !it.peekNext().startsWith(u"--"_s)) {
            options->config_directory = it.next();
        } else {
            QString errorMessage;
            if (!AccountSetupCommandLineManager::instance()->parseCommandlineOption(option, it, errorMessage)) {
                help();
                result = CommandMode::HelpMode;
                return result;
            }
        }
    }

    if (!provisionMode && (options->target_url.isEmpty() || options->source_dir.isEmpty())) {
        result = CommandMode::HelpMode;
        help();
        return result;
    } else if (!provisionMode) {
        result = CommandMode::SyncingMode;
    }

    return result;
}

/* If the selective sync list is different from before, we need to disable the read from db
  (The normal client does it in SelectiveSyncDialog::accept*)
 */
void selectiveSyncFixup(OCC::SyncJournalDb *journal, const QStringList &newList)
{
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

[[nodiscard]] bool setupAccountsOnly()
{
    auto result = false;

    ConfigFile cfg;
    const auto tryMigrate = cfg.overrideServerUrl().isEmpty();
    auto accountsRestoreResult = AccountManager::AccountsRestoreFailure;
    if (accountsRestoreResult = AccountManager::instance()->restore(tryMigrate);
        accountsRestoreResult == AccountManager::AccountsRestoreFailure) {
        qWarning() << "restoring existing user accounts failed";
        return result;
    }

    result = true;
    return result;
}

[[nodiscard]] bool setupAccountsAndFolders()
{
    auto result = false;

    auto folderManager = FolderMan::instance();
    ConfigFile configFile;
    configFile.setMigrationPhase(ConfigFile::MigrationPhase::SetupUsers);
    if (!setupAccountsOnly()) {
        return result;
    }

    configFile.setMigrationPhase(ConfigFile::MigrationPhase::SetupFolders);
    const auto foldersListSize = folderManager->setupFolders();
    folderManager->setSyncEnabled(true);

    const auto prettyNamesList = [](const QList<AccountStatePtr> &accounts) {
        QStringList list;
        for (const auto &account : accounts) {
            list << account->account()->prettyName().prepend("- ");
        }
        return list.join("\n");
    };

    const auto accounts = AccountManager::instance()->accounts();
    const auto accountsListSize = accounts.size();

    qWarning() << foldersListSize << "folder(s) migrated";
    qWarning() << accountsListSize << "account(s) migrated:" << prettyNamesList(accounts);

    result = true;
    return result;
}

int main(int argc, char **argv)
{
#ifdef Q_OS_WIN
    SetDllDirectory(L"");
#endif
    QCoreApplication app(argc, argv);

#ifdef Q_OS_WIN
    // Ensure OpenSSL config file is only loaded from app directory
    QString opensslConf = QCoreApplication::applicationDirPath() + u"/openssl.cnf"_s;
    qputenv("OPENSSL_CONF", opensslConf.toLocal8Bit());
#endif

    CmdOptions options;
    options.silent = false;
    options.trustSSL = false;
    options.useNetrc = false;
    options.interactive = true;
    options.ignoreHiddenFiles = false; // Default is to sync hidden files
    options.restartTimes = 3;
    options.uplimit = 0;
    options.downlimit = 0;

    const auto commandMode = parseOptions(app.arguments(), &options);

    if (commandMode == CommandMode::HelpMode) {
        return 0;
    }

    if (options.silent) {
        qInstallMessageHandler(nullMessageHandler);
    } else {
        qSetMessagePattern("%{time MM-dd hh:mm:ss:zzz} [ %{type} %{category} ]%{if-debug}\t[ %{function} ]%{endif}:\t%{message}");
    }

    if (!options.config_directory.isEmpty()) {
        if (!ConfigFile::setConfDir(options.config_directory)) {
            std::cerr << "Invalid confdir '" << qPrintable(options.config_directory) << "', disabling." << std::endl;
        }
    }

    if (commandMode == CommandMode::ProvisioningMode) {
        if (!setupAccountsAndFolders()) {
            qWarning() << "Restoring existing accounts failed. Skip creation of a new account. See prior messages for a detailed error.";
            return -1;
        }

        if (AccountSetupCommandLineManager::instance()->isCommandLineParsed()) {
            if (AccountSetupCommandLineManager::instance()->setupAccountFromCommandLine()) {
                return 0;
            } else {
                qWarning() << "Creation of the account failed. See prior messages for a detailed error.";
                return -1;
            }
        } else {
            AccountSetupCommandLineManager::destroy();
            qWarning() << "Missing mandatory command line options for provisioning mode";
            help();
            return -1;
        }
    }

    AccountPtr account = Account::create();

    if (!account) {
        qFatal("Could not initialize account!");
        return EXIT_FAILURE;
    }

    const auto sanitisedTargetUrl = options.target_url.endsWith('/') || options.target_url.endsWith('\\') 
        ? options.target_url.chopped(1) 
        : options.target_url;
    QUrl hostUrl = QUrl::fromUserInput(sanitisedTargetUrl);

    if (const auto hostUrlPath = hostUrl.path(); hostUrlPath.contains("/webdav", Qt::CaseInsensitive) || hostUrlPath.contains("/dav", Qt::CaseInsensitive)) {
        qWarning("Dav or webdav in server URL.");
        std::cerr << "Error! Please specify only the base URL of your host with username and password. Example:" << std::endl
                  << "https://username:password@cloud.example.com" << std::endl;
        return EXIT_FAILURE;
    }

    // Order of retrieval attempt (later attempts override earlier ones):
    // 1. From URL
    // 2. From options
    // 3. From netrc (if enabled)
    // 4. From prompt (if interactive)
    // 5. From environment (if non-interactive)

    QString user = hostUrl.userName();
    QString password = hostUrl.password();

    if (!options.user.isEmpty()) {
        user = options.user;
    }

    if (!options.password.isEmpty()) {
        password = options.password;
    }

    if (options.useNetrc) {
        NetrcParser parser;
        if (parser.parse()) {
            NetrcParser::LoginPair pair = parser.find(hostUrl.host());
            user = pair.first;
            password = pair.second;
        }
    }

    if (options.interactive) {
        if (user.isEmpty()) {
            std::cout << "Please enter username: ";
            std::string s;
            std::getline(std::cin, s);
            user = QString::fromStdString(s);
        }
        if (password.isEmpty()) {
            password = queryPassword(user);
        }
    } else {
        if (user.isEmpty()) {
            user = QString::fromUtf8(qgetenv("NC_USER"));
        }
        if (password.isEmpty()) {
            password = QString::fromUtf8(qgetenv("NC_PASSWORD"));
        }
    }
   

    // Find the folder and the original owncloud url

    hostUrl.setScheme(hostUrl.scheme().replace("owncloud", "http"));

    QUrl credentialFreeUrl = hostUrl;
    credentialFreeUrl.setUserName(QString());
    credentialFreeUrl.setPassword(QString());

    const QString folder = options.remotePath;

    if (!options.proxy.isNull()) {
        QString host;
        int port = 0;
        bool ok = false;

        QStringList pList = options.proxy.split(':');
        if (pList.count() == 3) {
            // http: //192.168.178.23 : 8080
            //  0            1            2
            host = pList.at(1);
            if (host.startsWith("//")) {
                host.remove(0, 2);
            }

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
    if (options.trustSSL) {
        cred->setSSLTrusted(true);
    }
#endif

    account->setUrl(hostUrl);
    account->setSslErrorHandler(sslErrorHandler);
    account->setTrustCertificates(options.trustSSL);

    QEventLoop loop;
    auto *csjob = new CheckServerJob(account);
    csjob->setIgnoreCredentialFailure(true);
    QObject::connect(csjob, &CheckServerJob::instanceFound, [&](const QUrl &, const QJsonObject &info) {
        // see ConnectionValidator::slotCapabilitiesRecieved: only set server version if not empty
        QString serverVersion = CheckServerJob::version(info);
        if (!serverVersion.isEmpty()) {
            account->setServerVersion(serverVersion);
        }
        loop.quit();
    });
    QObject::connect(csjob, &CheckServerJob::instanceNotFound, [&]() {
        loop.quit();
    });
    QObject::connect(csjob, &CheckServerJob::timeout, [&](const QUrl &) {
        loop.quit();
    });
    csjob->start();
    loop.exec();

    if (csjob->reply()->error() != QNetworkReply::NoError){
        std::cout<<"Error connecting to server for status\n";
        return EXIT_FAILURE;
    }

    auto *job = new JsonApiJob(account, QLatin1String("ocs/v1.php/cloud/capabilities"));
    QObject::connect(job, &JsonApiJob::jsonReceived, [&](const QJsonDocument &json) {
        auto caps = json.object().value("ocs").toObject().value("data").toObject().value("capabilities").toObject();
        qDebug() << "Server capabilities" << caps;
        account->setCapabilities(caps.toVariantMap());
        // see ConnectionValidator::slotCapabilitiesRecieved: only set server version if not empty
        QString serverVersion = caps["core"].toObject()["status"].toObject()["version"].toString();
        if (!serverVersion.isEmpty()) {
            account->setServerVersion(serverVersion);
        }
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

    opts = &options;

    QStringList selectiveSyncList;
    if (!options.unsyncedfolders.isEmpty()) {
        QFile f(options.unsyncedfolders);
        if (!f.open(QFile::ReadOnly)) {
            qCritical() << "Could not open file containing the list of unsynced folders: " << options.unsyncedfolders;
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
    QString dbPath = options.source_dir + SyncJournalDb::makeDbName(options.source_dir, credentialFreeUrl, folder, user);
    SyncJournalDb db(dbPath);

    if (!selectiveSyncList.empty()) {
        selectiveSyncFixup(&db, selectiveSyncList);
    }

    SyncOptions syncOptions;
    syncOptions.fillFromEnvironmentVariables();
    syncOptions.verifyChunkSizes();
    syncOptions.setIsCmd(true);
    SyncEngine engine(account, options.source_dir, syncOptions, folder, &db);
    engine.setIgnoreHiddenFiles(options.ignoreHiddenFiles);
    engine.setNetworkLimits(options.uplimit, options.downlimit);
    QObject::connect(&engine, &SyncEngine::finished,
        [&app](bool result) { app.exit(result ? EXIT_SUCCESS : EXIT_FAILURE); });
    QObject::connect(&engine, &SyncEngine::transmissionProgress, &cmd, &Cmd::transmissionProgressSlot);
    QObject::connect(&engine, &SyncEngine::syncError,
        [](const QString &error) { qWarning() << "Sync error:" << error; });


    // Exclude lists

    bool hasUserExcludeFile = !options.exclude.isEmpty() || !options.excludeAnchored.isEmpty();
    QString systemExcludeFile = ConfigFile::excludeFileFromSystem();

    // Always try to load the user-provided exclude list(s) if specified
    if (!options.exclude.isEmpty()) {
        if (!QFile::exists(options.exclude)) {
            // A user-supplied --exclude path that can't be found is a
            // configuration error, not something to silently ignore:
            // reloadExcludeFiles() below drops missing files without
            // failing, which previously made the whole sync run with
            // no exclusions and no diagnostic (see nextcloud/desktop#4621).
            qFatal("Exclude list file supplied via --exclude does not exist: %s", qUtf8Printable(options.exclude));
            return EXIT_FAILURE;
        }
        // Keeps addExcludeFilePath()'s historic filename-based anchoring
        // heuristic for --exclude, so existing setups that rely on it
        // (however unintentionally) don't change behavior. Use
        // --exclude-anchored instead if patterns aren't matching because
        // the file isn't literally named "sync-exclude.lst".
        engine.excludedFiles().addExcludeFilePath(options.exclude);
    }
    if (!options.excludeAnchored.isEmpty()) {
        if (!QFile::exists(options.excludeAnchored)) {
            qFatal("Exclude list file supplied via --exclude-anchored does not exist: %s", qUtf8Printable(options.excludeAnchored));
            return EXIT_FAILURE;
        }
        // Always anchor patterns at the sync root regardless of the file's
        // own name, see ExcludedFiles::addExcludeFilePath().
        engine.excludedFiles().addExcludeFilePath(options.excludeAnchored, ExcludedFiles::ExcludeFileAnchor::SyncRoot);
    }
    // Load the system list if available, or if there's no user-provided list
    if (!hasUserExcludeFile || QFile::exists(systemExcludeFile)) {
        engine.excludedFiles().addExcludeFilePath(systemExcludeFile);
    }

    if (!engine.excludedFiles().reloadExcludeFiles()) {
        qFatal("Cannot load system exclude list or list supplied via --exclude/--exclude-anchored");
        return EXIT_FAILURE;
    }


    // Have to be done async, else, an error before exec() does not terminate the event loop.
    QMetaObject::invokeMethod(&engine, "startSync", Qt::QueuedConnection);

    int resultCode = app.exec();

    if (engine.isAnotherSyncNeeded() != NoFollowUpSync) {
        if (restartCount < options.restartTimes) {
            restartCount++;
            qDebug() << "Restarting Sync, because another sync is needed" << restartCount;
            goto restart_sync;
        }
        qWarning() << "Another sync is needed, but not done because restart count is exceeded" << restartCount;
    }

    return resultCode;
}
