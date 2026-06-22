/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2013 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "creds/dummycredentials.h"
#include "accessmanager.h"

namespace OCC {

QString DummyCredentials::authType() const
{
    return QString::fromLatin1("dummy");
}

QString DummyCredentials::user() const
{
    return _user;
}

QString DummyCredentials::password() const
{
    Q_UNREACHABLE();
    return QString();
}

QNetworkAccessManager *DummyCredentials::createQNAM() const
{
    return new AccessManager;
}

bool DummyCredentials::ready() const
{
    return true;
}

bool DummyCredentials::stillValid(QNetworkReply *reply)
{
    Q_UNUSED(reply)
    return true;
}

void DummyCredentials::fetchFromKeychain(const QString &appName)
{
    Q_UNUSED(appName)
    _wasFetched = true;
    emit fetched();
}

void DummyCredentials::askFromUser()
{
    emit asked();
}

void DummyCredentials::persist()
{
}

} // namespace OCC
