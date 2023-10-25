#include "credentialmanager.h"

#include "account.h"
#include "configfile.h"
#include "theme.h"

#include "common/asserts.h"

#include <QCborValue>
#include <QLoggingCategory>
#include <QTimer>

#include <chrono>

using namespace std::chrono_literals;

using namespace OCC;

Q_LOGGING_CATEGORY(lcCredentialsManager, "sync.credentials.manager", QtDebugMsg)

namespace {
constexpr auto tiemoutC = 5s;
QString credentialKeyC()
{
    return QStringLiteral("%1_credentials").arg(Theme::instance()->appName());
}

QString accountKey(const Account *acc)
{
    OC_ASSERT(!acc->url().isEmpty());
    return QStringLiteral("%1:%2:%3").arg(credentialKeyC(), acc->url().host(), acc->uuid().toString(QUuid::WithoutBraces));
}

QString scope(const CredentialManager *const manager)
{
    return manager->account() ? accountKey(manager->account()) : credentialKeyC();
}

QString scopedKey(const CredentialManager *const manager, const QString &key)
{
    return scope(manager) + QLatin1Char(':') + key;
}
}

CredentialManager::CredentialManager(Account *acc)
    : QObject(acc)
    , _account(acc)
{
}

CredentialManager::CredentialManager(QObject *parent)
    : QObject(parent)
{
}


CredentialJob *CredentialManager::get(const QString &key)
{
    qCInfo(lcCredentialsManager) << "get" << scopedKey(this, key);
    auto out = new CredentialJob(this, key);
    out->start();
    return out;
}

QKeychain::Job *CredentialManager::set(const QString &key, const QVariant &data)
{
    OC_ASSERT(!data.isNull());
    qCInfo(lcCredentialsManager) << "set" << scopedKey(this, key);
    auto writeJob = new QKeychain::WritePasswordJob(Theme::instance()->appName());
    writeJob->setKey(scopedKey(this, key));

    auto timer = new QTimer(writeJob);
    timer->setInterval(tiemoutC);
    timer->setSingleShot(true);

    auto timedOut = std::make_unique<bool>(false);
    connect(timer, &QTimer::timeout, writeJob, [writeJob, timedOut = timedOut.get()] {
        *timedOut = true;
        Q_EMIT writeJob->finished(writeJob);
        writeJob->deleteLater();
    });
    connect(writeJob, &QKeychain::WritePasswordJob::finished, this, [writeJob, timer, key, timedOut = std::move(timedOut), this] {
        timer->stop();
        if (writeJob->error() == QKeychain::NoError) {
            if (*timedOut.get()) {
                qCInfo(lcCredentialsManager) << "set" << writeJob->key() << "timed out";
            } else {
                qCInfo(lcCredentialsManager) << "added" << writeJob->key();
                // just a list, the values don't matter
                credentialsList().setValue(key, true);
            }
        } else {
            qCWarning(lcCredentialsManager) << "Failed to set:" << writeJob->key() << writeJob->errorString();
        }
    });
    writeJob->setBinaryData(QCborValue::fromVariant(data).toCbor());
    // start is delayed so we can directly call it
    writeJob->start();
    timer->start();

    return writeJob;
}

QKeychain::Job *CredentialManager::remove(const QString &key)
{
    OC_ASSERT(contains(key));
    // remove immediately to prevent double invocation by clear()
    credentialsList().remove(key);
    qCInfo(lcCredentialsManager) << "del" << scopedKey(this, key);
    auto keychainJob = new QKeychain::DeletePasswordJob(Theme::instance()->appName());
    keychainJob->setKey(scopedKey(this, key));
    connect(keychainJob, &QKeychain::DeletePasswordJob::finished, this, [keychainJob, key, this] {
        OC_ASSERT(keychainJob->error() != QKeychain::EntryNotFound);
        if (keychainJob->error() == QKeychain::NoError) {
            qCInfo(lcCredentialsManager) << "removed" << scopedKey(this, key);
        } else {
            qCWarning(lcCredentialsManager) << "Failed to remove:" << scopedKey(this, key) << keychainJob->errorString();
        }
    });
    // start is delayed so we can directly call it
    keychainJob->start();
    return keychainJob;
}

