/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "unifiedsharingapi.h"

#include "createsharejob.h"
#include "destroysharejob.h"
#include "searchrecipientsjob.h"
#include "share.h"
#include "updatesharejob.h"

using namespace Qt::StringLiterals;

namespace OCC::Gui::Sharing
{

namespace
{
constexpr auto nodeSourceType = "OCA\\Files\\Sharing\\Source\\NodeShareSourceType"_L1;
}

UnifiedSharingApi::UnifiedSharingApi(AccountPtr account, QObject *parent)
    : QObject{parent}
    , _account{std::move(account)}
{
}

CreateShareJob *UnifiedSharingApi::createShare() const
{
    return new CreateShareJob{_account};
}

DestroyShareJob *UnifiedSharingApi::destroyShare(const QString &shareId) const
{
    return new DestroyShareJob{_account, shareId};
}

UpdateShareJob *UnifiedSharingApi::addSource(QPointer<Share> share, const QString &fileId) const
{
    return new UpdateShareJob{_account,
                              share,
                              "/ocs/v2.php/apps/sharing/api/v1/share/%1/source"_L1.arg(share->id()),
                              "POST"_ba,
                              {{"class"_L1, nodeSourceType}, {"value"_L1, fileId}}};
}

UpdateShareJob *UnifiedSharingApi::addRecipient(QPointer<Share> share, const QString &recipientType, const QString &recipientValue) const
{
    return new UpdateShareJob{_account,
                              share,
                              "/ocs/v2.php/apps/sharing/api/v1/share/%1/recipient"_L1.arg(share->id()),
                              "POST"_ba,
                              {{"class"_L1, recipientType}, {"value"_L1, recipientValue}}};
}

UpdateShareJob *UnifiedSharingApi::removeRecipient(QPointer<Share> share, const QString &recipientType, const QString &recipientValue) const
{
    return new UpdateShareJob{_account,
                              share,
                              "/ocs/v2.php/apps/sharing/api/v1/share/%1/recipient"_L1.arg(share->id()),
                              "DELETE"_ba,
                              {{"class"_L1, recipientType}, {"value"_L1, recipientValue}}};
}

SearchRecipientsJob *UnifiedSharingApi::searchRecipients(const QString &query, qint64 offset, qint64 limit) const
{
    return new SearchRecipientsJob{_account, query, offset, limit};
}

UpdateShareJob *UnifiedSharingApi::setPermission(QPointer<Share> share, const QString &permissionClass, bool enabled) const
{
    return new UpdateShareJob{_account,
                              share,
                              "/ocs/v2.php/apps/sharing/api/v1/share/%1/permission"_L1.arg(share->id()),
                              "PUT"_ba,
                              {{"class"_L1, permissionClass}, {"enabled"_L1, enabled ? "true"_L1 : "false"_L1}}};
}

UpdateShareJob *UnifiedSharingApi::setPermissionPreset(QPointer<Share> share, const QString &permissionPreset) const
{
    return new UpdateShareJob{_account,
                              share,
                              "/ocs/v2.php/apps/sharing/api/v1/share/%1/permission/preset"_L1.arg(share->id()),
                              "PUT"_ba,
                              {{"permissionPreset"_L1, permissionPreset}}};
}

}
