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

#include <QApplication>
#include <QLoggingCategory>
#include <QSystemTrayIcon>
#include <QUrlQuery>

namespace OCC {

Q_LOGGING_CATEGORY(lcUriSchemeHandler, "nextcloud.gui.urischemehandler", QtInfoMsg)

namespace {

constexpr auto openAction = "open";
constexpr auto addAccountAction = "addAccount";
constexpr auto serverUrlQueryItem = "server_url";

[[nodiscard]] bool isValidServerUrl(const QUrl &serverUrl)
{
    return serverUrl.isValid()
        && !serverUrl.isRelative()
        && !serverUrl.host().isEmpty()
        && (serverUrl.scheme() == QStringLiteral("http") || serverUrl.scheme() == QStringLiteral("https"));
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

    const auto action = url.host();
    if (action == QLatin1String(openAction)) {
        result.action = Action::OpenLocalEdit;
        return result;
    }

    if (action != QLatin1String(addAccountAction)) {
        result.error = QStringLiteral("The supplied URL action is not supported.");
        return result;
    }

    if (url.path() != QStringLiteral("/")) {
        result.error = QStringLiteral("The add account URL must use the nc://addAccount/ path.");
        return result;
    }

    const auto query = QUrlQuery{url};
    if (!query.hasQueryItem(QLatin1String(serverUrlQueryItem))) {
        result.error = QStringLiteral("The add account URL is missing the server_url query item.");
        return result;
    }

    const auto serverUrl = QUrl{query.queryItemValue(QLatin1String(serverUrlQueryItem))};
    if (!isValidServerUrl(serverUrl)) {
        result.error = QStringLiteral("The add account URL contains an invalid server_url query item.");
        return result;
    }

    result.action = Action::AddAccount;
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
    case Action::AddAccount:
#if defined ENFORCE_SINGLE_ACCOUNT
        if (!AccountManager::instance()->accounts().isEmpty()) {
            showWarning(QApplication::translate("UriSchemeHandler", "Adding another account is not allowed in this client."));
            return false;
        }
#endif
        OwncloudSetupWizard::runWizardForLoginFlow(qApp, SLOT(slotownCloudWizardDone(int)), parsedUri.serverUrl);
        return true;
    case Action::Invalid:
        showWarning(parsedUri.error);
        return false;
    }

    Q_UNREACHABLE();
}

}
