#pragma once

#include "httpcredentials.h"
#include "common/depreaction.h"

#include "account.h"
#include "configfile.h"
#include "theme.h"

#include <QApplication>
#include <QLoggingCategory>

#include <qt5keychain/keychain.h>

using namespace std::chrono_literals;

OC_DISABLE_DEPRECATED_WARNING

Q_DECLARE_LOGGING_CATEGORY(lcHttpLegacyCredentials)

namespace {
void addSettingsToJob(QKeychain::Job *job)
{
    auto settings = OCC::ConfigFile::settingsWithGroup(OCC::Theme::instance()->appName());
    settings->setParent(job); // make the job parent to make setting deleted properly
    job->setSettings(settings.release());
}


QString keychainKey(const QString &url, const QString &user, const QString &accountId)
{
    QString u(url);
    if (u.isEmpty()) {
        qCWarning(lcHttpLegacyCredentials) << "Empty url in keyChain, error!";
        return {};
    }
    if (user.isEmpty()) {
        qCWarning(lcHttpLegacyCredentials) << "Error: User is empty!";
        return {};
    }

    if (!u.endsWith(QLatin1Char('/'))) {
        u.append(QLatin1Char('/'));
    }

    QString key = user + QLatin1Char(':') + u;
    if (!accountId.isEmpty()) {
        key += QLatin1Char(':') + accountId;
    }
#ifdef Q_OS_WIN
    // On Windows the credential keys aren't namespaced properly
    // by qtkeychain. To work around that we manually add namespacing
    // to the generated keys. See #6125.
    // It's safe to do that since the key format is changing for 2.4
    // anyway to include the account ids. That means old keys can be
    // migrated to new namespaced keys on windows for 2.4.
    key.prepend(OCC::Theme::instance()->appNameGUI() + QLatin1Char('_'));
#endif
    return key;
}
} // ns

namespace OCC {

class HttpLegacyCredentials : public QObject
{
    Q_OBJECT

    auto isOAuthC()
    {
        return QStringLiteral("oauth");
    }

public:
    HttpLegacyCredentials(HttpCredentials *parent)
        : QObject(parent)
        , _parent(parent)
    {
    }

    void fetchFromKeychainHelper()
    {
        qCInfo(lcHttpLegacyCredentials) << "Started migration of < 2.8 credentials to 2.9+";
        slotReadPasswordFromKeychain();
    }

private:
    HttpCredentials *_parent;
    bool _retryOnKeyChainError = true;

    bool _keychainMigration = false;

    bool keychainUnavailableRetryLater(QKeychain::ReadPasswordJob *incoming)
    {
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
        Q_ASSERT(!incoming->insecureFallback()); // If insecureFallback is set, the next test would be pointless
        if (_retryOnKeyChainError && (incoming->error() == QKeychain::NoBackendAvailable || incoming->error() == QKeychain::OtherError)) {
            // Could be that the backend was not yet available. Wait some extra seconds.
            // (Issues #4274 and #6522)
            // (For kwallet, the error is OtherError instead of NoBackendAvailable, maybe a bug in QtKeychain)
            qCInfo(lcHttpLegacyCredentials) << "Backend unavailable (yet?) Retrying in a few seconds." << incoming->errorString();
            QTimer::singleShot(10s, this, &HttpLegacyCredentials::fetchFromKeychainHelper);
            _retryOnKeyChainError = false;
            return true;
        }
#else
        Q_UNUSED(incoming);
#endif
        _retryOnKeyChainError = false;
        return false;
    }

    void slotReadPasswordFromKeychain()
    {
        const QString kck = keychainKey(
            _parent->_account->url().toString(),
            _parent->_user,
            _keychainMigration ? QString() : _parent->_account->id());

        QKeychain::ReadPasswordJob *job = new QKeychain::ReadPasswordJob(Theme::instance()->appName());
        addSettingsToJob(job);
        job->setInsecureFallback(false);
        job->setKey(kck);
        connect(job, &QKeychain::ReadPasswordJob::finished, this, &HttpLegacyCredentials::slotReadJobDone);
        job->start();
    }


    void slotReadJobDone(QKeychain::Job *incoming)
    {
        Q_ASSERT(!_parent->_user.isEmpty());
        auto job = qobject_cast<QKeychain::ReadPasswordJob *>(incoming);
        QKeychain::Error error = job->error();

        // If we can't find the credentials at the keys that include the account id,
        // try to read them from the legacy locations that don't have a account id.
        if (!_keychainMigration && error == QKeychain::EntryNotFound) {
            qCWarning(lcHttpLegacyCredentials)
                << "Could not find keychain entries, attempting to read from legacy locations";
            _keychainMigration = true;
            fetchFromKeychainHelper();
            return;
        }

        const auto data = job->textData();
        if (error != QKeychain::NoError || data.isEmpty()) {
            // we come here if the password is empty or any other keychain
            // error happend.
            qCWarning(lcHttpLegacyCredentials) << "Migrating old keychain entries failed" << job->errorString();
            _parent->_fetchErrorString = job->error() != QKeychain::EntryNotFound ? job->errorString() : QString();

            _parent->_password.clear();
            _parent->_ready = false;
            emit _parent->fetched();
        } else {
            qCWarning(lcHttpLegacyCredentials) << "Migrated old keychain entries";
            if (_parent->_account->credentialSetting(isOAuthC()).toBool()) {
                _parent->_refreshToken = data;
                _parent->refreshAccessToken();
            } else {
                // All cool, the keychain did not come back with error.
                // Still, the password can be empty which indicates a problem and
                // the password dialog has to be opened.
                _parent->_password = data;
                _parent->_ready = true;
                emit _parent->fetched();
            }
        }
        // If keychain data was read from legacy location, wipe these entries and store new ones
        deleteOldKeychainEntries();
        if (_parent->_ready) {
            _parent->persist();
            qCWarning(lcHttpLegacyCredentials) << "Migrated old keychain entries";
        }
        deleteLater();
    }

    void slotWriteJobDone(QKeychain::Job *job)
    {
        if (job && job->error() != QKeychain::NoError) {
            qCWarning(lcHttpLegacyCredentials) << "Error while writing password"
                                               << job->error() << job->errorString();
        }
    }

    void deleteOldKeychainEntries()
    {
        // oooold
        auto startDeleteJob = [this](const QString &key, bool migratePreId = false) {
            QKeychain::DeletePasswordJob *job = new QKeychain::DeletePasswordJob(Theme::instance()->appName());
            addSettingsToJob(job);
            job->setInsecureFallback(true);
            job->setKey(keychainKey(_parent->_account->url().toString(), key, migratePreId ? QString() : _parent->_account->id()));
            job->start();
            connect(job, &QKeychain::DeletePasswordJob::finished, this, [job] {
                if (job->error() != QKeychain::NoError) {
                    qCWarning(lcHttpLegacyCredentials) << "Failed to delete legacy credentials" << job->key() << job->errorString();
                }
            });
        };

        // old
        startDeleteJob(_parent->_user, true);
        // pre 2.8
        startDeleteJob(_parent->_user);
    }
};

}

OC_ENABLE_DEPRECATED_WARNING
