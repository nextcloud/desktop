/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "removesourcejob.h"

#include "sharingconstants.h"

using namespace Qt::StringLiterals;

namespace OCC::Gui::Sharing
{

RemoveSourceJob::RemoveSourceJob(AccountPtr account, const QString &shareId, const QString &fileId)
    : UpdateShareJob{std::move(account),
                     "/ocs/v2.php/apps/sharing/api/v1/share/%1/source"_L1.arg(shareId),
                     "DELETE"_ba,
                     {.parameters = {{"class"_L1, SourceTypeClasses::node}, {"value"_L1, fileId}}, .passStatusCodes = {}, .body = {}}}
{
}

}
