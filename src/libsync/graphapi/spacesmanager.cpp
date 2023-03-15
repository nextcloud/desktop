/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "spacesmanager.h"

#include "drives.h"

#include "account.h"
#include "creds/abstractcredentials.h"

#include <QTimer>

#include <chrono>

using namespace std::chrono_literals;

using namespace OCC;
using namespace GraphApi;

namespace {
constexpr auto refreshTimeoutC = 30s;
}

SpacesManager::SpacesManager(Account *parent)
    : QObject(parent)
    , _account(parent)
    , _refreshTimer(new QTimer(this))
{
    _refreshTimer->setInterval(refreshTimeoutC);
    // the timer will be restarted once we received drives data
    _refreshTimer->setSingleShot(true);

    connect(_account, &Account::credentialsFetched, this, &SpacesManager::refresh);
    connect(_refreshTimer, &QTimer::timeout, this, &SpacesManager::refresh);
}

void SpacesManager::refresh()
{
    auto drivesJob = new Drives(_account->sharedFromThis(), this);
    connect(drivesJob, &Drives::finishedSignal, this, [drivesJob, this] {
        drivesJob->deleteLater();
        if (drivesJob->httpStatusCode() == 200) {
            std::unordered_map<QString, OpenAPI::OAIDrive> drivesMap;
            for (const auto &dr : drivesJob->drives()) {
                drivesMap.emplace(dr.getId(), std::move(dr));
            }
            _drivesMap = std::move(drivesMap);
        }
        Q_EMIT refreshed();
        _refreshTimer->start();
    });
    _refreshTimer->stop();
    drivesJob->start();
}

OpenAPI::OAIDrive SpacesManager::drive(const QString &id) const
{
    return _drivesMap.at(id);
}

OpenAPI::OAIDrive SpacesManager::driveByUrl(const QUrl &url) const
{
    auto it = std::find_if(_drivesMap.cbegin(), _drivesMap.cend(), [url](const auto &d) {
        return OCC::Utility::urlEqual(QUrl(d.second.getRoot().getWebDavUrl()), url);
    });
    if (it != _drivesMap.cend()) {
        return it->second;
    }
    return {};
}
