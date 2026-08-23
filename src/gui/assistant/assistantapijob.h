/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "networkjobs.h"

namespace OCC {

/** @brief Sends form-encoded Assistant requests and parses their JSON response. */
class AssistantApiJob final : public SimpleApiJob
{
    Q_OBJECT

public:
    /** @brief Creates an Assistant API job for an account and endpoint. */
    explicit AssistantApiJob(const AccountPtr &account, const QString &path, QObject *parent = nullptr);
    /** @brief Sets an URL-encoded request body. */
    void setFormBody(const QUrlQuery &query);

signals:
    /** @brief Reports the parsed response and its OCS or HTTP status code. */
    void jsonReceived(const QJsonDocument &json, int statusCode);

protected:
    /** @brief Parses the completed network reply. */
    bool finished() override;
};

}