QVector<QPointer<QKeychain::Job>> CredentialManager::clear(const QString &group)
{
    OC_ENFORCE(_account || !group.isEmpty());
    const auto keys = knownKeys(group);
    QVector<QPointer<QKeychain::Job>> out;
    out.reserve(keys.size());
    for (const auto &key : keys) {
        out << remove(key);
    }
    return out;
}

const Account *CredentialManager::account() const
{
    return _account;
}

bool CredentialManager::contains(const QString &key) const
{
    return credentialsList().contains(key);
}

QStringList CredentialManager::knownKeys(const QString &group) const
{
    if (group.isEmpty()) {
        return credentialsList().allKeys();
    }
    credentialsList().beginGroup(group);
    const auto keys = credentialsList().allKeys();
    QStringList out;
    out.reserve(keys.size());
    for (const auto &k : keys) {
        out.append(group + QLatin1Char('/') + k);
    }
    credentialsList().endGroup();
    return out;
}

/**
 * Utility function to lazily create the settings (group).
 *
 * IMPORTANT: the underlying storage is a std::unique_ptr, so do *NOT* store this reference anywhere!
 */
QSettings &CredentialManager::credentialsList() const
{
    // delayed init as scope requires a fully inizialised acc
    if (!_credentialsList) {
        _credentialsList = ConfigFile::settingsWithGroup(QStringLiteral("Credentials/") + scope(this));
    }
    return *_credentialsList;
}

CredentialJob::CredentialJob(CredentialManager *parent, const QString &key)
    : QObject(parent)
    , _key(key)
    , _parent(parent)
{
    connect(this, &CredentialJob::finished, this, &CredentialJob::deleteLater);
}

QString CredentialJob::errorString() const
{
    return _errorString;
}

const QVariant &CredentialJob::data() const
{
    return _data;
}

QKeychain::Error CredentialJob::error() const
{
    return _error;
}

void CredentialJob::start()
{
    if (!_parent->contains(_key)) {
        _error = QKeychain::EntryNotFound;
        // QKeychain is started delayed, emit the signal delayed to make sure we are connected
        qCDebug(lcCredentialsManager) << "We don't know" << _key << "skipping retrieval from keychain";
        QTimer::singleShot(0, this, &CredentialJob::finished);
        return;
    }

    _job = new QKeychain::ReadPasswordJob(Theme::instance()->appName());
    _job->setKey(scopedKey(_parent, _key));
    connect(_job, &QKeychain::ReadPasswordJob::finished, this, [this] {
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
        if (_retryOnKeyChainError && (_job->error() == QKeychain::NoBackendAvailable || _job->error() == QKeychain::OtherError)) {
            // Could be that the backend was not yet available. Wait some extra seconds.
            // (Issues #4274 and #6522)
            // (For kwallet, the error is OtherError instead of NoBackendAvailable, maybe a bug in QtKeychain)
            qCInfo(lcCredentialsManager) << "Backend unavailable (yet?) Retrying in a few seconds." << _job->errorString();
            QTimer::singleShot(10s, this, &CredentialJob::start);
            _retryOnKeyChainError = false;
        }
#endif
        OC_ASSERT(_job->error() != QKeychain::EntryNotFound);
        if (_job->error() == QKeychain::NoError) {
            QCborParserError error;
            const auto obj = QCborValue::fromCbor(_job->binaryData(), &error);
            if (error.error != QCborError::NoError) {
                _error = QKeychain::OtherError;
                _errorString = tr("Failed to parse credentials %1").arg(error.errorString());
                return;
            }
            _data = obj.toVariant();
            OC_ASSERT(_data.isValid());
        } else {
            qCWarning(lcCredentialsManager) << "Failed to get password" << scopedKey(_parent, _key) << _job->errorString();
            _error = _job->error();
            _errorString = _job->errorString();
        }
        Q_EMIT finished();
    });
    _job->start();
}

QString CredentialJob::key() const
{
    return _key;
}
