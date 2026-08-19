/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QtQmlIntegration>

#include "accountfwd.h"

namespace OCC::Gui::Sharing
{

class Share;

class SharingController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(AccountPtr account READ account WRITE setAccount NOTIFY accountChanged)
    Q_PROPERTY(QList<Share *> shares READ shares NOTIFY sharesChanged)
    Q_PROPERTY(bool creatingShare READ creatingShare NOTIFY creatingShareChanged)
    Q_PROPERTY(QString shareCreationError READ shareCreationError NOTIFY shareCreationErrorChanged)
    Q_PROPERTY(bool destroyingShare READ destroyingShare NOTIFY destroyingShareChanged)
    Q_PROPERTY(QString shareDestructionError READ shareDestructionError NOTIFY shareDestructionErrorChanged)
    Q_PROPERTY(bool resolvingInternalLink READ resolvingInternalLink NOTIFY resolvingInternalLinkChanged)
    Q_PROPERTY(QString internalLinkError READ internalLinkError NOTIFY internalLinkErrorChanged)

public:
    SharingController(QObject *parent = nullptr);
    ~SharingController() override;

    [[nodiscard]] AccountPtr account() const;
    void setAccount(AccountPtr account);

    /** @brief Returns all shares associated with the initialized file. */
    [[nodiscard]] const QList<Share *> &shares() const;

    /** @brief Returns whether a share, its source, and its initial recipient are currently being created. */
    [[nodiscard]] bool creatingShare() const;

    /** @brief Returns the last share creation error, or an empty string after a new attempt starts. */
    [[nodiscard]] QString shareCreationError() const;

    /** @brief Returns whether a share is currently being deleted. */
    [[nodiscard]] bool destroyingShare() const;

    /** @brief Returns the last share deletion error, or an empty string after a new attempt starts. */
    [[nodiscard]] QString shareDestructionError() const;

    /** @brief Returns whether the item's internal link is currently being resolved. */
    [[nodiscard]] bool resolvingInternalLink() const;

    /** @brief Returns the last internal-link resolution error. */
    [[nodiscard]] QString internalLinkError() const;

    /**
     * @brief Loads all shares associated with a file without creating a share.
     *
     * @param fileId Server file ID used to filter the shares request
    */
    Q_INVOKABLE void initialize(const QString &fileId);

    /**
     * @brief Creates a draft share for one recipient and attaches the specified file.
     *
     * The draft is exposed only after the share, source, and recipient requests
     * all succeed.
     *
     * @param fileId Server file ID to attach to the new share
     * @param recipientType Server-defined recipient class
     * @param recipientValue Server-defined recipient identifier
     * @param recipientInstance Remote server identifying a federated recipient, or an empty string for a local recipient
     */
    Q_INVOKABLE void createShareForRecipient(const QString &fileId,
                                             const QString &recipientType,
                                             const QString &recipientValue,
                                             const QString &recipientInstance = {});

    /** @brief Creates and activates a public-link share for the specified file. */
    Q_INVOKABLE void createPublicLink(const QString &fileId);

    /**
     * @brief Resolves the server-provided internal link for the specified file.
     *
     * @param remotePath Path of the file relative to the account's WebDAV root
     * @param numericFileId Numeric file ID used if the server does not expose a private-link property
     */
    Q_INVOKABLE void requestInternalLink(const QString &remotePath, const QString &numericFileId);

    /** @brief Permanently removes a share managed by this controller. */
    Q_INVOKABLE void destroyShare(Share *share);

    /**
     * @brief Adds a recipient to a share.
     *
     * @param recipientInstance Remote server identifying a federated recipient, or an empty string for a local recipient
     */
    Q_INVOKABLE void addRecipient(Share *share,
                                  const QString &recipientType,
                                  const QString &recipientValue,
                                  const QString &recipientInstance = {});

    /**
     * @brief Removes a recipient from a share.
     *
     * @param recipientInstance Remote server identifying a federated recipient, or an empty string for a local recipient
     */
    Q_INVOKABLE void removeRecipient(Share *share,
                                     const QString &recipientType,
                                     const QString &recipientValue,
                                     const QString &recipientInstance = {});

    /**
     * @brief Generates and assigns a new secret to a recipient.
     *
     * @param recipientInstance Remote server identifying a federated recipient, or an empty string for a local recipient
     */
    Q_INVOKABLE void updateRecipientSecret(Share *share,
                                           const QString &recipientType,
                                           const QString &recipientValue,
                                           const QString &recipientInstance = {});

