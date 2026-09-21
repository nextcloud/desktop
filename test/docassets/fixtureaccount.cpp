/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fixtureaccount.h"
#include "account.h"
#include "capturescene.h"
#include "fixturecredentials.h"
#include "testhelper.h"

namespace OCC::DocAssets
{
void prepareAccount(CaptureScene &scene)
{
    scene.account = Account::create();
    scene.account->setUrl(QUrl(QStringLiteral("https://cloud.example.com")));
    scene.account->setCredentials(new FixtureCredentials);
    scene.account->setDavUser(QStringLiteral("alex"));
    scene.account->setDavDisplayName(QStringLiteral("Alex Morgan"));
    scene.account->setServerVersion(QStringLiteral("33.0.0"));
    scene.account->setCapabilities(
        {{QStringLiteral("assistant"), QVariantMap{{QStringLiteral("enabled"), true}, {QStringLiteral("version"), QStringLiteral("1.0.9")}}}});
    scene.accountState.reset(new FakeAccountState(scene.account));
}
}
