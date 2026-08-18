/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "setpropertyjob.h"

#include "unifiedshare.h"

#include <QJsonValue>

using namespace Qt::StringLiterals;

namespace OCC::Gui::Sharing
{

SetPropertyJob::SetPropertyJob(AccountPtr account,
                               Share &share,
                               const QString &propertyClass,
                               const std::optional<QString> &value)
    : UpdateShareJob{std::move(account),
                     share,
                     "/ocs/v2.php/apps/sharing/api/v1/share/%1/property"_L1.arg(share.id()),
                     "PUT"_ba,
                     {.parameters = {}, .passStatusCodes = {}, .body = QJsonObject{{"class"_L1, propertyClass}, {"value"_L1, value ? QJsonValue{*value} : QJsonValue{QJsonValue::Null}}}}}
{
}

}
