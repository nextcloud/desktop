/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QList>
#include <QObject>
#include <QPair>

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
    explicit UnifiedSharingRequest(AccountPtr account,
                                   const QString &path,
                                   const QByteArray &verb,
                                   const QList<QPair<QString, QString>> &parameters = {},
                                   const QList<int> &passStatusCodes = {});

    void start() override;

private:
    bool _started = false;
};

}
