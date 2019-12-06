#include "webflowcredentials.h"

#include "creds/httpcredentials.h"

#include <QAuthenticator>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>
#include <keychain.h>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>

#include "accessmanager.h"
#include "account.h"
#include "configfile.h"
#include "theme.h"
#include "wizard/webview.h"
#include "webflowcredentialsdialog.h"

using namespace QKeychain;

namespace OCC {

Q_LOGGING_CATEGORY(lcWebFlowCredentials, "sync.credentials.webflow", QtInfoMsg)

namespace {
    const char userC[] = "user";
    const char clientCertificatePEMC[] = "_clientCertificatePEM";
    const char clientKeyPEMC[] = "_clientKeyPEM";
    const char clientCaCertificatePEMC[] = "_clientCaCertificatePEM";
} // ns

class WebFlowCredentialsAccessManager : public AccessManager
{
public:
    WebFlowCredentialsAccessManager(const WebFlowCredentials *cred, QObject *parent = nullptr)
        : AccessManager(parent)
        , _cred(cred)
    {
    }

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData) override
    {
        QNetworkRequest req(request);
        if (!req.attribute(WebFlowCredentials::DontAddCredentialsAttribute).toBool()) {
            if (_cred && !_cred->password().isEmpty()) {
                QByteArray credHash = QByteArray(_cred->user().toUtf8() + ":" + _cred->password().toUtf8()).toBase64();
                req.setRawHeader("Authorization", "Basic " + credHash);
            }
        }

        if (_cred && !_cred->_clientSslKey.isNull() && !_cred->_clientSslCertificate.isNull()) {
            // SSL configuration
            QSslConfiguration sslConfiguration = req.sslConfiguration();
            sslConfiguration.setLocalCertificate(_cred->_clientSslCertificate);
            sslConfiguration.setPrivateKey(_cred->_clientSslKey);

            // Merge client side CA with system CA
            auto ca = sslConfiguration.systemCaCertificates();
            ca.append(_cred->_clientSslCaCertificates);
            sslConfiguration.setCaCertificates(ca);

            req.setSslConfiguration(sslConfiguration);
        }

        return AccessManager::createRequest(op, req, outgoingData);
    }

private:
    // The credentials object dies along with the account, while the QNAM might
    // outlive both.
    QPointer<const WebFlowCredentials> _cred;
};

static void addSettingsToJob(Account *account, QKeychain::Job *job)
{
    Q_UNUSED(account)
    auto settings = ConfigFile::settingsWithGroup(Theme::instance()->appName());
    settings->setParent(job); // make the job parent to make setting deleted properly
    job->setSettings(settings.release());
}

WebFlowCredentials::WebFlowCredentials()
    : _ready(false)
    , _credentialsValid(false)
    , _keychainMigration(false)
    , _retryOnKeyChainError(false)
{

}

WebFlowCredentials::WebFlowCredentials(const QString &user, const QString &password, const QSslCertificate &certificate, const QSslKey &key, const QList<QSslCertificate> &caCertificates)
    : _user(user)
    , _password(password)
    , _clientSslKey(key)
    , _clientSslCertificate(certificate)
    , _clientSslCaCertificates(caCertificates)
    , _ready(true)
    , _credentialsValid(true)
    , _keychainMigration(false)
    , _retryOnKeyChainError(false)
{

}

QString WebFlowCredentials::authType() const {
    return QString::fromLatin1("webflow");
}

QString WebFlowCredentials::user() const {
    return _user;
}

QString WebFlowCredentials::password() const {
    return _password;
}

QNetworkAccessManager *WebFlowCredentials::createQNAM() const {
    qCInfo(lcWebFlowCredentials()) << "Get QNAM";
    AccessManager *qnam = new WebFlowCredentialsAccessManager(this);

    connect(qnam, &AccessManager::authenticationRequired, this, &WebFlowCredentials::slotAuthentication);
    connect(qnam, &AccessManager::finished, this, &WebFlowCredentials::slotFinished);

    return qnam;
}

bool WebFlowCredentials::ready() const {
    return _ready;
}

void WebFlowCredentials::fetchFromKeychain() {
    _wasFetched = true;

    // Make sure we get the user from the config file
    fetchUser();

    if (ready()) {
        emit fetched();
    } else {
        qCInfo(lcWebFlowCredentials()) << "Fetch from keychain!";
        fetchFromKeychainHelper();
    }
}

