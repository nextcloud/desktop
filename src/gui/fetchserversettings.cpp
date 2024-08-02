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

#include "fetchserversettings.h"

#include "gui/accountstate.h"
#include "gui/connectionvalidator.h"

#include "libsync/networkjobs/jsonjob.h"

using namespace std::chrono_literals;

using namespace OCC;


Q_LOGGING_CATEGORY(lcfetchserversettings, "sync.fetchserversettings", QtInfoMsg)

namespace {
auto fetchSettingsTimeout()
{
    return std::min(20s, AbstractNetworkJob::httpTimeout);
}
}

// TODO: move to libsync?
FetchServerSettingsJob::FetchServerSettingsJob(const OCC::AccountPtr &account, QObject *parent)
    : QObject(parent)
    , _account(account)
{
}


void FetchServerSettingsJob::start()
{
    // The main flow now needs the capabilities
    auto *job = new JsonApiJob(_account, QStringLiteral("ocs/v2.php/cloud/capabilities"), {}, {}, this);
    job->setAuthenticationJob(isAuthJob());
    job->setTimeout(fetchSettingsTimeout());

    connect(job, &JsonApiJob::finishedSignal, this, [job, this] {
        auto caps =
            job->data().value(QStringLiteral("ocs")).toObject().value(QStringLiteral("data")).toObject().value(QStringLiteral("capabilities")).toObject();
        qCInfo(lcfetchserversettings) << "Server capabilities" << caps;
        if (job->ocsSuccess()) {
            // Record that the server supports HTTP/2
            // Actual decision if we should use HTTP/2 is done in AccessManager::createRequest
            // TODO: http2 support is currently disabled in the client code
            if (auto reply = job->reply()) {
                _account->setHttp2Supported(reply->attribute(QNetworkRequest::Http2WasUsedAttribute).toBool());
            }
            _account->setCapabilities({_account->url(), caps.toVariantMap()});
            // We cannot deal with servers < 10.0.0
            switch (_account->serverSupportLevel()) {
            case Account::ServerSupportLevel::Unknown:
                [[fallthrough]];
            case Account::ServerSupportLevel::Supported:
                break;
            case Account::ServerSupportLevel::Unsupported:
                Q_EMIT finishedSignal(Result::UnsupportedServer);
                return;
            }
            auto *userJob = new JsonApiJob(_account, QStringLiteral("ocs/v2.php/cloud/user"), SimpleNetworkJob::UrlQuery{}, QNetworkRequest{}, this);
            userJob->setAuthenticationJob(isAuthJob());
            userJob->setTimeout(fetchSettingsTimeout());
            connect(userJob, &JsonApiJob::finishedSignal, this, [userJob, this] {
                if (userJob->timedOut()) {
                    Q_EMIT finishedSignal(Result::TimeOut);
                } else if (userJob->httpStatusCode() == 401) {
                    Q_EMIT finishedSignal(Result::InvalidCredentials);
                } else if (userJob->ocsSuccess()) {
                    const auto userData = userJob->data().value(QStringLiteral("ocs")).toObject().value(QStringLiteral("data")).toObject();
                    const QString user = userData.value(QStringLiteral("id")).toString();
                    if (!user.isEmpty()) {
                        _account->setDavUser(user);
                    }
                    const QString displayName = userData.value(QStringLiteral("display-name")).toString();
                    if (!displayName.isEmpty()) {
                        _account->setDavDisplayName(displayName);
                    }
                    runAsyncUpdates();
                    Q_EMIT finishedSignal(Result::Success);
                } else {
                    Q_EMIT finishedSignal(Result::Undefined);
                }
            });
            userJob->start();
        } else {
            if (job->timedOut()) {
                Q_EMIT finishedSignal(Result::TimeOut);
            } else if (job->httpStatusCode() == 401) {
                Q_EMIT finishedSignal(Result::InvalidCredentials);
            } else {
                Q_EMIT finishedSignal(Result::Undefined);
            }
        }
    });
    job->start();
}

void FetchServerSettingsJob::runAsyncUpdates()
{
    // those jobs are:
    // - never auth jobs
    // - might get queued
    // - have the default timeout
    // - must not be parented by this object

    // ideally we would parent them to the account, but as things are messed up by the shared pointer stuff we can't at the moment
    // so we just set them free
    if (!_account->capabilities().spacesSupport().enabled && _account->capabilities().avatarsAvailable()) {
        // the avatar job uses the legacy webdav url and ocis will require a new approach
        auto *avatarJob = new AvatarJob(_account, _account->davUser(), 128, nullptr);
        connect(avatarJob, &AvatarJob::avatarPixmap, this, [this](const QPixmap &img) { _account->setAvatar(AvatarJob::makeCircularAvatar(img)); });
        avatarJob->start();
    };

    if (_account->capabilities().appProviders().enabled) {
        auto *jsonJob = new JsonJob(_account, _account->capabilities().appProviders().appsUrl, {}, "GET");
        connect(jsonJob, &JsonJob::finishedSignal, this, [jsonJob, this] { _account->setAppProvider(AppProvider{jsonJob->data()}); });
        jsonJob->start();
    }
}

bool FetchServerSettingsJob::isAuthJob() const
{
    return qobject_cast<ConnectionValidator *>(parent());
}
