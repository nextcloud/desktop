/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "setpropertyjob.h"

#include <QJsonValue>

using namespace Qt::StringLiterals;

namespace OCC::Gui::Sharing
{

SetPropertyJob::SetPropertyJob(AccountPtr account, const QString &shareId, const QString &propertyClass, const std::optional<QString> &value)
    : UpdateShareJob{std::move(account),
                     "/ocs/v2.php/apps/sharing/api/v1/share/%1/property"_L1.arg(shareId),
                     "PUT"_ba,
                     {.parameters = {},
                      .passStatusCodes = {},
                      .body = QJsonObject{{"class"_L1, propertyClass}, {"value"_L1, value ? QJsonValue{*value} : QJsonValue{QJsonValue::Null}}}}}
{
}

}
