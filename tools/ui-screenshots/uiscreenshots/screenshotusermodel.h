/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SCREENSHOTUSERMODEL_H
#define SCREENSHOTUSERMODEL_H

#include <QAbstractListModel>

#include "screenshotuser.h"

namespace OCC {

/** @brief Exposes one deterministic user through the `UserModel` surface needed by the target QML. */
class ScreenshotUserModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(OCC::ScreenshotUser *currentUser READ currentUser NOTIFY currentUserChanged)
    Q_PROPERTY(int currentUserId READ currentUserId CONSTANT)
    Q_PROPERTY(int count READ count CONSTANT)

public:
    /** @brief Creates the one-row screenshot user model. */
    explicit ScreenshotUserModel(QObject *parent = nullptr);

    /** @brief Returns one for the root model and zero for child indexes. */
    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    /** @brief Returns deterministic account data for the requested role. */
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    /** @brief Returns the deterministic current user. */
    [[nodiscard]] ScreenshotUser *currentUser() const;
    /** @brief Returns the sole current-user row. */
    [[nodiscard]] int currentUserId() const;
    /** @brief Returns the number of deterministic users. */
    [[nodiscard]] int count() const;
    /** @brief Returns the number of deterministic users through the production invokable. */
    Q_INVOKABLE int numUsers() const;
    /** @brief Returns whether the requested deterministic user is connected. */
    Q_INVOKABLE bool isUserConnected(int id) const;

signals:
    /** @brief Mirrors the production current-user notification; the fixture remains stable. */
    void currentUserChanged();

protected:
    /** @brief Returns production-compatible role names used by shared QML. */
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

private:
    Q_DISABLE_COPY_MOVE(ScreenshotUserModel)

    ScreenshotUser *const _user;
};

}

#endif // SCREENSHOTUSERMODEL_H