void WebFlowCredentials::askFromUser() {
    // Determine if the old flow has to be used (GS for now)
    // Do a DetermineAuthTypeJob to make sure that the server is still using Flow2
    DetermineAuthTypeJob *job = new DetermineAuthTypeJob(_account->sharedFromThis(), this);
    connect(job, &DetermineAuthTypeJob::authType, [this](DetermineAuthTypeJob::AuthType type) {
        // LoginFlowV2 > WebViewFlow > OAuth > Shib > Basic
        bool useFlow2 = (type != DetermineAuthTypeJob::WebViewFlow);

        _askDialog = new WebFlowCredentialsDialog(_account, useFlow2);

        if (!useFlow2) {
            QUrl url = _account->url();
            QString path = url.path() + "/index.php/login/flow";
            url.setPath(path);
            _askDialog->setUrl(url);
        }

        QString msg = tr("You have been logged out of %1 as user %2. Please login again")
                .arg(_account->displayName(), _user);
        _askDialog->setInfo(msg);

        _askDialog->show();

        connect(_askDialog, &WebFlowCredentialsDialog::urlCatched, this, &WebFlowCredentials::slotAskFromUserCredentialsProvided);
    });
    job->start();

    qCDebug(lcWebFlowCredentials()) << "User needs to reauth!";
}

void WebFlowCredentials::slotAskFromUserCredentialsProvided(const QString &user, const QString &pass, const QString &host) {
    Q_UNUSED(host)

    if (_user != user) {
        qCInfo(lcWebFlowCredentials()) << "Authed with the wrong user!";

        QString msg = tr("Please login with the user: %1")
                .arg(_user);
        _askDialog->setError(msg);

        if (!_askDialog->isUsingFlow2()) {
            QUrl url = _account->url();
            QString path = url.path() + "/index.php/login/flow";
            url.setPath(path);
            _askDialog->setUrl(url);
        }

        return;
    }

    qCInfo(lcWebFlowCredentials()) << "Obtained a new password";

    _password = pass;
    _ready = true;
    _credentialsValid = true;
    persist();
    emit asked();

    _askDialog->close();
    delete _askDialog;
    _askDialog = nullptr;
}


bool WebFlowCredentials::stillValid(QNetworkReply *reply) {
    if (reply->error() != QNetworkReply::NoError) {
        qCWarning(lcWebFlowCredentials()) << reply->error();
        qCWarning(lcWebFlowCredentials()) << reply->errorString();
    }
    return (reply->error() != QNetworkReply::AuthenticationRequiredError);
}

void WebFlowCredentials::persist() {
    if (_user.isEmpty()) {
        // We don't even have a user nothing to see here move along
        return;
    }

    _account->setCredentialSetting(userC, _user);
    _account->wantsAccountSaved(_account);

    // write cert if there is one
    if (!_clientSslCertificate.isNull()) {
        WritePasswordJob *job = new WritePasswordJob(Theme::instance()->appName());
        addSettingsToJob(_account, job);
        job->setInsecureFallback(false);
        connect(job, &Job::finished, this, &WebFlowCredentials::slotWriteClientCertPEMJobDone);
        job->setKey(keychainKey(_account->url().toString(), _user + clientCertificatePEMC, _account->id()));
        job->setBinaryData(_clientSslCertificate.toPem());
        job->start();
    } else {
        // no cert, just write credentials
        slotWriteClientCertPEMJobDone();
    }
}

void WebFlowCredentials::slotWriteClientCertPEMJobDone()
{
    // write ssl key if there is one
    if (!_clientSslKey.isNull()) {
        WritePasswordJob *job = new WritePasswordJob(Theme::instance()->appName());
        addSettingsToJob(_account, job);
        job->setInsecureFallback(false);
        connect(job, &Job::finished, this, &WebFlowCredentials::slotWriteClientKeyPEMJobDone);
        job->setKey(keychainKey(_account->url().toString(), _user + clientKeyPEMC, _account->id()));
        job->setBinaryData(_clientSslKey.toPem());
        job->start();
    } else {
        // no key, just write credentials
        slotWriteClientKeyPEMJobDone();
    }
}

