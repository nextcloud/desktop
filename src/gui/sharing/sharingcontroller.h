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
    Q_INVOKABLE void destroyShare(Share *share);
    Q_INVOKABLE void addRecipient(Share *share, const QString &recipientType, const QString &recipientValue);
    Q_INVOKABLE void removeRecipient(Share *share, const QString &recipientType, const QString &recipientValue);

    Q_INVOKABLE void setPermission(Share *share, const QString &permissionClass, bool enabled);
    Q_INVOKABLE void setPermissionPreset(Share *share, const QString &permissionPreset);

Q_SIGNALS:
    void accountChanged();
    void sharesChanged();

    /** @brief Emitted when creatingShare changes. */
    void creatingShareChanged();

    /** @brief Emitted when shareCreationError changes. */
    void shareCreationErrorChanged();

    /** @brief Emitted after a recipient was added and the Share was updated. */
    void recipientAdded(Share *share);

    /** @brief Emitted when adding a recipient failed. */
    void recipientAdditionFailed(Share *share, const QString &error);

private:
    AccountPtr _account;
    QList<Share *> _shares;
    bool _creatingShare = false;
    QString _shareCreationError;

    [[nodiscard]] bool containsShare(const Share *share) const;
    void addSourceAfterCreation(QPointer<Share> share, const QString &fileId);
    void finishShareCreation(QPointer<Share> share);
    void failShareCreation(const QString &error, QPointer<Share> share = {});
    void setCreatingShare(bool creatingShare);
    void setShareCreationError(const QString &error);
    void replaceShares(const QList<Share *> &shares);
};

}
