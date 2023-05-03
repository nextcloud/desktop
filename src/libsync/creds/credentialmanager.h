#pragma once

#include <QSettings>
#include <QVariant>

#include "owncloudlib.h"

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <qt5keychain/keychain.h>
#else
#include <qt6keychain/keychain.h>
#endif

#include <memory>

namespace OCC {
class Account;
class CredentialJob;

class OWNCLOUDSYNC_EXPORT CredentialManager : public QObject
{
    Q_OBJECT
public:
    // global credentials
    CredentialManager(QObject *parent);
    // account related credentials
    explicit CredentialManager(Account *acc);

    CredentialJob *get(const QString &key);
    QKeychain::Job *set(const QString &key, const QVariant &data);
    QKeychain::Job *remove(const QString &key);
    /**
     * Delete all credentials asigned with an account
     */
    QVector<QPointer<QKeychain::Job>> clear(const QString &group = {});

    bool contains(const QString &key) const;
    const Account *account() const;

private:
    QSettings &credentialsList() const;

    // TestCredentialManager
    QStringList knownKeys(const QString &group = {}) const;

    const Account *const _account = nullptr;
    mutable std::unique_ptr<QSettings> _credentialsList;

    friend class TestCredentialManager;
};

class OWNCLOUDSYNC_EXPORT CredentialJob : public QObject
{
    Q_OBJECT
public:
    QString key() const;

    QKeychain::Error error() const;

    const QVariant &data() const;

    QString errorString() const;

Q_SIGNALS:
    void finished();

private:
    CredentialJob(CredentialManager *parent, const QString &key);
    void start();

    QString _key;
    QVariant _data;
    QKeychain::Error _error = QKeychain::NoError;
    QString _errorString;
    bool _retryOnKeyChainError = true;
    QKeychain::ReadPasswordJob *_job;

    CredentialManager *const _parent;

    friend class CredentialManager;
    friend class TestCredentialManager;
};


}
