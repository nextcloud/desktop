/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "propagatorjobs.h"

#include <QStringList>

class FailingTrashLocalRemoveJob final : public OCC::PropagateLocalRemove
{
public:
    using PropagateLocalRemove::PropagateLocalRemove;

    mutable QStringList _trashAttempts;

protected:
    [[nodiscard]] bool moveToTrash(const QString &fileName, QString *errorString) const override
    {
        Q_UNUSED(errorString)
        _trashAttempts.append(fileName);
        return false;
    }
};
