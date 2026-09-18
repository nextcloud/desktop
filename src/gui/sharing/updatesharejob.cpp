/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "updatesharejob.h"

namespace OCC::Gui::Sharing
{

UpdateShareJob::UpdateShareJob(AccountPtr account, const QString &path, const QByteArray &verb, const UnifiedSharingRequest::Options &options)
    : UnifiedSharingRequest{std::move(account), path, verb, options}
{
    connect(this, &OcsJob::jobFinished, this, [this](const QJsonDocument &json, int) {
        Q_EMIT shareUpdated(json);
    });
}

}
