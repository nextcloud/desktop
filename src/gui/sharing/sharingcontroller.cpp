/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "sharingcontroller.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkReply>

#include <algorithm>
#include <memory>
#include <optional>

#include "addrecipientjob.h"
#include "addsourcejob.h"
#include "createsharejob.h"
#include "deletesharejob.h"
#include "generatesecretjob.h"
#include "getsharejob.h"
#include "getsharesjob.h"
#include "networkjobs.h"
#include "recipient.h"
#include "removerecipientjob.h"
#include "setpermissionjob.h"
#include "setpermissionpresetjob.h"
#include "setpropertyjob.h"
#include "setrecipientpermissionjob.h"
#include "setrecipientsecretjob.h"
#include "setsharestatejob.h"
#include "sharingconstants.h"
#include "unifiedshare.h"

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

SharingController::~SharingController() = default;

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

QList<Share *> SharingController::shares() const
{
    auto result = QList<Share *>{};
    result.reserve(static_cast<qsizetype>(_shares.size()));
    for (const auto &share : _shares) {
        result.append(share.get());
    }
    return result;
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

bool SharingController::resolvingInternalLink() const
{
    return _resolvingInternalLink;
}

QString SharingController::internalLinkError() const
{
    return _internalLinkError;
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
    connect(job, &GetSharesJob::sharesFetched, this, [this](const QJsonDocument &json) {
        auto ownedShares = std::vector<std::unique_ptr<Share>>{};
        const auto data = json.object().value("ocs"_L1).toObject().value("data"_L1).toArray();
        ownedShares.reserve(static_cast<size_t>(data.size()));
        for (const auto &value : data) {
            if (!value.isObject()) {
                continue;
            }
            const auto shareJson = QJsonDocument{QJsonObject{
                {"ocs"_L1, QJsonObject{{"data"_L1, value.toObject()}}},
            }};
            ownedShares.emplace_back(Share::fromJson(shareJson, _account));
        }
        replaceShares(std::move(ownedShares));
    });
    job->start();
}

void SharingController::createShareForRecipient(const QString &fileId,
                                                const QString &recipientType,
                                                const QString &recipientValue,
                                                const QString &recipientInstance)
{
    if (recipientType.isEmpty() || recipientValue.isEmpty()) {
        qCWarning(lcSharingController) << "attempted to create a share without a recipient";
        return;
    }

    if (beginShareCreation(fileId)) {
        startShareCreation(fileId, recipientType, recipientValue, recipientInstance);
    }
}

void SharingController::createPublicLink(const QString &fileId)
{
    if (std::ranges::any_of(_shares, [](const auto &share) {
            return share && share->isPublicLink();
        })) {
        qCDebug(lcSharingController) << "ignoring attempt to create a second public link";
        return;
    }

    if (!beginShareCreation(fileId)) {
        return;
    }

    const auto generateJob = new GenerateSecretJob{_account};
    connect(generateJob, &GenerateSecretJob::secretGenerated, this, [this, fileId](const QString &recipientValue) {
        if (recipientValue.isEmpty()) {
            failShareCreation(tr("The server did not generate a valid public-link identifier."));
            return;
        }
        startShareCreation(fileId, QString{RecipientTypeClasses::token}, recipientValue, {}, true);
    });
    connect(generateJob, &GenerateSecretJob::ocsError, this, [this](int, const QString &message) {
        failShareCreation(message.isEmpty() ? tr("Could not create the public link.") : message);
    });
    connect(generateJob, &GenerateSecretJob::networkError, this, [this](const QNetworkReply *reply) {
        failShareCreation(reply ? reply->errorString() : tr("Could not create the public link."));
    });
    generateJob->start();
}

void SharingController::fetchShareDetails(Share *share)
{
    if (!_account) {
        qCWarning(lcSharingController) << "attempted to fetch share details without an account set";
        return;
    }

    if (!containsShare(share)) {
        qCWarning(lcSharingController) << "attempted to fetch details for a share not owned by this controller";
        return;
    }

    const auto shareId = share->id();
    const auto job = new GetShareJob{_account, shareId};
    connect(job, &GetShareJob::shareJsonFetched, this, [this, shareId](const QJsonDocument &json) {
        if (const auto currentShare = shareById(shareId)) {
            currentShare->updateFromJson(json);
        }
    });
    job->start();
}

void SharingController::requestInternalLink(const QString &remotePath, const QString &numericFileId)
{
    if (!_account || remotePath.isEmpty() || _resolvingInternalLink) {
        return;
    }

    setInternalLinkError({});
    setResolvingInternalLink(true);
    fetchPrivateLinkUrl(_account, remotePath, numericFileId.toUtf8(), this, [this](const QString &url) {
        setResolvingInternalLink(false);
        if (url.isEmpty()) {
            setInternalLinkError(tr("Could not retrieve the internal link."));
            return;
        }
        Q_EMIT internalLinkResolved(url);
    });
}

bool SharingController::beginShareCreation(const QString &fileId)
{
    if (!_account) {
        qCWarning(lcSharingController) << "attempted to create a new share without an account set";
        return false;
    }

    if (fileId.isEmpty()) {
        qCWarning(lcSharingController) << "attempted to create a new share without a file ID";
        return false;
    }

    if (_creatingShare) {
        qCDebug(lcSharingController) << "ignoring attempt to create a share while another creation is in progress";
        return false;
    }

    setShareCreationError({});
    setCreatingShare(true);
    return true;
}

void SharingController::startShareCreation(const QString &fileId,
                                           const QString &recipientType,
                                           const QString &recipientValue,
                                           const QString &recipientInstance,
                                           bool activateAfterCreation)
{
    if (!_creatingShare) {
        return;
    }

    const auto job = new CreateShareJob{_account};
    connect(job,
            &CreateShareJob::shareCreated,
            this,
            [this, fileId, recipientType, recipientValue, recipientInstance, activateAfterCreation](const QJsonDocument &json) -> void {
                auto share = Share::fromJson(json, _account);
                if (!share || share->id().isEmpty()) {
                    qCWarning(lcSharingController) << "share created without a valid Share object";
                    failShareCreation(tr("The server returned an invalid share."));
                    return;
                }

                const auto shareId = share->id();
                _draftShare = std::move(share);
                addSourceAfterCreation(shareId, fileId, recipientType, recipientValue, recipientInstance, activateAfterCreation);
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
    const auto shareId = share->id();
    const auto job = new DeleteShareJob{_account, shareId};
    connect(job, &DeleteShareJob::jobFinished, this, [this, shareId](const QJsonDocument &, int) {
        const auto shareIterator = std::ranges::find_if(_shares, [shareId](const auto &candidate) {
            return candidate && candidate->id() == shareId;
        });
        if (shareIterator == _shares.end()) {
            setDestroyingShare(false);
            return;
        }

        _pendingDraftUpdates.remove(shareId);
        _activationRequested.remove(shareId);
        _activationBlocked.remove(shareId);
        auto removedShare = std::move(*shareIterator);
        _shares.erase(shareIterator);
        Q_UNUSED(removedShare);
        setDestroyingShare(false);
        Q_EMIT sharesChanged();
    });
    connect(job, &DeleteShareJob::ocsError, this, [this](int, const QString &message) {
        setShareDestructionError(message.isEmpty() ? tr("Could not delete the share.") : message);
        setDestroyingShare(false);
    });
    connect(job, &DeleteShareJob::networkError, this, [this](const QNetworkReply *reply) {
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

    const auto shareId = share->id();
    const auto job = new AddRecipientJob{_account, shareId, recipientType, recipientValue, optionalString(recipientInstance)};
    connect(job, &AddRecipientJob::shareUpdated, this, [this, shareId](const QJsonDocument &json) {
        if (const auto updatedShare = updateShareFromJson(shareId, json)) {
            Q_EMIT recipientAdded(updatedShare);
        }
    });
    connect(job, &AddRecipientJob::ocsError, this, [this, shareId](int, const QString &message) {
        Q_EMIT recipientAdditionFailed(shareById(shareId), message.isEmpty() ? tr("Could not add the recipient.") : message);
    });
    connect(job, &AddRecipientJob::networkError, this, [this, shareId](const QNetworkReply *reply) {
        Q_EMIT recipientAdditionFailed(shareById(shareId), reply ? reply->errorString() : tr("Could not add the recipient."));
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

    const auto shareId = share->id();
    const auto job = new RemoveRecipientJob{_account, shareId, recipientType, recipientValue, optionalString(recipientInstance)};
    connect(job, &RemoveRecipientJob::shareUpdated, this, [this, shareId](const QJsonDocument &json) {
        if (const auto updatedShare = updateShareFromJson(shareId, json)) {
            Q_EMIT recipientRemoved(updatedShare);
        }
    });
    connect(job, &RemoveRecipientJob::ocsError, this, [this, shareId](int, const QString &message) {
        Q_EMIT recipientRemovalFailed(shareById(shareId), message.isEmpty() ? tr("Could not remove the recipient.") : message);
    });
    connect(job, &RemoveRecipientJob::networkError, this, [this, shareId](const QNetworkReply *reply) {
        Q_EMIT recipientRemovalFailed(shareById(shareId), reply ? reply->errorString() : tr("Could not remove the recipient."));
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

    const auto shareId = share->id();
    const auto instance = optionalString(recipientInstance);
    const auto generateJob = new GenerateSecretJob{_account};
    connect(generateJob, &GenerateSecretJob::secretGenerated, this, [this, shareId, recipientType, recipientValue, instance](const QString &secret) {
        if (!shareById(shareId) || secret.isEmpty()) {
            Q_EMIT recipientSecretUpdateFailed(shareById(shareId), tr("The server did not generate a valid sharing link."));
            return;
        }

        const auto updateJob = new SetRecipientSecretJob{_account, shareId, recipientType, recipientValue, secret, instance};
        connect(updateJob, &SetRecipientSecretJob::shareUpdated, this, [this, shareId](const QJsonDocument &json) {
            if (const auto updatedShare = updateShareFromJson(shareId, json)) {
                Q_EMIT recipientSecretUpdated(updatedShare);
            }
        });
        connect(updateJob, &SetRecipientSecretJob::ocsError, this, [this, shareId](int, const QString &message) {
            Q_EMIT recipientSecretUpdateFailed(shareById(shareId), message.isEmpty() ? tr("Could not update the sharing link.") : message);
        });
        connect(updateJob, &SetRecipientSecretJob::networkError, this, [this, shareId](const QNetworkReply *reply) {
            Q_EMIT recipientSecretUpdateFailed(shareById(shareId), reply ? reply->errorString() : tr("Could not update the sharing link."));
        });
        updateJob->start();
    });
    connect(generateJob, &GenerateSecretJob::ocsError, this, [this, shareId](int, const QString &message) {
        Q_EMIT recipientSecretUpdateFailed(shareById(shareId), message.isEmpty() ? tr("Could not generate a sharing link.") : message);
    });
    connect(generateJob, &GenerateSecretJob::networkError, this, [this, shareId](const QNetworkReply *reply) {
        Q_EMIT recipientSecretUpdateFailed(shareById(shareId), reply ? reply->errorString() : tr("Could not generate a sharing link."));
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

    const auto shareId = share->id();
    const auto permissionFailureReported = std::make_shared<bool>(false);
    const auto job = new SetPermissionJob{_account, shareId, permissionClass, enabled};
    trackDraftUpdate(shareId, job);
    connect(job, &SetPermissionJob::shareUpdated, this, [this, shareId](const QJsonDocument &json) {
        updateShareFromJson(shareId, json);
    });
    connect(job, &SetPermissionJob::ocsError, this, [this, shareId, permissionFailureReported](int, const QString &message) {
        if (*permissionFailureReported) {
            return;
        }
        *permissionFailureReported = true;
        markDraftUpdateFailed(shareId);
        Q_EMIT permissionUpdateFailed(shareById(shareId), message.isEmpty() ? tr("Could not update the permissions.") : message);
    });
    connect(job, &SetPermissionJob::networkError, this, [this, shareId, permissionFailureReported](const QNetworkReply *reply) {
        if (*permissionFailureReported) {
            return;
        }
        *permissionFailureReported = true;
        markDraftUpdateFailed(shareId);
        Q_EMIT permissionUpdateFailed(shareById(shareId), reply ? reply->errorString() : tr("Could not update the permissions."));
    });
    job->start();
}

void SharingController::setRecipientPermission(Share *share,
                                               const QString &recipientType,
                                               const QString &recipientValue,
                                               const QString &recipientInstance,
                                               const QString &permissionClass,
                                               bool enabled)
{
    if (!_account) {
        qCWarning(lcSharingController) << "attempted to set a recipient permission without an account set";
        return;
    }

    if (!containsShare(share)) {
        qCWarning(lcSharingController) << "attempted to set a recipient permission on a share not owned by this controller";
        return;
    }

    if (recipientType.isEmpty() || recipientValue.isEmpty() || permissionClass.isEmpty()) {
        qCWarning(lcSharingController) << "attempted to set a recipient permission with incomplete identity";
        return;
    }

    const auto shareId = share->id();
    const auto permissionFailureReported = std::make_shared<bool>(false);
    const auto job =
        new SetRecipientPermissionJob{_account, shareId, recipientType, recipientValue, optionalString(recipientInstance), permissionClass, enabled};
    trackDraftUpdate(shareId, job);
    connect(job,
            &SetRecipientPermissionJob::shareUpdated,
            this,
            [this, shareId, recipientType, recipientValue, recipientInstance, permissionClass, enabled](const QJsonDocument &json) {
                const auto updatedShare = updateShareFromJson(shareId, json);
                if (!updatedShare) {
                    return;
                }

                const auto recipients = updatedShare->recipients();
                const auto recipient = std::ranges::find_if(recipients, [&recipientType, &recipientValue, &recipientInstance](const Recipient *candidate) {
                    return candidate && candidate->className() == recipientType && candidate->value() == recipientValue
                        && candidate->instanceString() == recipientInstance;
                });
                if (recipient != recipients.cend()) {
                    (*recipient)->setPermissionOverride(permissionClass, enabled);
                }
            });
    connect(job, &SetRecipientPermissionJob::ocsError, this, [this, shareId, permissionFailureReported](int, const QString &message) {
        if (*permissionFailureReported) {
            return;
        }
        *permissionFailureReported = true;
        markDraftUpdateFailed(shareId);
        Q_EMIT recipientPermissionUpdateFailed(shareById(shareId), message.isEmpty() ? tr("Could not update the recipient permissions.") : message);
    });
    connect(job, &SetRecipientPermissionJob::networkError, this, [this, shareId, permissionFailureReported](const QNetworkReply *reply) {
        if (*permissionFailureReported) {
            return;
        }
        *permissionFailureReported = true;
        markDraftUpdateFailed(shareId);
        Q_EMIT recipientPermissionUpdateFailed(shareById(shareId), reply ? reply->errorString() : tr("Could not update the recipient permissions."));
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

    const auto shareId = share->id();
    const auto permissionFailureReported = std::make_shared<bool>(false);
    const auto job = new SetPermissionPresetJob{_account, shareId, permissionPreset};
    trackDraftUpdate(shareId, job);
    connect(job, &SetPermissionPresetJob::shareUpdated, this, [this, shareId](const QJsonDocument &json) {
        updateShareFromJson(shareId, json);
    });
    connect(job, &SetPermissionPresetJob::ocsError, this, [this, shareId, permissionFailureReported](int, const QString &message) {
        if (*permissionFailureReported) {
            return;
        }
        *permissionFailureReported = true;
        markDraftUpdateFailed(shareId);
        Q_EMIT permissionUpdateFailed(shareById(shareId), message.isEmpty() ? tr("Could not update the permissions.") : message);
    });
    connect(job, &SetPermissionPresetJob::networkError, this, [this, shareId, permissionFailureReported](const QNetworkReply *reply) {
        if (*permissionFailureReported) {
            return;
        }
        *permissionFailureReported = true;
        markDraftUpdateFailed(shareId);
        Q_EMIT permissionUpdateFailed(shareById(shareId), reply ? reply->errorString() : tr("Could not update the permissions."));
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

    const auto shareId = share->id();
    const auto propertyValue = value.isEmpty() ? std::nullopt : std::optional{value};
    const auto job = new SetPropertyJob{_account, shareId, propertyClass, propertyValue};
    trackDraftUpdate(shareId, job);
    connect(job, &SetPropertyJob::shareUpdated, this, [this, shareId](const QJsonDocument &json) {
        if (const auto updatedShare = updateShareFromJson(shareId, json)) {
            Q_EMIT propertyUpdated(updatedShare);
        }
    });
    connect(job, &SetPropertyJob::ocsError, this, [this, shareId](int, const QString &message) {
        markDraftUpdateFailed(shareId);
        Q_EMIT propertyUpdateFailed(shareById(shareId), message.isEmpty() ? tr("Could not update the sharing setting.") : message);
    });
    connect(job, &SetPropertyJob::networkError, this, [this, shareId](const QNetworkReply *reply) {
        markDraftUpdateFailed(shareId);
        Q_EMIT propertyUpdateFailed(shareById(shareId), reply ? reply->errorString() : tr("Could not update the sharing setting."));
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

    if (share->state() != Share::State::Draft) {
        qCDebug(lcSharingController) << "ignoring attempt to activate a share that is not a draft";
        return;
    }

    const auto shareId = share->id();
    if (_activationRequested.contains(shareId)) {
        return;
    }

    if (_pendingDraftUpdates.value(shareId) > 0) {
        _activationRequested.insert(shareId);
        return;
    }

    startShareActivation(share);
}

void SharingController::startShareActivation(Share *share)
{
    if (!containsShare(share) || share->state() != Share::State::Draft) {
        return;
    }

    const auto shareId = share->id();
    const auto job = new SetShareStateJob{_account, shareId, Share::State::Active};
    connect(job, &SetShareStateJob::shareUpdated, this, [this, shareId](const QJsonDocument &json) {
        const auto updatedShare = updateShareFromJson(shareId, json);
        if (updatedShare && updatedShare->state() == Share::State::Active) {
            Q_EMIT shareActivated(updatedShare);
            return;
        }

        Q_EMIT shareActivationFailed(shareById(shareId), tr("The server did not activate the share."));
    });
    connect(job, &SetShareStateJob::ocsError, this, [this, shareId](int, const QString &message) {
        Q_EMIT shareActivationFailed(shareById(shareId), message.isEmpty() ? tr("Could not send the share.") : message);
    });
    connect(job, &SetShareStateJob::networkError, this, [this, shareId](const QNetworkReply *reply) {
        Q_EMIT shareActivationFailed(shareById(shareId), reply ? reply->errorString() : tr("Could not send the share."));
    });
    job->start();
}

bool SharingController::containsShare(const Share *share) const
{
    return share && std::ranges::any_of(_shares, [share](const auto &candidate) {
               return candidate.get() == share;
           });
}

Share *SharingController::shareById(const QString &shareId) const
{
    if (_draftShare && _draftShare->id() == shareId) {
        return _draftShare.get();
    }

    const auto share = std::ranges::find_if(_shares, [shareId](const auto &candidate) {
        return candidate && candidate->id() == shareId;
    });
    return share == _shares.cend() ? nullptr : share->get();
}

Share *SharingController::updateShareFromJson(const QString &shareId, const QJsonDocument &json)
{
    const auto share = shareById(shareId);
    if (share) {
        share->updateFromJson(json);
    }
    return share;
}

void SharingController::addSourceAfterCreation(const QString &shareId,
                                               const QString &fileId,
                                               const QString &recipientType,
                                               const QString &recipientValue,
                                               const QString &recipientInstance,
                                               bool activateAfterCreation)
{
    if (!shareById(shareId)) {
        failShareCreation(tr("The newly created share is no longer available."));
        return;
    }

    const auto job = new AddSourceJob{_account, shareId, fileId};
    connect(job,
            &AddSourceJob::shareUpdated,
            this,
            [this, shareId, recipientType, recipientValue, recipientInstance, activateAfterCreation](const QJsonDocument &json) {
                if (!updateShareFromJson(shareId, json)) {
                    failShareCreation(tr("The newly created share is no longer available."), shareId);
                    return;
                }

                if (recipientType.isEmpty()) {
                    finishShareCreation(shareId, activateAfterCreation);
                    return;
                }

                addRecipientAfterCreation(shareId, recipientType, recipientValue, recipientInstance, activateAfterCreation);
            });
    connect(job, &AddSourceJob::ocsError, this, [this, shareId](int, const QString &message) {
        failShareCreation(message.isEmpty() ? tr("Could not attach the item to the share.") : message, shareId);
    });
    connect(job, &AddSourceJob::networkError, this, [this, shareId](const QNetworkReply *reply) {
        failShareCreation(reply ? reply->errorString() : tr("Could not attach the item to the share."), shareId);
    });
    job->start();
}

void SharingController::addRecipientAfterCreation(const QString &shareId,
                                                  const QString &recipientType,
                                                  const QString &recipientValue,
                                                  const QString &recipientInstance,
                                                  bool activateAfterCreation)
{
    if (!shareById(shareId)) {
        failShareCreation(tr("The newly created share is no longer available."));
        return;
    }

    const auto job = new AddRecipientJob{_account, shareId, recipientType, recipientValue, optionalString(recipientInstance)};
    connect(job, &AddRecipientJob::shareUpdated, this, [this, shareId, activateAfterCreation](const QJsonDocument &json) {
        if (!updateShareFromJson(shareId, json)) {
            failShareCreation(tr("The newly created share is no longer available."), shareId);
            return;
        }
        finishShareCreation(shareId, activateAfterCreation);
    });
    connect(job, &AddRecipientJob::ocsError, this, [this, shareId](int, const QString &message) {
        failShareCreation(message.isEmpty() ? tr("Could not add the recipient.") : message, shareId);
    });
    connect(job, &AddRecipientJob::networkError, this, [this, shareId](const QNetworkReply *reply) {
        failShareCreation(reply ? reply->errorString() : tr("Could not add the recipient."), shareId);
    });
    job->start();
}

void SharingController::finishShareCreation(const QString &shareId, bool activateAfterCreation)
{
    if (!_draftShare || _draftShare->id() != shareId) {
        failShareCreation(tr("The newly created share is no longer available."), shareId);
        return;
    }

    _shares.emplace_back(std::move(_draftShare));
    const auto share = _shares.back().get();
    setCreatingShare(false);
    Q_EMIT shareCreated(share);
    Q_EMIT sharesChanged();
    if (activateAfterCreation) {
        startShareActivation(share);
    }
}

void SharingController::failShareCreation(const QString &error, const QString &shareId)
{
    if (!_creatingShare) {
        return;
    }

    setShareCreationError(error);
    setCreatingShare(false);

    if (!shareId.isEmpty()) {
        const auto cleanupJob = new DeleteShareJob{_account, shareId};
        cleanupJob->start();
    }
    _draftShare.reset();
}

void SharingController::trackDraftUpdate(const QString &shareId, QObject *job)
{
    const auto share = shareById(shareId);
    if (!share || !job || share->state() != Share::State::Draft) {
        return;
    }

    ++_pendingDraftUpdates[shareId];
    connect(job, &QObject::destroyed, this, [this, shareId] {
        const auto share = shareById(shareId);
        if (!share) {
            return;
        }

        auto pendingUpdate = _pendingDraftUpdates.find(shareId);
        if (pendingUpdate == _pendingDraftUpdates.end()) {
            return;
        }

        --pendingUpdate.value();
        if (pendingUpdate.value() > 0) {
            return;
        }

        _pendingDraftUpdates.erase(pendingUpdate);
        if (!_activationRequested.remove(shareId)) {
            return;
        }

        if (_activationBlocked.remove(shareId)) {
            Q_EMIT shareActivationFailed(share, tr("Could not save all changes to the share."));
            return;
        }

        startShareActivation(share);
    });
}

void SharingController::markDraftUpdateFailed(const QString &shareId)
{
    if (shareById(shareId) && _activationRequested.contains(shareId)) {
        _activationBlocked.insert(shareId);
    }
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

void SharingController::setResolvingInternalLink(bool resolvingInternalLink)
{
    if (_resolvingInternalLink == resolvingInternalLink) {
        return;
    }
    _resolvingInternalLink = resolvingInternalLink;
    Q_EMIT resolvingInternalLinkChanged();
}

void SharingController::setInternalLinkError(const QString &error)
{
    if (_internalLinkError == error) {
        return;
    }
    _internalLinkError = error;
    Q_EMIT internalLinkErrorChanged();
}

void SharingController::replaceShares(std::vector<std::unique_ptr<Share>> shares)
{
    _pendingDraftUpdates.clear();
    _activationRequested.clear();
    _activationBlocked.clear();
    auto oldShares = std::move(_shares);
    _shares = std::move(shares);
    Q_UNUSED(oldShares);
    Q_EMIT sharesChanged();
}
