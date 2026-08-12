/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "sharingcontroller.h"

#include <QJsonDocument>
#include <QLoggingCategory>
#include <QNetworkReply>

#include <optional>
#include <memory>

#include "addrecipientjob.h"
#include "addsourcejob.h"
#include "createsharejob.h"
#include "destroysharejob.h"
#include "generatesecretjob.h"
#include "getsharesjob.h"
#include "removerecipientjob.h"
#include "setpermissionjob.h"
#include "setpermissionpresetjob.h"
#include "setpropertyjob.h"
#include "setrecipientsecretjob.h"
#include "setsharestatejob.h"
#include "share.h"
#include "sharingconstants.h"

Q_LOGGING_CATEGORY(lcSharingController, "nextcloud.gui.sharing.sharingcontroller", QtInfoMsg)

using namespace Qt::StringLiterals;

using namespace OCC;
using namespace OCC::Gui::Sharing;

namespace
{
std::optional<QString> optionalString(const QString &value)
{
    return value.isEmpty() ? std::nullopt : std::optional{value};
}
}

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

bool SharingController::destroyingShare() const
{
    return _destroyingShare;
}

QString SharingController::shareDestructionError() const
{
    return _shareDestructionError;
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

    if (_destroyingShare) {
        qCDebug(lcSharingController) << "ignoring attempt to destroy a share while another deletion is in progress";
        return;
    }

    setShareDestructionError({});
    setDestroyingShare(true);
    const auto guardedShare = QPointer<Share>{share};
    const auto job = new DestroyShareJob{_account, share->id()};
    connect(job, &DestroyShareJob::jobFinished, this, [this, guardedShare](const QJsonDocument &, int) {
        if (!guardedShare) {
            setDestroyingShare(false);
            return;
        }

        const auto share = guardedShare.data();
        _shares.removeAll(share);
        setDestroyingShare(false);
        share->deleteLater();
        Q_EMIT sharesChanged();
    });
    connect(job, &DestroyShareJob::ocsError, this, [this](int, const QString &message) {
        setShareDestructionError(message.isEmpty() ? tr("Could not delete the share.") : message);
        setDestroyingShare(false);
    });
    connect(job, &DestroyShareJob::networkError, this, [this](const QNetworkReply *reply) {
        setShareDestructionError(reply ? reply->errorString() : tr("Could not delete the share."));
        setDestroyingShare(false);
    });
    job->start();
}

void SharingController::addRecipient(Share *share,
                                     const QString &recipientType,
                                     const QString &recipientValue,
                                     const QString &recipientInstance)
{
    if (!_account) {
        qCWarning(lcSharingController) << "attempted to add a new recipient to a share without an account set";
        return;
    }

    if (!containsShare(share)) {
        qCWarning(lcSharingController) << "attempted to add a recipient to a share not owned by this controller";
        return;
    }

    const auto guardedShare = QPointer<Share>{share};
    const auto job = new AddRecipientJob{_account, *share, recipientType, recipientValue, optionalString(recipientInstance)};
    connect(job, &AddRecipientJob::shareUpdated, this, [this](QPointer<Share> updatedShare) {
        if (updatedShare) {
            Q_EMIT recipientAdded(updatedShare);
        }
    });
    connect(job, &AddRecipientJob::ocsError, this, [this, guardedShare](int, const QString &message) {
        Q_EMIT recipientAdditionFailed(guardedShare, message.isEmpty() ? tr("Could not add the recipient.") : message);
    });
    connect(job, &AddRecipientJob::networkError, this, [this, guardedShare](const QNetworkReply *reply) {
        Q_EMIT recipientAdditionFailed(guardedShare, reply ? reply->errorString() : tr("Could not add the recipient."));
    });
    job->start();
}

