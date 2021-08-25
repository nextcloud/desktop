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

Q_LOGGING_CATEGORY(lcCredentaislManager, "sync.credentials.manager", QtDebugMsg)

namespace {
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
    qCInfo(lcCredentaislManager) << "get" << scopedKey(this, key);
    auto out = new CredentialJob(this, key);
    out->start();
    return out;
}

QKeychain::Job *CredentialManager::set(const QString &key, const QVariant &data)
{
    qCInfo(lcCredentaislManager) << "set" << scopedKey(this, key);
    auto writeJob = new QKeychain::WritePasswordJob(Theme::instance()->appName());
    writeJob->setKey(scopedKey(this, key));
    connect(writeJob, &QKeychain::WritePasswordJob::finished, this, [writeJob, key, this] {
        if (writeJob->error() == QKeychain::NoError) {
            qCInfo(lcCredentaislManager) << "added" << scopedKey(this, key);
            // just a list, the values don't matter
            credentialsList().setValue(key, true);
        } else {
            qCWarning(lcCredentaislManager) << "Failed to set:" << scopedKey(this, key) << writeJob->errorString();
        }
    });
    writeJob->setBinaryData(QCborValue::fromVariant(data).toCbor());
    // start is delayed so we can directly call it
    writeJob->start();
    return writeJob;
}

QKeychain::Job *CredentialManager::remove(const QString &key)
{
    OC_ASSERT(contains(key));
    // remove immediately to prevent double invocation by clear()
    credentialsList().remove(key);
    qCInfo(lcCredentaislManager) << "del" << scopedKey(this, key);
    auto keychainJob = new QKeychain::DeletePasswordJob(Theme::instance()->appName());
    keychainJob->setKey(scopedKey(this, key));
    connect(keychainJob, &QKeychain::DeletePasswordJob::finished, this, [keychainJob, key, this] {
        OC_ASSERT(keychainJob->error() != QKeychain::EntryNotFound);
        if (keychainJob->error() == QKeychain::NoError) {
            qCInfo(lcCredentaislManager) << "removed" << scopedKey(this, key);
        } else {
            qCWarning(lcCredentaislManager) << "Failed to remove:" << scopedKey(this, key) << keychainJob->errorString();
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
            qCInfo(lcCredentaislManager) << "Backend unavailable (yet?) Retrying in a few seconds." << _job->errorString();
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
            qCWarning(lcCredentaislManager) << "Failed to read client id" << _job->errorString();
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
