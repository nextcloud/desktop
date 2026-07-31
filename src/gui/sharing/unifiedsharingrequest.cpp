/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "unifiedsharingrequest.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcUnifiedSharingRequest, "nextcloud.gui.sharing.unifiedsharingrequest", QtInfoMsg)

using namespace OCC;
using namespace OCC::Gui::Sharing;

UnifiedSharingRequest::UnifiedSharingRequest(AccountPtr account,
                                             const QString &path,
                                             const QByteArray &verb,
                                             const UnifiedSharingRequestOptions &options)
    : OcsJob{account}
{
    setPath(path);
    setVerb(verb);
    for (const auto &[name, value] : options.parameters) {
        addParam(name, value);
    }
    if (options.passStatusCodes) {
        setPassStatusCodes(*options.passStatusCodes);
    }
    if (options.body) {
        setJsonBody(*options.body);
    }
}

void UnifiedSharingRequest::start()
{
    if (_started) {
        qCWarning(lcUnifiedSharingRequest) << "Attempted to start a Unified Sharing request more than once.";
        return;
    }
    _started = true;
    OcsJob::start();
}