void SharingController::removeRecipient(Share *share,
                                        const QString &recipientType,
                                        const QString &recipientValue,
                                        const QString &recipientInstance)
{
    if (!_account) {
        qCWarning(lcSharingController) << "attempted to remove a recipient from a share without an account set";
        return;
    }

    if (!containsShare(share)) {
        qCWarning(lcSharingController) << "attempted to remove a recipient from a share not owned by this controller";
        return;
    }

    const auto guardedShare = QPointer<Share>{share};
    const auto job = new RemoveRecipientJob{_account, *share, recipientType, recipientValue, optionalString(recipientInstance)};
    connect(job, &RemoveRecipientJob::shareUpdated, this, [this](QPointer<Share> updatedShare) {
        if (updatedShare) {
            Q_EMIT recipientRemoved(updatedShare);
        }
    });
    connect(job, &RemoveRecipientJob::ocsError, this, [this, guardedShare](int, const QString &message) {
        Q_EMIT recipientRemovalFailed(guardedShare, message.isEmpty() ? tr("Could not remove the recipient.") : message);
    });
    connect(job, &RemoveRecipientJob::networkError, this, [this, guardedShare](const QNetworkReply *reply) {
        Q_EMIT recipientRemovalFailed(guardedShare, reply ? reply->errorString() : tr("Could not remove the recipient."));
    });
    job->start();
}

void SharingController::updateRecipientSecret(Share *share,
                                              const QString &recipientType,
                                              const QString &recipientValue,
                                              const QString &recipientInstance)
{
    if (!_account) {
        qCWarning(lcSharingController) << "attempted to update a recipient secret without an account set";
        return;
    }

    if (!containsShare(share)) {
        qCWarning(lcSharingController) << "attempted to update a recipient secret on a share not owned by this controller";
        return;
    }

    const auto guardedShare = QPointer<Share>{share};
    const auto instance = optionalString(recipientInstance);
    const auto generateJob = new GenerateSecretJob{_account};
    connect(generateJob, &GenerateSecretJob::secretGenerated, this, [this, guardedShare, recipientType, recipientValue, instance](const QString &secret) {
        if (!guardedShare || secret.isEmpty()) {
            Q_EMIT recipientSecretUpdateFailed(guardedShare, tr("The server did not generate a valid sharing link."));
            return;
        }

        const auto updateJob = new SetRecipientSecretJob{_account, *guardedShare, recipientType, recipientValue, secret, instance};
        connect(updateJob, &SetRecipientSecretJob::shareUpdated, this, [this](QPointer<Share> updatedShare) {
            if (updatedShare) {
                Q_EMIT recipientSecretUpdated(updatedShare);
            }
        });
        connect(updateJob, &SetRecipientSecretJob::ocsError, this, [this, guardedShare](int, const QString &message) {
            Q_EMIT recipientSecretUpdateFailed(guardedShare, message.isEmpty() ? tr("Could not update the sharing link.") : message);
        });
        connect(updateJob, &SetRecipientSecretJob::networkError, this, [this, guardedShare](const QNetworkReply *reply) {
            Q_EMIT recipientSecretUpdateFailed(guardedShare, reply ? reply->errorString() : tr("Could not update the sharing link."));
        });
        updateJob->start();
    });
    connect(generateJob, &GenerateSecretJob::ocsError, this, [this, guardedShare](int, const QString &message) {
        Q_EMIT recipientSecretUpdateFailed(guardedShare, message.isEmpty() ? tr("Could not generate a sharing link.") : message);
    });
    connect(generateJob, &GenerateSecretJob::networkError, this, [this, guardedShare](const QNetworkReply *reply) {
        Q_EMIT recipientSecretUpdateFailed(guardedShare, reply ? reply->errorString() : tr("Could not generate a sharing link."));
    });
    generateJob->start();
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

    const auto guardedShare = QPointer<Share>{share};
    const auto permissionFailureReported = std::make_shared<bool>(false);
    const auto job = new SetPermissionJob{_account, *share, permissionClass, enabled};
    connect(job, &SetPermissionJob::ocsError, this, [this, guardedShare, permissionFailureReported](int, const QString &message) {
        if (*permissionFailureReported) {
            return;
        }
        *permissionFailureReported = true;
        Q_EMIT permissionUpdateFailed(guardedShare, message.isEmpty() ? tr("Could not update the permissions.") : message);
    });
    connect(job, &SetPermissionJob::networkError, this, [this, guardedShare, permissionFailureReported](const QNetworkReply *reply) {
        if (*permissionFailureReported) {
            return;
        }
        *permissionFailureReported = true;
        Q_EMIT permissionUpdateFailed(guardedShare, reply ? reply->errorString() : tr("Could not update the permissions."));
    });
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

    const auto guardedShare = QPointer<Share>{share};
    const auto permissionFailureReported = std::make_shared<bool>(false);
    const auto job = new SetPermissionPresetJob{_account, *share, permissionPreset};
    connect(job, &SetPermissionPresetJob::ocsError, this, [this, guardedShare, permissionFailureReported](int, const QString &message) {
        if (*permissionFailureReported) {
            return;
        }
        *permissionFailureReported = true;
        Q_EMIT permissionUpdateFailed(guardedShare, message.isEmpty() ? tr("Could not update the permissions.") : message);
    });
    connect(job, &SetPermissionPresetJob::networkError, this, [this, guardedShare, permissionFailureReported](const QNetworkReply *reply) {
        if (*permissionFailureReported) {
            return;
        }
        *permissionFailureReported = true;
        Q_EMIT permissionUpdateFailed(guardedShare, reply ? reply->errorString() : tr("Could not update the permissions."));
    });
    job->start();
}

