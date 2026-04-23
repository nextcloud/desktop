/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "addaccounturlhandler.h"

#include <QApplication>
#include <QCoreApplication>
#include <QUrlQuery>

#include "config.h"
#include "editlocallymanager.h"
#include "owncloudsetupwizard.h"

namespace OCC::AddAccountUrlHandler {

bool isAddAccountActionUrl(const QUrl &url)
{
    if (url.scheme() != QLatin1String(APPLICATION_URI_HANDLER_SCHEME)) {
        return false;
    }

    const auto pathParts = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    const auto hasAddAccountHost = url.host().compare(QLatin1String("addaccount"), Qt::CaseInsensitive) == 0;
    const auto hasAddAccountPath = !pathParts.isEmpty() && pathParts.constFirst().compare(QLatin1String("addaccount"), Qt::CaseInsensitive) == 0;
    return hasAddAccountHost || hasAddAccountPath;
}

QUrl parseAddAccountServerUrl(const QUrl &url)
{
    if (!isAddAccountActionUrl(url)) {
        return {};
    }

    const auto query = QUrlQuery(url);
    const auto serverUrlRaw = query.queryItemValue(QStringLiteral("server_url"));
    if (serverUrlRaw.isEmpty()) {
        return {};
    }

    const auto serverUrl = QUrl::fromUserInput(serverUrlRaw);
    if (!serverUrl.isValid() || serverUrl.host().isEmpty()) {
        return {};
    }
    if (serverUrl.scheme() != QLatin1String("https") && serverUrl.scheme() != QLatin1String("http")) {
        return {};
    }

    return serverUrl;
}

bool handleAddAccountUrl(const QUrl &url)
{
    if (!isAddAccountActionUrl(url)) {
        return false;
    }

    const auto serverUrl = parseAddAccountServerUrl(url);
    if (serverUrl.isValid()) {
        OwncloudSetupWizard::runWizardForLoginFlow(serverUrl, qApp, SLOT(slotownCloudWizardDone(int)));
    } else {
        EditLocallyManager::showError(QCoreApplication::translate("Application", "Invalid account setup URL"),
                                      QCoreApplication::translate("Application", "The provided addAccount URL could not be parsed."));
    }

    return true;
}

}
