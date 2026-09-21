/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include "creds/dummycredentials.h"
#include "fixturenetworkaccessmanager.h"

namespace OCC::DocAssets
{
class FixtureCredentials : public DummyCredentials
{
public:
    FixtureCredentials()
    {
        _user = QStringLiteral("alex");
    }
    QString password() const override
    {
        return QStringLiteral("documentation-only");
    }
    QNetworkAccessManager *createQNAM() const override
    {
        return new FixtureNetworkAccessManager;
    }
};
}
