/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "updatesharejob.h"

#include "share.h"

namespace OCC::Gui::Sharing
{

UpdateShareJob::UpdateShareJob(AccountPtr account,
                               Share &share,
                               const QString &path,
                               const QByteArray &verb,
                               const UnifiedSharingRequestOptions &options)
    : UnifiedSharingRequest{std::move(account), path, verb, options}
{
    connect(this, &OcsJob::jobFinished, this, [this, share = QPointer<Share>{&share}](const QJsonDocument &json, int) {
        if (share) {
            share->updateFromJson(json);
        }
        Q_EMIT shareUpdated(share);
    });
}

}