void WebFlowCredentials::writeSingleClientCaCertPEM()
{
    // write a ca cert if there is any in the queue
    if (!_clientSslCaCertificatesWriteQueue.isEmpty()) {
        // grab and remove the first cert from the queue
        auto cert = _clientSslCaCertificatesWriteQueue.dequeue();

        auto index = (_clientSslCaCertificates.count() - _clientSslCaCertificatesWriteQueue.count()) - 1;

        // keep the limit
        if (index > (_clientSslCaCertificatesMaxCount - 1)) {
            qCWarning(lcWebFlowCredentials) << "Maximum client CA cert count exceeded while writing slot" << QString::number(index) << "), cutting off after " << QString::number(_clientSslCaCertificatesMaxCount) << "certs";

            _clientSslCaCertificatesWriteQueue.clear();

            slotWriteClientCaCertsPEMJobDone(nullptr);
            return;
        }

        WritePasswordJob *job = new WritePasswordJob(Theme::instance()->appName());
        addSettingsToJob(_account, job);
        job->setInsecureFallback(false);
        connect(job, &Job::finished, this, &WebFlowCredentials::slotWriteClientCaCertsPEMJobDone);
        job->setKey(keychainKey(_account->url().toString(), _user + clientCaCertificatePEMC + QString::number(index), _account->id()));
        job->setBinaryData(cert.toPem());
        job->start();
    } else {
        slotWriteClientCaCertsPEMJobDone(nullptr);
    }
}

void WebFlowCredentials::slotWriteClientKeyPEMJobDone()
{
    _clientSslCaCertificatesWriteQueue.clear();

    // write ca certs if there are any
    if (!_clientSslCaCertificates.isEmpty()) {
        // queue the certs to avoid trouble on Windows (Workaround for CredWriteW used by QtKeychain)
        _clientSslCaCertificatesWriteQueue.append(_clientSslCaCertificates);

        // first ca cert
        writeSingleClientCaCertPEM();
    } else {
        slotWriteClientCaCertsPEMJobDone(nullptr);
    }
}

void WebFlowCredentials::slotWriteClientCaCertsPEMJobDone(QKeychain::Job *incomingJob)
{
    // errors / next ca cert?
    if (incomingJob && !_clientSslCaCertificates.isEmpty()) {
        WritePasswordJob *writeJob = static_cast<WritePasswordJob *>(incomingJob);

        if (writeJob->error() != NoError) {
            qCWarning(lcWebFlowCredentials) << "Error while writing client CA cert" << writeJob->errorString();
        }

        if (!_clientSslCaCertificatesWriteQueue.isEmpty()) {
            // next ca cert
            writeSingleClientCaCertPEM();
            return;
        }
    }

    // done storing ca certs, time for the password
    WritePasswordJob *job = new WritePasswordJob(Theme::instance()->appName());
    addSettingsToJob(_account, job);
    job->setInsecureFallback(false);
    connect(job, &Job::finished, this, &WebFlowCredentials::slotWriteJobDone);
    job->setKey(keychainKey(_account->url().toString(), _user, _account->id()));
    job->setTextData(_password);
    job->start();
}

void WebFlowCredentials::slotWriteJobDone(QKeychain::Job *job)
{
    delete job->settings();
    switch (job->error()) {
    case NoError:
        break;
    default:
        qCWarning(lcWebFlowCredentials) << "Error while writing password" << job->errorString();
    }
    WritePasswordJob *wjob = qobject_cast<WritePasswordJob *>(job);
    wjob->deleteLater();
}

void WebFlowCredentials::invalidateToken() {
    // clear the session cookie.
    _account->clearCookieJar();

    // let QNAM forget about the password
    // This needs to be done later in the event loop because we might be called (directly or
    // indirectly) from QNetworkAccessManagerPrivate::authenticationRequired, which itself
    // is a called from a BlockingQueuedConnection from the Qt HTTP thread. And clearing the
    // cache needs to synchronize again with the HTTP thread.
    QTimer::singleShot(0, _account, &Account::clearQNAMCache);
}

void WebFlowCredentials::forgetSensitiveData() {
    _password = QString();
    _ready = false;

    fetchUser();

    _account->deleteAppPassword();

    const QString kck = keychainKey(_account->url().toString(), _user, _account->id());
    if (kck.isEmpty()) {
        qCDebug(lcWebFlowCredentials()) << "InvalidateToken: User is empty, bailing out!";
        return;
    }

    DeletePasswordJob *job = new DeletePasswordJob(Theme::instance()->appName());
    job->setInsecureFallback(false);
    job->setKey(kck);
    job->start();

    invalidateToken();

    /* IMPORTANT
     * TODO: For "Log out" & "Remove account": Remove client CA certs and KEY!
     *
     *       Disabled as long as selecting another cert is not supported by the UI.
     *
     *       Being able to specify a new certificate is important anyway: expiry etc.
    */
    //deleteKeychainEntries();
}

