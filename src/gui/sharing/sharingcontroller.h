/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <qqmlintegration.h>

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

public:
    SharingController(QObject *parent = nullptr);
    ~SharingController() override;

    [[nodiscard]] AccountPtr account() const;
    void setAccount(AccountPtr account);

    /** @brief Returns all shares associated with the initialized file. */
    [[nodiscard]] const QList<Share *> &shares() const;

    /** @brief Returns whether a share and its source are currently being created. */
    [[nodiscard]] bool creatingShare() const;

    /** @brief Returns the last share creation error, or an empty string after a new attempt starts. */
    [[nodiscard]] QString shareCreationError() const;

    /** @brief Returns whether a share is currently being deleted. */
    [[nodiscard]] bool destroyingShare() const;

    /** @brief Returns the last share deletion error, or an empty string after a new attempt starts. */
    [[nodiscard]] QString shareDestructionError() const;

    /**
     * @brief Loads all shares associated with a file without creating a share.
     *
     * @param fileId Server file ID used to filter the shares request
    */
    Q_INVOKABLE void initialize(const QString &fileId);

    /**
     * @brief Creates a draft share and attaches the specified file as its source.
     *
     * The share is added to shares only after both requests succeed. A second
     * call while creation is in progress is ignored.
     *
     * @param fileId Server file ID to attach to the new share
    */
    Q_INVOKABLE void createShare(const QString &fileId);

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

    /** @brief Emitted when destroyingShare changes. */
    void destroyingShareChanged();

    /** @brief Emitted when shareDestructionError changes. */
    void shareDestructionErrorChanged();

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

    [[nodiscard]] bool containsShare(const Share *share) const;
    void addSourceAfterCreation(QPointer<Share> share, const QString &fileId);
    void finishShareCreation(QPointer<Share> share);
    void failShareCreation(const QString &error, QPointer<Share> share = {});
    void setCreatingShare(bool creatingShare);
    void setShareCreationError(const QString &error);
    void setDestroyingShare(bool destroyingShare);
    void setShareDestructionError(const QString &error);
    void replaceShares(const QList<Share *> &shares);
};

}
