/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "removesourcejob.h"

#include "share.h"

using namespace Qt::StringLiterals;

namespace OCC::Gui::Sharing
{

RemoveSourceJob::RemoveSourceJob(AccountPtr account, Share &share, const QString &fileId)
    : UpdateShareJob{std::move(account),
                     share,
                     "/ocs/v2.php/apps/sharing/api/v1/share/%1/source"_L1.arg(share.id()),
                     "DELETE"_ba,
                     {.parameters = {{"class"_L1, "OCA\\Files\\Sharing\\Source\\NodeShareSourceType"_L1}, {"value"_L1, fileId}}}}
{
}

}
