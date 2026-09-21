/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "sharingfixtures.h"

#include "account.h"
#include "capturescene.h"
#include "fixtureaccount.h"
#include "fixturenetworkaccessmanager.h"
#include "sharing/sharingconstants.h"
#include "sharing/sharingcontroller.h"
#include "sharing/unifiedshare.h"

#include <QAbstractItemModel>
#include <QQuickWindow>
#include <QUrlQuery>

namespace OCC::DocAssets
{
namespace
{
const auto fileId = QStringLiteral("42");

QByteArray sharesResponse()
{
    return QByteArrayLiteral(R"({
        "ocs": {
            "meta": {"status": "ok", "statuscode": 200, "message": "OK"},
            "data": [
                {
                    "id": "share-team",
                    "state": "active",
                    "permission_preset": "OC\\Core\\Sharing\\Permission\\EditSharePermissionPreset",
                    "permissions": [
                        {"class": "view", "display_name": "View files", "enabled": true},
                        {"class": "edit", "display_name": "Edit files", "enabled": true},
                        {"class": "download", "display_name": "Download files", "enabled": true}
                    ],
                    "properties": [
                        {
                            "class": "OC\\Core\\Sharing\\Property\\NoteProperty",
                            "display_name": "Note to recipients",
                            "priority": 20,
                            "type": "string",
                            "advanced": false,
                            "required": false,
                            "value": "Please review the project plan before Friday."
                        },
                        {
                            "class": "OC\\Core\\Sharing\\Property\\ExpirationProperty",
                            "display_name": "Expiration date",
                            "priority": 20,
                            "type": "date",
                            "advanced": true,
                            "required": false,
                            "hint": "YYYY-MM-DD",
                            "value": "2026-10-31"
                        },
                        {
                            "class": "OC\\Core\\Sharing\\Property\\PasswordProperty",
                            "display_name": "Password protection",
                            "priority": 10,
                            "type": "password",
                            "advanced": true,
                            "required": false,
                            "value": "documentation"
                        }
                    ],
                    "recipients": [
                        {
                            "class": "OC\\Core\\Sharing\\Recipient\\UserShareRecipientType",
                            "display_name": "Alice Morgan",
                            "value": "alice",
                            "initiator": {"display_name": "Alex Morgan"},
                            "permissions": [
                                {"class": "view", "display_name": "View files", "enabled": true},
                                {"class": "edit", "display_name": "Edit files", "enabled": true},
                                {"class": "download", "display_name": "Download files", "enabled": true}
                            ]
                        },
                        {
                            "class": "OC\\Core\\Sharing\\Recipient\\GroupShareRecipientType",
                            "display_name": "Design team",
                            "value": "design-team",
                            "permissions": [
                                {"class": "view", "display_name": "View files", "enabled": true},
                                {"class": "edit", "display_name": "Edit files", "enabled": true},
                                {"class": "download", "display_name": "Download files", "enabled": true}
                            ]
                        }
                    ]
                },
                {
                    "id": "share-public",
                    "state": "active",
                    "permission_preset": "OC\\Core\\Sharing\\Permission\\ViewSharePermissionPreset",
                    "recipients": [
                        {
                            "class": "OC\\Core\\Sharing\\Recipient\\TokenShareRecipientType",
                            "display_name": "Public link",
                            "value": "public",
                            "secret": {"url": "https://cloud.example.com/s/project-plan", "updatable": true}
                        }
                    ]
                }
            ]
        }
    })");
}

QUrl sharesRequestUrl()
{
    auto url = QUrl(QStringLiteral("https://cloud.example.com/ocs/v2.php/apps/sharing/api/v1/shares"));
    auto query = QUrlQuery{};
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("100"));
    query.addQueryItem(QStringLiteral("filterSourceTypeClass"), QString{Gui::Sharing::SourceTypeClasses::node});
    query.addQueryItem(QStringLiteral("filterSourceTypeValue"), fileId);
    query.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    url.setQuery(query);
    return url;
}

Gui::Sharing::SharingController *sharingController(QQuickWindow *window)
{
    return window ? window->findChild<Gui::Sharing::SharingController *>(QStringLiteral("sharingController")) : nullptr;
}
}

bool prepareSharing(CaptureScene &scene, const QString &scenario, QString *error)
{
    const auto overview = scenario == QStringLiteral("sharing-overview");
    const auto details = scenario == QStringLiteral("sharing-details");
    const auto advanced = scenario == QStringLiteral("sharing-advanced-settings");
    if (!overview && !details && !advanced) {
        *error = QStringLiteral("Unsupported Sharing scenario: %1").arg(scenario);
        return false;
    }

    prepareAccount(scene);
    auto *network = static_cast<FixtureNetworkAccessManager *>(scene.account->networkAccessManager());
    network->getResponses.insert(sharesRequestUrl(), sharesResponse());
    scene.module = QStringLiteral("com.nextcloud.desktopclient.sharing");
    scene.type = QStringLiteral("ShareDialog");
    scene.properties = {
        {QStringLiteral("account"), QVariant::fromValue(scene.account)},
        {QStringLiteral("localPath"), QString{}},
        {QStringLiteral("shortLocalPath"), QStringLiteral("Project plan.pdf")},
        {QStringLiteral("fileId"), fileId},
        {QStringLiteral("remotePath"), QStringLiteral("/Documents/Project plan.pdf")},
    };
    scene.activate = [overview, advanced](QQuickWindow *window, QString *) {
        auto *controller = sharingController(window);
        if (!controller || controller->shares().size() != 2) {
            return false;
        }
        if (overview) {
            return true;
        }
        auto *share = controller->shares().constFirst();
        if (!window->setProperty("selectedShare", QVariant::fromValue(share)) || !window->setProperty("hasSelectedShare", true)) {
            return false;
        }
        return !advanced || window->setProperty("advancedSettingsVisible", true);
    };
    scene.settled = [overview, details, advanced](QQuickWindow *window, QString *) {
        if (overview) {
            const auto *model = window->findChild<QAbstractItemModel *>(QStringLiteral("shareListModel"));
            return model && model->rowCount() == 3;
        }
        if (!window->property("hasSelectedShare").toBool()) {
            return false;
        }
        if (details) {
            const auto *model = window->findChild<QAbstractItemModel *>(QStringLiteral("recipientModel"));
            return model && model->rowCount() == 2 && window->findChild<QObject *>(QStringLiteral("shareDetailsPage"));
        }
        const auto *model = window->findChild<QAbstractItemModel *>(QStringLiteral("advancedPropertyModel"));
        return advanced && window->property("advancedSettingsVisible").toBool() && model && model->rowCount() == 2
            && window->findChild<QObject *>(QStringLiteral("shareAdvancedSettingsPage"));
    };
    return true;
}
}