    Q_INVOKABLE void setPermission(Share *share, const QString &permissionClass, bool enabled);
    Q_INVOKABLE void setPermissionPreset(Share *share, const QString &permissionPreset);
    Q_INVOKABLE void setProperty(Share *share, const QString &propertyClass, const QString &value);

    /** @brief Activates a draft share, making it available to its recipients. */
    Q_INVOKABLE void activateShare(Share *share);

Q_SIGNALS:
    void accountChanged();
    void sharesChanged();

    /** @brief Emitted when creatingShare changes. */
    void creatingShareChanged();

    /** @brief Emitted when shareCreationError changes. */
    void shareCreationErrorChanged();

    /** @brief Emitted after a draft share and its requested source and recipient have been created. */
    void shareCreated(Share *share);

    /** @brief Emitted when destroyingShare changes. */
    void destroyingShareChanged();

    /** @brief Emitted when shareDestructionError changes. */
    void shareDestructionErrorChanged();

    /** @brief Emitted when resolvingInternalLink changes. */
    void resolvingInternalLinkChanged();

    /** @brief Emitted when internalLinkError changes. */
    void internalLinkErrorChanged();

    /** @brief Emitted with the server-provided internal link after it is resolved. */
    void internalLinkResolved(const QString &url);

    /** @brief Emitted after a recipient was added and the Share was updated. */
    void recipientAdded(Share *share);

    /** @brief Emitted when adding a recipient failed. */
    void recipientAdditionFailed(Share *share, const QString &error);

    /** @brief Emitted after a recipient was removed and the Share was updated. */
    void recipientRemoved(Share *share);

    /** @brief Emitted when removing a recipient failed. */
    void recipientRemovalFailed(Share *share, const QString &error);

    /** @brief Emitted after a recipient secret was generated and applied. */
    void recipientSecretUpdated(Share *share);

    /** @brief Emitted when generating or applying a recipient secret failed. */
    void recipientSecretUpdateFailed(Share *share, const QString &error);

    /** @brief Emitted after a share property was updated. */
    void propertyUpdated(Share *share);

    /** @brief Emitted when updating a share property failed. */
    void propertyUpdateFailed(Share *share, const QString &error);

    /** @brief Emitted when updating an individual permission or permission preset failed. */
    void permissionUpdateFailed(Share *share, const QString &error);

    /** @brief Emitted after a draft share was activated. */
    void shareActivated(Share *share);

    /** @brief Emitted when activating a draft share failed. */
    void shareActivationFailed(Share *share, const QString &error);

private:
    AccountPtr _account;
    QList<Share *> _shares;
    bool _creatingShare = false;
    QString _shareCreationError;
    bool _destroyingShare = false;
    QString _shareDestructionError;
    bool _resolvingInternalLink = false;
    QString _internalLinkError;
    QHash<Share *, int> _pendingDraftUpdates;
    QSet<Share *> _activationRequested;
    QSet<Share *> _activationBlocked;

    [[nodiscard]] bool containsShare(const Share *share) const;
    [[nodiscard]] bool beginShareCreation(const QString &fileId);
    void startShareCreation(const QString &fileId,
                            const QString &recipientType,
                            const QString &recipientValue,
                            const QString &recipientInstance,
                            bool activateAfterCreation = false);
    void addSourceAfterCreation(QPointer<Share> share,
                                const QString &fileId,
                                const QString &recipientType,
                                const QString &recipientValue,
                                const QString &recipientInstance,
                                bool activateAfterCreation);
    void addRecipientAfterCreation(QPointer<Share> share,
                                   const QString &recipientType,
                                   const QString &recipientValue,
                                   const QString &recipientInstance,
                                   bool activateAfterCreation);
    void finishShareCreation(QPointer<Share> share, bool activateAfterCreation);
    void failShareCreation(const QString &error, QPointer<Share> share = {});
    void trackDraftUpdate(Share *share, QObject *job);
    void markDraftUpdateFailed(Share *share);
    void startShareActivation(Share *share);
    void setCreatingShare(bool creatingShare);
    void setShareCreationError(const QString &error);
    void setDestroyingShare(bool destroyingShare);
    void setShareDestructionError(const QString &error);
    void setResolvingInternalLink(bool resolvingInternalLink);
    void setInternalLinkError(const QString &error);
    void replaceShares(const QList<Share *> &shares);
};

}
