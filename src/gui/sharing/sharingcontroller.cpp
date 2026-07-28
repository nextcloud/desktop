/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "sharingcontroller.h"

#include <QJsonDocument>
#include <QLoggingCategory>

#include "addrecipientjob.h"
#include "addsourcejob.h"
#include "createsharejob.h"
#include "destroysharejob.h"
#include "getsharesjob.h"
#include "removerecipientjob.h"
#include "setpermissionjob.h"
#include "setpermissionpresetjob.h"
#include "share.h"
#include "sharingconstants.h"
#include "updatesharejob.h"

Q_LOGGING_CATEGORY(lcSharingController, "nextcloud.gui.sharing.sharingcontroller", QtInfoMsg)

using namespace Qt::StringLiterals;

using namespace OCC;
using namespace OCC::Gui::Sharing;

SharingController::SharingController(QObject *parent)
    : QObject{parent}
{
}

AccountPtr SharingController::account() const
{
    return _account;
}

void SharingController::setAccount(AccountPtr account)
{
    if (_account == account) {
        return;
    }

    _account = account;
    Q_EMIT accountChanged();
}

Share *SharingController::share() const
{
    return _share.get();
}

void SharingController::initialize(const QString &fileId)
{
    if (!_account) {
        qCWarning(lcSharingController) << "attempted to initialize sharing without an account set";
        return;
    }

    if (fileId.isEmpty()) {
        qCWarning(lcSharingController) << "attempted to initialize sharing without a file ID";
        return;
    }

    const auto job = new GetSharesJob{_account, SourceTypeClasses::node, fileId};
    connect(job, &GetSharesJob::sharesFetched, this, &SharingController::handleSharesFetched);
    job->start();
}

void SharingController::createShare(const QString &fileId)
{
    if (!_account) {
        qCWarning(lcSharingController) << "attempted to create a new share without an account set";
        return;
    }

    const auto job = new CreateShareJob{_account};
    connect(job, &CreateShareJob::shareCreated, this, [this, fileId](QPointer<Share> share) -> void {
        if (!share) {
            qCWarning(lcSharingController) << "share created without a valid Share object";
            return;
        }

        share->setParent(this);
        _share = share;
        Q_EMIT shareChanged();
        addSourceAfterCreation(fileId);
    });
    job->start();
}

void SharingController::destroyShare()
{
    if (!_account) {
        qCWarning(lcSharingController) << "attempted to create a new share without an account set";
        return;
    }

    if (!_share) {
        qCWarning(lcSharingController) << "attempted to destroty a share without a share";
        return;
    }

    const auto job = new DestroyShareJob{_account, _share->id()};
    connect(job, &DestroyShareJob::jobFinished, this, [this](const QJsonDocument &, int) {
        _share = nullptr;
        Q_EMIT shareChanged(); // TODO: shareDeleted maybe?
    });
    job->start();
}

void SharingController::addRecipient(const QString &recipientType, const QString &recipientValue)
{
    if (!_account) {
        qCWarning(lcSharingController) << "attempted to add a new recipient to a share without an account set";
        return;
    }

    if (!_share) {
        qCWarning(lcSharingController) << "attempted to add a new recipient without a share";
        return;
    }

    const auto job = new AddRecipientJob{_account, *_share, recipientType, recipientValue};
    connect(job, &UpdateShareJob::shareUpdated, this, [this](QPointer<Share>) {
        qCDebug(lcSharingController).nospace() << "recipient added"
                                               << " id=" << _share->id();
    });
    job->start();
}

void SharingController::removeRecipient(const QString &recipientType, const QString &recipientValue)
{
    if (!_account) {
        qCWarning(lcSharingController) << "attempted to remove a new recipient to a share without an account set";
        return;
    }

    if (!_share) {
        qCWarning(lcSharingController) << "attempted to remove a new recipient without a share";
        return;
    }

    const auto job = new RemoveRecipientJob{_account, *_share, recipientType, recipientValue};
    connect(job, &UpdateShareJob::shareUpdated, this, [this](QPointer<Share>) {
        qCDebug(lcSharingController).nospace() << "recipient removed"
                                               << " id=" << _share->id();
    });
    job->start();
}

void SharingController::setPermission(const QString &permissionClass, bool enabled)
{
    if (!_account) {
        qCWarning(lcSharingController) << "attempted to set permission without an account set";
        return;
    }

    if (!_share) {
        qCWarning(lcSharingController) << "attempted to set permission without a share";
        return;
    }

    const auto job = new SetPermissionJob{_account, *_share, permissionClass, enabled};
    connect(job, &UpdateShareJob::shareUpdated, this, [this](QPointer<Share>) {
        qCDebug(lcSharingController).nospace() << "permissions updated"
                                               << " id=" << _share->id();
    });
    job->start();
}

void SharingController::setPermissionPreset(const QString &permissionPreset)
{
    if (!_account) {
        qCWarning(lcSharingController) << "attempted to set permission preset without an account set";
        return;
    }

    if (!_share) {
        qCWarning(lcSharingController) << "attempted to set permission preset without a share";
        return;
    }

    if (permissionPreset.isEmpty()) {
        qCDebug(lcSharingController) << "ignoring attempt to set a null/empty permission preset";
        return;
    }

    const auto job = new SetPermissionPresetJob{_account, *_share, permissionPreset};
    connect(job, &UpdateShareJob::shareUpdated, this, [this](QPointer<Share>) {
        qCDebug(lcSharingController).nospace() << "permissions updated"
                                               << " id=" << _share->id();
    });
    job->start();
}

void SharingController::addSourceAfterCreation(const QString &fileId)
{
    const auto job = new AddSourceJob{_account, *_share, fileId};
    connect(job, &UpdateShareJob::shareUpdated, this, [this, fileId](QPointer<Share>) {
        qCDebug(lcSharingController).nospace() << "share created"
                                               << " id=" << _share->id() << " fileId=" << fileId;
    });
    job->start();
}

void SharingController::handleSharesFetched(const QList<QPointer<Share>> &shares)
{
    for (const auto &share : _shares) {
        if (share) {
            share->deleteLater();
        }
    }

    _shares = shares;
    for (const auto &share : _shares) {
        if (share) {
            share->setParent(this);
        }
    }

    if (_share) {
        _share = nullptr;
        Q_EMIT shareChanged();
    }
    Q_EMIT sharesChanged();
}
