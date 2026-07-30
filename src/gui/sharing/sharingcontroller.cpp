/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "sharingcontroller.h"

#include <QJsonDocument>
#include <QLoggingCategory>
#include <QNetworkReply>

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

Q_LOGGING_CATEGORY(lcSharingController, "nextcloud.gui.sharing.sharingcontroller", QtInfoMsg)

using namespace Qt::StringLiterals;

using namespace OCC;
using namespace OCC::Gui::Sharing;

SharingController::SharingController(QObject *parent)
    : QObject{parent}
{
}

SharingController::~SharingController()
{
    qDeleteAll(_shares);
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

const QList<Share *> &SharingController::shares() const
{
    return _shares;
}

bool SharingController::creatingShare() const
{
    return _creatingShare;
}

QString SharingController::shareCreationError() const
{
    return _shareCreationError;
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
    connect(job, &GetSharesJob::sharesFetched, this, [this](const QList<QPointer<Share>> &shares) {
        auto ownedShares = QList<Share *>{};
        ownedShares.reserve(shares.size());
        for (const auto &share : shares) {
            if (share) {
                ownedShares.append(share);
            }
        }
        replaceShares(ownedShares);
    });
    job->start();
}

void SharingController::createShare(const QString &fileId)
{
    if (!_account) {
        qCWarning(lcSharingController) << "attempted to create a new share without an account set";
        return;
    }

    if (fileId.isEmpty()) {
        qCWarning(lcSharingController) << "attempted to create a new share without a file ID";
        return;
    }

    if (_creatingShare) {
        qCDebug(lcSharingController) << "ignoring attempt to create a share while another creation is in progress";
        return;
    }

    setShareCreationError({});
    setCreatingShare(true);

    const auto job = new CreateShareJob{_account};
    connect(job, &CreateShareJob::shareCreated, this, [this, fileId](QPointer<Share> share) -> void {
        if (!share || share->id().isEmpty()) {
            qCWarning(lcSharingController) << "share created without a valid Share object";
            failShareCreation(tr("The server returned an invalid share."), share);
            return;
        }

        share->setParent(this);
        addSourceAfterCreation(share, fileId);
    });
    connect(job, &CreateShareJob::ocsError, this, [this](int, const QString &message) {
        failShareCreation(message.isEmpty() ? tr("Could not create the share.") : message);
    });
    connect(job, &CreateShareJob::networkError, this, [this](const QNetworkReply *reply) {
        failShareCreation(reply ? reply->errorString() : tr("Could not create the share."));
    });
    job->start();
}

void SharingController::destroyShare(Share *share)
{
    if (!_account) {
        qCWarning(lcSharingController) << "attempted to destroy a share without an account set";
        return;
    }

    if (!containsShare(share)) {
        qCWarning(lcSharingController) << "attempted to destroy a share not owned by this controller";
        return;
    }

    const auto job = new DestroyShareJob{_account, share->id()};
    connect(job, &DestroyShareJob::jobFinished, this, [this, share](const QJsonDocument &, int) {
        _shares.removeAll(share);
        share->deleteLater();
        Q_EMIT sharesChanged();
    });
    job->start();
}

void SharingController::addRecipient(Share *share, const QString &recipientType, const QString &recipientValue)
{
    if (!_account) {
        qCWarning(lcSharingController) << "attempted to add a new recipient to a share without an account set";
        return;
    }

    if (!containsShare(share)) {
        qCWarning(lcSharingController) << "attempted to add a recipient to a share not owned by this controller";
        return;
    }

    const auto job = new AddRecipientJob{_account, *share, recipientType, recipientValue};
    job->start();
}

void SharingController::removeRecipient(Share *share, const QString &recipientType, const QString &recipientValue)
{
    if (!_account) {
        qCWarning(lcSharingController) << "attempted to remove a new recipient to a share without an account set";
        return;
    }

    if (!containsShare(share)) {
        qCWarning(lcSharingController) << "attempted to remove a recipient from a share not owned by this controller";
        return;
    }

    const auto job = new RemoveRecipientJob{_account, *share, recipientType, recipientValue};
    job->start();
}

void SharingController::setPermission(Share *share, const QString &permissionClass, bool enabled)
{
    if (!_account) {
        qCWarning(lcSharingController) << "attempted to set permission without an account set";
        return;
    }

    if (!containsShare(share)) {
        qCWarning(lcSharingController) << "attempted to set permission on a share not owned by this controller";
        return;
    }

    const auto job = new SetPermissionJob{_account, *share, permissionClass, enabled};
    job->start();
}

void SharingController::setPermissionPreset(Share *share, const QString &permissionPreset)
{
    if (!_account) {
        qCWarning(lcSharingController) << "attempted to set permission preset without an account set";
        return;
    }

    if (!containsShare(share)) {
        qCWarning(lcSharingController) << "attempted to set a permission preset on a share not owned by this controller";
        return;
    }

    if (permissionPreset.isEmpty()) {
        qCDebug(lcSharingController) << "ignoring attempt to set a null/empty permission preset";
        return;
    }

    const auto job = new SetPermissionPresetJob{_account, *share, permissionPreset};
    job->start();
}

bool SharingController::containsShare(const Share *share) const
{
    return share && _shares.contains(share);
}

void SharingController::addSourceAfterCreation(QPointer<Share> share, const QString &fileId)
{
    if (!share) {
        failShareCreation(tr("The newly created share is no longer available."));
        return;
    }

    const auto job = new AddSourceJob{_account, *share, fileId};
    connect(job, &AddSourceJob::shareUpdated, this, [this](QPointer<Share> updatedShare) {
        if (!updatedShare) {
            failShareCreation(tr("The newly created share is no longer available."));
            return;
        }
        finishShareCreation(updatedShare);
    });
    connect(job, &AddSourceJob::ocsError, this, [this, share](int, const QString &message) {
        failShareCreation(message.isEmpty() ? tr("Could not attach the item to the share.") : message, share);
    });
    connect(job, &AddSourceJob::networkError, this, [this, share](const QNetworkReply *reply) {
        failShareCreation(reply ? reply->errorString() : tr("Could not attach the item to the share."), share);
    });
    job->start();
}

void SharingController::finishShareCreation(QPointer<Share> share)
{
    _shares.append(share);
    setCreatingShare(false);
    Q_EMIT sharesChanged();
}

void SharingController::failShareCreation(const QString &error, QPointer<Share> share)
{
    if (!_creatingShare) {
        return;
    }

    setShareCreationError(error);
    setCreatingShare(false);

    if (!share) {
        return;
    }

    if (!share->id().isEmpty()) {
        const auto cleanupJob = new DestroyShareJob{_account, share->id()};
        cleanupJob->start();
    }
    delete share.data();
}

void SharingController::setCreatingShare(bool creatingShare)
{
    if (_creatingShare == creatingShare) {
        return;
    }
    _creatingShare = creatingShare;
    Q_EMIT creatingShareChanged();
}

void SharingController::setShareCreationError(const QString &error)
{
    if (_shareCreationError == error) {
        return;
    }
    _shareCreationError = error;
    Q_EMIT shareCreationErrorChanged();
}

void SharingController::replaceShares(const QList<Share *> &shares)
{
    qDeleteAll(_shares);
    _shares = shares;
    Q_EMIT sharesChanged();
}
