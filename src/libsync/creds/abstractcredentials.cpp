/*
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2013 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QLoggingCategory>
#include <QString>
#include <QCoreApplication>

#include "common/asserts.h"
#include "creds/abstractcredentials.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcCredentials, "nextcloud.sync.credentials", QtInfoMsg)

AbstractCredentials::AbstractCredentials() = default;

void AbstractCredentials::setAccount(Account *account)
{
    ENFORCE(!_account, "should only setAccount once");
    _account = account;
}

QString AbstractCredentials::keychainKey(const QString &url, const QString &user, const QString &accountId, const QString &appName)
{
    Q_UNUSED(appName)
    auto serverUrl = url;
    if (serverUrl.isEmpty()) {
        qCWarning(lcCredentials) << "Empty url in keyChain, error!";
        return {};
    }
    if (user.isEmpty()) {
        qCWarning(lcCredentials) << "Error: User is empty!";
        return {};
    }

    if (!serverUrl.endsWith(QChar('/'))) {
        serverUrl.append(QChar('/'));
    }

    auto key = QString(user + QLatin1Char(':') + serverUrl);
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
    const auto currentAppName = appName.isEmpty() ? QCoreApplication::applicationName()
                                                  : appName;
    key.prepend(currentAppName + "_");
#endif
    return key;
}
} // namespace OCC
