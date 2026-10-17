/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fileproviderstoragemove.h"

namespace OCC::Mac::FileProviderStorageMove
{

Result execute(const QString &previousVolumeUuid,
               const QByteArray &previousVolumeBookmark,
               const QString &newVolumeUuid,
               const QByteArray &newVolumeBookmark,
               const SetEnabled &setEnabled,
               const SetStorage &setStorage)
{
    Result result;
    result.disableAction = setEnabled(false);

    if (result.disableAction == ActionResult::Failed) {
        result.outcome = Outcome::DisableFailed;
        return result;
    }

    setStorage(newVolumeUuid, newVolumeBookmark);
    result.enableAction = setEnabled(true);

    if (result.enableAction != ActionResult::Failed) {
        return result;
    }

    setStorage(previousVolumeUuid, previousVolumeBookmark);
    result.rollbackAction = setEnabled(true);
    result.outcome = result.rollbackAction == ActionResult::Failed ? Outcome::RollbackFailed : Outcome::EnableFailedRolledBack;
    return result;
}

} // namespace OCC::Mac::FileProviderStorageMove
