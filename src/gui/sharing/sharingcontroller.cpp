/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "sharingcontroller.h"

#include <QJsonDocument>
#include <QLoggingCategory>

#include "share.h"
#include "ocssharingjob.h"

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

void SharingController::createShare(const QString &fileId)
{
    if (!_account) {
        qCWarning(lcSharingController) << "attempted to create a new share without an account set";
        return;
    }

    const auto job = new OcsSharingJob(_account);
    connect(job, &OcsSharingJob::shareCreated, this, [this, fileId](QPointer<Share> share) -> void {
        if (!share) {
            qCWarning(lcSharingController) << "share created without a valid Share object";
            return;
        }

        share->setParent(this);
        _share = share;
        Q_EMIT shareChanged();
        addSourceAfterCreation(fileId);
    });
    job->createShare();
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

    const auto job = new OcsSharingJob(_account);
    connect(job, &OcsSharingJob::jobFinished, this, [this](const QJsonDocument &json, int statusCode) -> void {
        _share = nullptr;
        Q_EMIT shareChanged(); // TODO: shareDeleted maybe?
    });
    job->createShare();
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

    const auto job = new OcsSharingJob(_account, _share->id());
    connect(job, &OcsSharingJob::jobFinished, this, [this](const QJsonDocument &json, int statusCode) -> void {
        qCDebug(lcSharingController).nospace() << "recipient added"
            << " id=" << _share->id();
        _share->updateFromJson(json);
    });
    job->addRecipient(recipientType, recipientValue);
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

    const auto job = new OcsSharingJob(_account, _share->id());
    connect(job, &OcsSharingJob::jobFinished, this, [this](const QJsonDocument &json, int statusCode) -> void {
        qCDebug(lcSharingController).nospace() << "recipient removed"
            << " id=" << _share->id();
        _share->updateFromJson(json);
    });
    job->addRecipient(recipientType, recipientValue);
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

    const auto job = new OcsSharingJob(_account, _share->id());
    connect(job, &OcsSharingJob::jobFinished, this, [this](const QJsonDocument &json, int statusCode) -> void {
        qCDebug(lcSharingController).nospace() << "permissions updated"
            << " id=" << _share->id();
        _share->updateFromJson(json);
    });
    job->setPermission(permissionClass, enabled);
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

    const auto job = new OcsSharingJob(_account, _share->id());
    connect(job, &OcsSharingJob::jobFinished, this, [this](const QJsonDocument &json, int statusCode) -> void {
        qCDebug(lcSharingController).nospace() << "permissions updated"
            << " id=" << _share->id();
        _share->updateFromJson(json);
    });
    job->setPermissionPreset(permissionPreset);
}

void SharingController::addSourceAfterCreation(const QString &fileId)
{
    const auto job = new OcsSharingJob(_account, _share->id());
    connect(job, &OcsSharingJob::jobFinished, this, [this, fileId](const QJsonDocument &json, int statusCode) -> void {
        qCDebug(lcSharingController).nospace() << "share created"
            << " id=" << _share->id()
            << " fileId=" << fileId;
        _share->updateFromJson(json);
    });
    job->addSource(fileId);
}