void WebFlowCredentials::setAccount(Account *account) {
    AbstractCredentials::setAccount(account);
    if (_user.isEmpty()) {
        fetchUser();
    }
}

QString WebFlowCredentials::fetchUser() {
    _user = _account->credentialSetting(userC).toString();
    return _user;
}

void WebFlowCredentials::slotAuthentication(QNetworkReply *reply, QAuthenticator *authenticator) {
    Q_UNUSED(reply)

    if (!_ready) {
        return;
    }

    if (_credentialsValid == false) {
        return;
    }

    qCDebug(lcWebFlowCredentials()) << "Requires authentication";

    authenticator->setUser(_user);
    authenticator->setPassword(_password);
    _credentialsValid = false;
}

void WebFlowCredentials::slotFinished(QNetworkReply *reply) {
    qCInfo(lcWebFlowCredentials()) << "request finished";

    if (reply->error() == QNetworkReply::NoError) {
        _credentialsValid = true;

        /// Used later for remote wipe
        _account->setAppPassword(_password);
    }
}

void WebFlowCredentials::fetchFromKeychainHelper() {
    // Read client cert from keychain
    const QString kck = keychainKey(
        _account->url().toString(),
        _user + clientCertificatePEMC,
        _keychainMigration ? QString() : _account->id());

    ReadPasswordJob *job = new ReadPasswordJob(Theme::instance()->appName());
    addSettingsToJob(_account, job);
    job->setInsecureFallback(false);
    job->setKey(kck);
    connect(job, &Job::finished, this, &WebFlowCredentials::slotReadClientCertPEMJobDone);
    job->start();
}

void WebFlowCredentials::slotReadClientCertPEMJobDone(QKeychain::Job *incomingJob)
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
    Q_ASSERT(!incomingJob->insecureFallback()); // If insecureFallback is set, the next test would be pointless
    if (_retryOnKeyChainError && (incomingJob->error() == QKeychain::NoBackendAvailable
            || incomingJob->error() == QKeychain::OtherError)) {
        // Could be that the backend was not yet available. Wait some extra seconds.
        // (Issues #4274 and #6522)
        // (For kwallet, the error is OtherError instead of NoBackendAvailable, maybe a bug in QtKeychain)
        qCInfo(lcWebFlowCredentials) << "Backend unavailable (yet?) Retrying in a few seconds." << incomingJob->errorString();
        QTimer::singleShot(10000, this, &WebFlowCredentials::fetchFromKeychainHelper);
        _retryOnKeyChainError = false;
        return;
    }
    _retryOnKeyChainError = false;
#endif

    // Store PEM in memory
    ReadPasswordJob *readJob = static_cast<ReadPasswordJob *>(incomingJob);
    if (readJob->error() == NoError && readJob->binaryData().length() > 0) {
        QList<QSslCertificate> sslCertificateList = QSslCertificate::fromData(readJob->binaryData(), QSsl::Pem);
        if (sslCertificateList.length() >= 1) {
            _clientSslCertificate = sslCertificateList.at(0);
        }
    }

    // Load key too
    const QString kck = keychainKey(
        _account->url().toString(),
        _user + clientKeyPEMC,
        _keychainMigration ? QString() : _account->id());

    ReadPasswordJob *job = new ReadPasswordJob(Theme::instance()->appName());
    addSettingsToJob(_account, job);
    job->setInsecureFallback(false);
    job->setKey(kck);
    connect(job, &Job::finished, this, &WebFlowCredentials::slotReadClientKeyPEMJobDone);
    job->start();
}

void WebFlowCredentials::slotReadClientKeyPEMJobDone(QKeychain::Job *incomingJob)
{
    // Store key in memory
    ReadPasswordJob *readJob = static_cast<ReadPasswordJob *>(incomingJob);

    if (readJob->error() == NoError && readJob->binaryData().length() > 0) {
        QByteArray clientKeyPEM = readJob->binaryData();
        // FIXME Unfortunately Qt has a bug and we can't just use QSsl::Opaque to let it
        // load whatever we have. So we try until it works.
        _clientSslKey = QSslKey(clientKeyPEM, QSsl::Rsa);
        if (_clientSslKey.isNull()) {
            _clientSslKey = QSslKey(clientKeyPEM, QSsl::Dsa);
        }
        if (_clientSslKey.isNull()) {
            _clientSslKey = QSslKey(clientKeyPEM, QSsl::Ec);
        }
        if (_clientSslKey.isNull()) {
            qCWarning(lcWebFlowCredentials) << "Could not load SSL key into Qt!";
        }
    }

    // Start fetching client CA certs
    _clientSslCaCertificates.clear();

    readSingleClientCaCertPEM();
}

