/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "urischemehandler.h"

#include "accountmanager.h"
#include "config.h"
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
#include <QStringList>
#include <QSystemTrayIcon>
#include <QUrlQuery>

namespace OCC {

Q_LOGGING_CATEGORY(lcUriSchemeHandler, "nextcloud.gui.urischemehandler", QtInfoMsg)

namespace {

constexpr auto openAction = "open";
constexpr auto loginAction = "login";
constexpr auto loginServerPathPrefix = "/server:";

[[nodiscard]] QString describeUriForLog(const QUrl &url)
{
    const auto query = QUrlQuery{url};
    QStringList queryItemNames;
    const auto queryItems = query.queryItems();
    for (const auto &queryItem : queryItems) {
        queryItemNames.append(queryItem.first);
    }

    return QStringLiteral("scheme=%1 host=%2 path=%3 queryItems=%4")
        .arg(url.scheme(),
             url.host(),
             url.path(),
             queryItemNames.join(QLatin1Char(',')));
}

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

[[nodiscard]] QList<QUrl> configuredOverrideServerUrls()
{
    const auto theme = Theme::instance();
    auto serverUrls = QList<QUrl>{};

    if (theme->multipleOverrideServers()) {
        const auto serversJsonArray = QJsonDocument::fromJson(theme->overrideServerUrl().toUtf8()).array();
        for (const auto &serverJson : serversJsonArray) {
            const auto serverUrl = QUrl{serverJson.toObject().value(QStringLiteral("url")).toString()};
            if (isValidServerUrl(serverUrl)) {
                serverUrls.append(serverUrl);
            }
        }
        return serverUrls;
    }

    const auto serverUrl = QUrl{theme->overrideServerUrl()};
    if (isValidServerUrl(serverUrl)) {
        serverUrls.append(serverUrl);
    }

    return serverUrls;
}

[[nodiscard]] bool isAllowedByConfiguredServerOverride(const QUrl &serverUrl)
{
    const auto theme = Theme::instance();
    if (theme->overrideServerUrl().isEmpty()
        || (!theme->forceOverrideServerUrl() && !theme->multipleOverrideServers())) {
        return true;
    }

    const auto normalizedServerUrl = normalizedServerUrlForComparison(serverUrl);
    const auto allowedServerUrls = configuredOverrideServerUrls();
    for (const auto &allowedServerUrl : allowedServerUrls) {
        if (normalizedServerUrlForComparison(allowedServerUrl) == normalizedServerUrl) {
            return true;
        }
    }

    return false;
}

void showWarning(const QString &message)
{
    qCWarning(lcUriSchemeHandler) << message;

    if (auto systray = Systray::instance()) {
        systray->showMessage(QApplication::translate("UriSchemeHandler", "Could not handle link"),
                             message,
                             QSystemTrayIcon::Warning);
    }
}

}

UriSchemeHandler::ParsedUri UriSchemeHandler::parseUri(const QUrl &url)
{
    qCInfo(lcUriSchemeHandler) << "Parsing URI scheme request:" << describeUriForLog(url);

    ParsedUri result;
    result.originalUrl = url;

    if (!url.isValid()) {
        result.error = QStringLiteral("The supplied URL is invalid.");
        qCWarning(lcUriSchemeHandler) << "Rejected URI scheme request:" << result.error << describeUriForLog(url);
        return result;
    }

    if (url.scheme() != QStringLiteral(APPLICATION_URI_HANDLER_SCHEME)) {
        result.error = QStringLiteral("The supplied URL does not use the supported scheme.");
        qCWarning(lcUriSchemeHandler) << "Rejected URI scheme request:" << result.error << describeUriForLog(url);
        return result;
    }

    const auto action = url.host().toCaseFolded();
    if (action == QLatin1String(openAction)) {
        result.action = Action::OpenLocalEdit;
        qCInfo(lcUriSchemeHandler) << "Accepted URI scheme request for local edit:" << describeUriForLog(url);
        return result;
    }

    if (action != QLatin1String(loginAction)) {
        result.error = QStringLiteral("The supplied URL action is not supported.");
        qCWarning(lcUriSchemeHandler) << "Rejected URI scheme request:" << result.error << describeUriForLog(url);
        return result;
    }

    const auto path = url.path(QUrl::FullyDecoded);
    if (!path.startsWith(QLatin1String(loginServerPathPrefix))) {
        result.error = QStringLiteral("The login URL must use the nc://login/server:{server} path.");
        qCWarning(lcUriSchemeHandler) << "Rejected URI scheme request:" << result.error << describeUriForLog(url);
        return result;
    }

    const auto serverUrlValue = path.mid(QString::fromLatin1(loginServerPathPrefix).size());
    const auto serverUrl = QUrl{serverUrlValue};
    qCInfo(lcUriSchemeHandler) << "Decoded login server URL:"
                               << "isValid=" << serverUrl.isValid()
                               << "isRelative=" << serverUrl.isRelative()
                               << "scheme=" << serverUrl.scheme()
                               << "host=" << serverUrl.host()
                               << "path=" << serverUrl.path();
    if (!isValidServerUrl(serverUrl)) {
        result.error = QStringLiteral("The login URL contains an invalid server URL.");
        qCWarning(lcUriSchemeHandler) << "Rejected URI scheme request:" << result.error << describeUriForLog(url);
        return result;
    }

    if (!isAllowedByConfiguredServerOverride(serverUrl)) {
        result.error = QStringLiteral("The login URL is not allowed by the configured server restriction.");
        qCWarning(lcUriSchemeHandler) << "Rejected URI scheme request:" << result.error << describeUriForLog(url);
        return result;
    }

    result.action = Action::Login;
    result.serverUrl = serverUrl;
    qCInfo(lcUriSchemeHandler) << "Accepted URI scheme request to log in to server:"
                               << serverUrl.toString(QUrl::RemoveUserInfo | QUrl::RemoveQuery | QUrl::RemoveFragment);
    return result;
}

bool UriSchemeHandler::handleUri(const QUrl &url)
{
    qCInfo(lcUriSchemeHandler) << "Handling URI scheme request:" << describeUriForLog(url);

    const auto parsedUri = parseUri(url);
    switch (parsedUri.action) {
    case Action::OpenLocalEdit:
        qCInfo(lcUriSchemeHandler) << "Dispatching URI scheme request to local edit manager.";
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
        qCInfo(lcUriSchemeHandler) << "Dispatching URI scheme request to quick account setup.";
        OwncloudSetupWizard::runWizardForLoginFlow(qApp, SLOT(slotownCloudWizardDone(int)), parsedUri.serverUrl);
        return true;
    case Action::Invalid:
        qCWarning(lcUriSchemeHandler) << "URI scheme request was not dispatched:" << parsedUri.error;
        showWarning(parsedUri.error);
        return false;
    }

    Q_UNREACHABLE();
}

}