void SharingController::setProperty(Share *share, const QString &propertyClass, const QString &value)
{
    if (!_account) {
        qCWarning(lcSharingController) << "attempted to set a share property without an account set";
        return;
    }

    if (!containsShare(share)) {
        qCWarning(lcSharingController) << "attempted to set a property on a share not owned by this controller";
        return;
    }

    if (propertyClass.isEmpty()) {
        qCWarning(lcSharingController) << "attempted to set a share property without a property class";
        return;
    }

    const auto guardedShare = QPointer<Share>{share};
    const auto propertyValue = value.isEmpty() ? std::nullopt : std::optional{value};
    const auto job = new SetPropertyJob{_account, *share, propertyClass, propertyValue};
    connect(job, &SetPropertyJob::shareUpdated, this, [this](QPointer<Share> updatedShare) {
        if (updatedShare) {
            Q_EMIT propertyUpdated(updatedShare);
        }
    });
    connect(job, &SetPropertyJob::ocsError, this, [this, guardedShare](int, const QString &message) {
        Q_EMIT propertyUpdateFailed(guardedShare, message.isEmpty() ? tr("Could not update the sharing setting.") : message);
    });
    connect(job, &SetPropertyJob::networkError, this, [this, guardedShare](const QNetworkReply *reply) {
        Q_EMIT propertyUpdateFailed(guardedShare, reply ? reply->errorString() : tr("Could not update the sharing setting."));
    });
    job->start();
}

void SharingController::activateShare(Share *share)
{
    if (!_account) {
        qCWarning(lcSharingController) << "attempted to activate a share without an account set";
        return;
    }

    if (!containsShare(share)) {
        qCWarning(lcSharingController) << "attempted to activate a share not owned by this controller";
        return;
    }

    if (share->state() != Share::ShareState::Draft) {
        qCDebug(lcSharingController) << "ignoring attempt to activate a share that is not a draft";
        return;
    }

    const auto guardedShare = QPointer<Share>{share};
    const auto job = new SetShareStateJob{_account, *share, Share::ShareState::Active};
    connect(job, &SetShareStateJob::shareUpdated, this, [this, guardedShare](QPointer<Share> updatedShare) {
        if (updatedShare && updatedShare->state() == Share::ShareState::Active) {
            Q_EMIT shareActivated(updatedShare);
            return;
        }

        Q_EMIT shareActivationFailed(guardedShare, tr("The server did not activate the share."));
    });
    connect(job, &SetShareStateJob::ocsError, this, [this, guardedShare](int, const QString &message) {
        Q_EMIT shareActivationFailed(guardedShare, message.isEmpty() ? tr("Could not send the share.") : message);
    });
    connect(job, &SetShareStateJob::networkError, this, [this, guardedShare](const QNetworkReply *reply) {
        Q_EMIT shareActivationFailed(guardedShare, reply ? reply->errorString() : tr("Could not send the share."));
    });
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

void SharingController::setDestroyingShare(bool destroyingShare)
{
    if (_destroyingShare == destroyingShare) {
        return;
    }
    _destroyingShare = destroyingShare;
    Q_EMIT destroyingShareChanged();
}

void SharingController::setShareDestructionError(const QString &error)
{
    if (_shareDestructionError == error) {
        return;
    }
    _shareDestructionError = error;
    Q_EMIT shareDestructionErrorChanged();
}

void SharingController::replaceShares(const QList<Share *> &shares)
{
    qDeleteAll(_shares);
    _shares = shares;
    Q_EMIT sharesChanged();
}
