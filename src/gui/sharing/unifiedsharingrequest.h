/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPair>

#include <optional>

#include "ocsjob.h"

#include "accountfwd.h"

namespace OCC::Gui::Sharing
{

/**
 * @brief Configures and starts one request to the Unified Sharing API.
 */
class UnifiedSharingRequest : public OcsJob
{
    Q_OBJECT

public:
    /**
    * @brief Optional parts of a Unified Sharing request.
    */
    struct Options
    {
        QList<QPair<QString, QString>> parameters; //!< Query or form parameters to send
        std::optional<QList<int>> passStatusCodes; //!< Accepted status codes, or no value to keep the OCS defaults
        std::optional<QJsonObject> body; //!< JSON body to send, or no value to send no JSON body
    };

    explicit UnifiedSharingRequest(AccountPtr account,
                                   const QString &path,
                                   const QByteArray &verb,
                                   const Options &options = {});

    void start() override;

private:
    bool _started = false;
};

}
