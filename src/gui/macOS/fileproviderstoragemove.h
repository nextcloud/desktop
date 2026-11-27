/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QByteArray>
#include <QString>

#include <functional>

namespace OCC::Mac::FileProviderStorageMove
{

enum class ActionResult {
    NoAction,
    Changed,
    Failed,
};

enum class Outcome {
    Success,
    DisableFailed,
    EnableFailedRolledBack,
    RollbackFailed,
};

struct Result {
    Outcome outcome = Outcome::Success;
    ActionResult disableAction = ActionResult::NoAction;
    ActionResult enableAction = ActionResult::NoAction;
    ActionResult rollbackAction = ActionResult::NoAction;
};

using SetEnabled = std::function<ActionResult(bool enabled)>;
using SetStorage = std::function<void(const QString &volumeUuid, const QByteArray &bookmark)>;

/**
 * Move one account's File Provider domain to a new storage location.
 *
 * The current domain is removed first. If creating the replacement domain fails, the
 * previous storage preference is restored and a fresh domain is created there.
 */
[[nodiscard]] Result execute(const QString &previousVolumeUuid,
                             const QByteArray &previousVolumeBookmark,
                             const QString &newVolumeUuid,
                             const QByteArray &newVolumeBookmark,
                             const SetEnabled &setEnabled,
                             const SetStorage &setStorage);

} // namespace OCC::Mac::FileProviderStorageMove