void WebFlowCredentials::readSingleClientCaCertPEM()
{
    // try to fetch a client ca cert
    if (_clientSslCaCertificates.count() < _clientSslCaCertificatesMaxCount) {
        const QString kck = keychainKey(
            _account->url().toString(),
            _user + clientCaCertificatePEMC + QString::number(_clientSslCaCertificates.count()),
            _keychainMigration ? QString() : _account->id());

        ReadPasswordJob *job = new ReadPasswordJob(Theme::instance()->appName());
        addSettingsToJob(_account, job);
        job->setInsecureFallback(false);
        job->setKey(kck);
        connect(job, &Job::finished, this, &WebFlowCredentials::slotReadClientCaCertsPEMJobDone);
        job->start();
    } else {
        qCWarning(lcWebFlowCredentials) << "Maximum client CA cert count exceeded while reading, ignoring after " << _clientSslCaCertificatesMaxCount;

        slotReadClientCaCertsPEMJobDone(nullptr);
    }
}

void WebFlowCredentials::slotReadClientCaCertsPEMJobDone(QKeychain::Job *incomingJob) {
    // Store key in memory
    ReadPasswordJob *readJob = static_cast<ReadPasswordJob *>(incomingJob);

    if (readJob) {
        if (readJob->error() == NoError && readJob->binaryData().length() > 0) {
            QList<QSslCertificate> sslCertificateList = QSslCertificate::fromData(readJob->binaryData(), QSsl::Pem);
            if (sslCertificateList.length() >= 1) {
                _clientSslCaCertificates.append(sslCertificateList.at(0));
            }

            // try next cert
            readSingleClientCaCertPEM();
            return;
        } else {
            if (readJob->error() != QKeychain::Error::EntryNotFound ||
                ((readJob->error() == QKeychain::Error::EntryNotFound) && _clientSslCaCertificates.count() == 0)) {
                qCWarning(lcWebFlowCredentials) << "Unable to read client CA cert slot " << QString::number(_clientSslCaCertificates.count()) << readJob->errorString();
            }
        }
    }

    // Now fetch the actual server password
    const QString kck = keychainKey(
        _account->url().toString(),
        _user,
        _keychainMigration ? QString() : _account->id());

    ReadPasswordJob *job = new ReadPasswordJob(Theme::instance()->appName());
    addSettingsToJob(_account, job);
    job->setInsecureFallback(false);
    job->setKey(kck);
    connect(job, &Job::finished, this, &WebFlowCredentials::slotReadPasswordJobDone);
    job->start();
}

void WebFlowCredentials::slotReadPasswordJobDone(Job *incomingJob) {
    QKeychain::ReadPasswordJob *job = static_cast<ReadPasswordJob *>(incomingJob);
    QKeychain::Error error = job->error();

    // If we could not find the entry try the old entries
    if (!_keychainMigration && error == QKeychain::EntryNotFound) {
        _keychainMigration = true;
        fetchFromKeychainHelper();
        return;
    }

    if (_user.isEmpty()) {
        qCWarning(lcWebFlowCredentials) << "Strange: User is empty!";
    }

    if (error == QKeychain::NoError) {
        _password = job->textData();
        _ready = true;
        _credentialsValid = true;
    } else {
        _ready = false;
    }
    emit fetched();

    // If keychain data was read from legacy location, wipe these entries and store new ones
    if (_keychainMigration && _ready) {
        _keychainMigration = false;
        persist();
        deleteKeychainEntries(true); // true: delete old entries
        qCInfo(lcWebFlowCredentials) << "Migrated old keychain entries";
    }
}

void WebFlowCredentials::deleteKeychainEntries(bool oldKeychainEntries) {
    auto startDeleteJob = [this, oldKeychainEntries](QString user) {
        DeletePasswordJob *job = new DeletePasswordJob(Theme::instance()->appName());
        addSettingsToJob(_account, job);
        job->setInsecureFallback(true);
        job->setKey(keychainKey(_account->url().toString(),
                                user,
                                oldKeychainEntries ? QString() : _account->id()));
        job->start();
    };

    startDeleteJob(_user);
    startDeleteJob(_user + clientKeyPEMC);
    startDeleteJob(_user + clientCertificatePEMC);

    for (auto i = 0; i < _clientSslCaCertificates.count(); i++) {
        startDeleteJob(_user + clientCaCertificatePEMC + QString::number(i));
    }
}

}
