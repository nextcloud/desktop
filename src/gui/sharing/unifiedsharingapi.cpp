/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "unifiedsharingapi.h"

#include "unifiedsharingrequest.h"

using namespace Qt::StringLiterals;

namespace OCC::Gui::Sharing
{

namespace
{
constexpr auto sharingV1Base = "/ocs/v2.php/apps/sharing/api/v1"_L1;
constexpr auto nodeSourceType = "OCA\\Files\\Sharing\\Source\\NodeShareSourceType"_L1;
}

UnifiedSharingApi::UnifiedSharingApi(AccountPtr account, QObject *parent)
    : QObject{parent}
    , _account{std::move(account)}
{
}

UnifiedSharingRequest *UnifiedSharingApi::createShare()
{
    return new UnifiedSharingRequest{_account, sharingV1Base % "/share"_L1, "POST"_ba, {}, {201}};
}

UnifiedSharingRequest *UnifiedSharingApi::destroyShare(const QString &shareId)
{
    return new UnifiedSharingRequest{_account, sharingV1Base % "/share/%1"_L1.arg(shareId), "DELETE"_ba, {}, {204}};
}

UnifiedSharingRequest *UnifiedSharingApi::addSource(const QString &shareId, const QString &fileId)
{
    return new UnifiedSharingRequest{_account,
                                     sharingV1Base % "/share/%1/source"_L1.arg(shareId),
                                     "POST"_ba,
                                     {{"class"_L1, nodeSourceType}, {"value"_L1, fileId}}};
}

UnifiedSharingRequest *UnifiedSharingApi::addRecipient(const QString &shareId, const QString &recipientType, const QString &recipientValue)
{
    return new UnifiedSharingRequest{_account,
                                     sharingV1Base % "/share/%1/recipient"_L1.arg(shareId),
                                     "POST"_ba,
                                     {{"class"_L1, recipientType}, {"value"_L1, recipientValue}}};
}

UnifiedSharingRequest *UnifiedSharingApi::removeRecipient(const QString &shareId, const QString &recipientType, const QString &recipientValue)
{
    return new UnifiedSharingRequest{_account,
                                     sharingV1Base % "/share/%1/recipient"_L1.arg(shareId),
                                     "DELETE"_ba,
                                     {{"class"_L1, recipientType}, {"value"_L1, recipientValue}}};
}

UnifiedSharingRequest *UnifiedSharingApi::searchRecipients(const QString &query, qint64 offset, qint64 limit)
{
    return new UnifiedSharingRequest{_account,
                                     sharingV1Base % "/recipients"_L1,
                                     "GET"_ba,
                                     {{"query"_L1, query}, {"offset"_L1, QString::number(offset)}, {"limit"_L1, QString::number(limit)}}};
}

UnifiedSharingRequest *UnifiedSharingApi::setPermission(const QString &shareId, const QString &permissionClass, bool enabled)
{
    return new UnifiedSharingRequest{_account,
                                     sharingV1Base % "/share/%1/permission"_L1.arg(shareId),
                                     "PUT"_ba,
                                     {{"class"_L1, permissionClass}, {"enabled"_L1, enabled ? "true"_L1 : "false"_L1}}};
}

UnifiedSharingRequest *UnifiedSharingApi::setPermissionPreset(const QString &shareId, const QString &permissionPreset)
{
    return new UnifiedSharingRequest{_account,
                                     sharingV1Base % "/share/%1/permission/preset"_L1.arg(shareId),
                                     "PUT"_ba,
                                     {{"permissionPreset"_L1, permissionPreset}}};
}

}
