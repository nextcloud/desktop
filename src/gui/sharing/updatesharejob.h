/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "unifiedsharingrequest.h"

namespace OCC::Gui::Sharing
{

class Share;
class UnifiedSharingApi;

/**
 * @brief Applies a server response to an existing share.
 */
class UpdateShareJob : public UnifiedSharingRequest
{
    Q_OBJECT

Q_SIGNALS:
    void shareUpdated(QPointer<Share> share);

private:
    friend class UnifiedSharingApi;
    explicit UpdateShareJob(AccountPtr account,
                            QPointer<Share> share,
                            const QString &path,
                            const QByteArray &verb,
                            const QList<QPair<QString, QString>> &parameters);
};

}
