/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "urischemehandler.h"

#include "accountmanager.h"
#include "config.h"
#include "configfile.h"
#include "editlocallymanager.h"
#include "owncloudsetupwizard.h"
#include "systray.h"
#include "theme.h"

#include <QApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QLoggingCategory>
#include <QSystemTrayIcon>

namespace OCC {

Q_LOGGING_CATEGORY(lcUriSchemeHandler, "nextcloud.gui.urischemehandler", QtInfoMsg)

namespace {

constexpr auto openAction = "open";
constexpr auto loginAction = "login";
constexpr auto loginServerPathPrefix = "/server:";

[[nodiscard]] bool isValidServerUrl(const QUrl &serverUrl)
{
    return serverUrl.isValid()
        && !serverUrl.isRelative()
        && !serverUrl.host().isEmpty()
        && (serverUrl.scheme() == QStringLiteral("http") || serverUrl.scheme() == QStringLiteral("https"));
}

[[nodiscard]] QUrl normalizedServerUrlForComparison(const QUrl &serverUrl)
{
    auto normalizedUrl = serverUrl.adjusted(QUrl::NormalizePathSegments
                                            | QUrl::StripTrailingSlash
                                            | QUrl::RemoveUserInfo
                                            | QUrl::RemoveQuery
                                            | QUrl::RemoveFragment);
    normalizedUrl.setScheme(normalizedUrl.scheme().toCaseFolded());
    normalizedUrl.setHost(normalizedUrl.host().toCaseFolded());

    if ((normalizedUrl.scheme() == QStringLiteral("http") && normalizedUrl.port() == 80)
        || (normalizedUrl.scheme() == QStringLiteral("https") && normalizedUrl.port() == 443)) {
        normalizedUrl.setPort(-1);
    }
    if (normalizedUrl.path() == QStringLiteral("/")) {
        normalizedUrl.setPath(QString{});
    }

    return normalizedUrl;
}

[[nodiscard]] QList<QUrl> configuredOverrideServerUrls(const QString &overrideServerUrl)
{
    auto serverUrls = QList<QUrl>{};

    QJsonParseError jsonParseError;
    const auto serversJsonDocument = QJsonDocument::fromJson(overrideServerUrl.toUtf8(), &jsonParseError);
    if (jsonParseError.error == QJsonParseError::NoError && serversJsonDocument.isArray()) {
        const auto serversJsonArray = serversJsonDocument.array();
        for (const auto &serverJson : serversJsonArray) {
            const auto serverUrl = QUrl{serverJson.toObject().value(QStringLiteral("url")).toString()};
            if (isValidServerUrl(serverUrl)) {
                serverUrls.append(serverUrl);
            }
        }
        return serverUrls;
    }

    const auto serverUrl = QUrl{overrideServerUrl};
    if (isValidServerUrl(serverUrl)) {
        serverUrls.append(serverUrl);
    }

    return serverUrls;
}

[[nodiscard]] bool isAllowedByConfiguredServerOverride(const QUrl &serverUrl)
{
    const auto configuredOverrideServerUrl = ConfigFile{}.overrideServerUrl();
    if (!configuredOverrideServerUrl.isEmpty()) {
        const auto normalizedServerUrl = normalizedServerUrlForComparison(serverUrl);
        const auto allowedServerUrls = configuredOverrideServerUrls(configuredOverrideServerUrl);
        for (const auto &allowedServerUrl : allowedServerUrls) {
            if (normalizedServerUrlForComparison(allowedServerUrl) == normalizedServerUrl) {
                return true;
            }
        }

        return false;
    }

    const auto theme = Theme::instance();
    if (theme->overrideServerUrl().isEmpty()
        || !theme->forceOverrideServerUrl()) {
        return true;
    }

    const auto normalizedServerUrl = normalizedServerUrlForComparison(serverUrl);
    const auto allowedServerUrls = configuredOverrideServerUrls(theme->overrideServerUrl());
    for (const auto &allowedServerUrl : allowedServerUrls) {
        if (normalizedServerUrlForComparison(allowedServerUrl) == normalizedServerUrl) {
            return true;
        }
    }

    return false;
}

void showWarning(const QString &message)
{
    if (auto systray = Systray::instance()) {
        systray->showMessage(QApplication::translate("UriSchemeHandler", "Could not handle link"),
                             message,
                             QSystemTrayIcon::Warning);
    }
}

}

UriSchemeHandler::ParsedUri UriSchemeHandler::parseUri(const QUrl &url)
{
    ParsedUri result;
    result.originalUrl = url;

    if (!url.isValid()) {
        result.error = QStringLiteral("The supplied URL is invalid.");
        return result;
    }

    if (url.scheme() != QStringLiteral(APPLICATION_URI_HANDLER_SCHEME)) {
        result.error = QStringLiteral("The supplied URL does not use the supported scheme.");
        return result;
    }

    const auto action = url.host().toCaseFolded();
    if (action == QLatin1String(openAction)) {
        result.action = Action::OpenLocalEdit;
        return result;
    }

    if (action != QLatin1String(loginAction)) {
        result.error = QStringLiteral("The supplied URL action is not supported.");
        return result;
    }

    const auto path = url.path(QUrl::FullyDecoded);
    if (!path.startsWith(QLatin1String(loginServerPathPrefix))) {
        result.error = QStringLiteral("The login URL must use the nc://login/server:{server} path.");
        return result;
    }

    const auto serverUrlValue = path.mid(QString::fromLatin1(loginServerPathPrefix).size());
    const auto serverUrl = QUrl{serverUrlValue};
    if (!isValidServerUrl(serverUrl)) {
        result.error = QStringLiteral("The login URL contains an invalid server URL.");
        return result;
    }

    if (!isAllowedByConfiguredServerOverride(serverUrl)) {
        result.error = QStringLiteral("The login URL is not allowed by the configured server restriction.");
        return result;
    }

    result.action = Action::Login;
    result.serverUrl = serverUrl;
    return result;
}

bool UriSchemeHandler::handleUri(const QUrl &url)
{
    const auto parsedUri = parseUri(url);
    switch (parsedUri.action) {
    case Action::OpenLocalEdit:
        EditLocallyManager::instance()->handleRequest(parsedUri.originalUrl);
        return true;
    case Action::Login:
#if defined ENFORCE_SINGLE_ACCOUNT
        if (!AccountManager::instance()->accounts().isEmpty()) {
            qCWarning(lcUriSchemeHandler) << "Rejected login URI scheme request because this client enforces a single account.";
            showWarning(QApplication::translate("UriSchemeHandler", "Adding another account is not allowed in this client."));
            return false;
        }
#endif
        OwncloudSetupWizard::runWizardForLoginFlow(qApp, SLOT(slotownCloudWizardDone(int)), parsedUri.serverUrl);
        return true;
    case Action::Invalid:
        qCWarning(lcUriSchemeHandler) << "Could not handle URI scheme request:" << parsedUri.error;
        showWarning(parsedUri.error);
        return false;
    }

    Q_UNREACHABLE();
}

}
