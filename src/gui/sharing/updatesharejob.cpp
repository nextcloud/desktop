/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "updatesharejob.h"

#include "share.h"

namespace OCC::Gui::Sharing
{

UpdateShareJob::UpdateShareJob(AccountPtr account,
                               QPointer<Share> share,
                               const QString &path,
                               const QByteArray &verb,
                               const QList<QPair<QString, QString>> &parameters)
    : UnifiedSharingRequest{std::move(account), path, verb, parameters}
{
    connect(this, &OcsJob::jobFinished, this, [this, share](const QJsonDocument &json, int) {
        if (share) {
            share->updateFromJson(json);
        }
        Q_EMIT shareUpdated(share);
    });
}

}
