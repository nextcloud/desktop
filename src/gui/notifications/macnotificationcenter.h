/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "accountfwd.h"
#include "tray/activitydata.h"

#include <QSet>
#include <QString>
#include <QUrl>

namespace OCC::MacNotificationCenter {

/** @brief Configure the macOS notification center delegate, permissions and base categories. */
void initialize(const QString &localizedDownloadString);

/** @brief Return whether the macOS notification center is available. */
[[nodiscard]] bool canSendNotification();

/** @brief Send a basic macOS notification. */
void sendNotification(const QString &title, const QString &message);

/** @brief Send an update notification that opens a download URL. */
void sendUpdateNotification(const QString &title, const QString &message, const QUrl &webUrl);

/**
 * @brief Send a server notification with its server-provided actions.
 * @param playSound Whether the notification should play the default sound.
 */
void sendServerNotification(const Activity &activity, const AccountStatePtr &accountState, bool playSound);

/** @brief Remove a pending and delivered server notification. */
void removeServerNotification(const QString &accountId, qint64 notificationId);

/** @brief Remove native server notifications absent from the latest server list. */
void reconcileServerNotifications(const QString &accountId, const QSet<qint64> &activeNotificationIds);

}
