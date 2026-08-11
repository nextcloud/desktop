/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SCREENSHOTUSERSTATUSSELECTORMODEL_H
#define SCREENSHOTUSERSTATUSSELECTORMODEL_H

#include "userstatusselectormodel.h"

#include <QObject>
#include <QUrl>
#include <QVariantList>

namespace OCC {

/** @brief Supplies deterministic online-status data through the production selector contract. */
class ScreenshotUserStatusSelectorModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int userIndex READ userIndex WRITE setUserIndex NOTIFY userIndexChanged)
    Q_PROPERTY(QString userStatusMessage READ userStatusMessage WRITE setUserStatusMessage NOTIFY userStatusChanged)
    Q_PROPERTY(QString userStatusEmoji READ userStatusEmoji WRITE setUserStatusEmoji NOTIFY userStatusChanged)
    Q_PROPERTY(int onlineStatus READ onlineStatus WRITE setOnlineStatus NOTIFY userStatusChanged)
    Q_PROPERTY(QVariantList predefinedStatuses READ predefinedStatuses CONSTANT)
    Q_PROPERTY(QVariantList clearStageTypes READ clearStageTypes CONSTANT)
    Q_PROPERTY(QString clearAtDisplayString READ clearAtDisplayString NOTIFY clearAtDisplayStringChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage CONSTANT)
    Q_PROPERTY(bool busyStatusSupported READ busyStatusSupported CONSTANT)
    Q_PROPERTY(bool userStatusLoaded READ userStatusLoaded CONSTANT)
    Q_PROPERTY(bool finishOnOnlineStatusSet READ finishOnOnlineStatusSet WRITE setFinishOnOnlineStatusSet NOTIFY finishOnOnlineStatusSetChanged)
    Q_PROPERTY(QUrl onlineIcon READ onlineIcon CONSTANT)
    Q_PROPERTY(QUrl awayIcon READ awayIcon CONSTANT)
    Q_PROPERTY(QUrl dndIcon READ dndIcon CONSTANT)
    Q_PROPERTY(QUrl busyIcon READ busyIcon CONSTANT)
    Q_PROPERTY(QUrl invisibleIcon READ invisibleIcon CONSTANT)

public:
    /** @brief Creates the deterministic loaded status model. */
    explicit ScreenshotUserStatusSelectorModel(QObject *parent = nullptr);

    /** @brief Returns the selected fictional user row. */
    [[nodiscard]] int userIndex() const;
    /** @brief Selects the fictional user while keeping fixture data loaded. */
    void setUserIndex(int userIndex);
    /** @brief Returns the fictional status message. */
    [[nodiscard]] QString userStatusMessage() const;
    /** @brief Updates the in-memory fictional status message. */
    void setUserStatusMessage(const QString &message);
    /** @brief Returns the fictional status emoji. */
    [[nodiscard]] QString userStatusEmoji() const;
    /** @brief Updates the in-memory fictional status emoji. */
    void setUserStatusEmoji(const QString &emoji);
    /** @brief Returns the selected production online-status enum value. */
    [[nodiscard]] int onlineStatus() const;
    /** @brief Updates the selected production online-status enum value. */
    void setOnlineStatus(int status);
    /** @brief Returns fictional predefined statuses. */
    [[nodiscard]] QVariantList predefinedStatuses() const;
    /** @brief Returns the production-compatible status-clearing choices. */
    [[nodiscard]] QVariantList clearStageTypes() const;
    /** @brief Returns the selected clearing choice's display text. */
    [[nodiscard]] QString clearAtDisplayString() const;
    /** @brief Returns an empty fixture error. */
    [[nodiscard]] QString errorMessage() const;
    /** @brief Returns true because the fixture supports the Busy status. */
    [[nodiscard]] bool busyStatusSupported() const;
    /** @brief Returns true because fixture data is immediately available. */
    [[nodiscard]] bool userStatusLoaded() const;
    /** @brief Returns whether online-state changes should close the window. */
    [[nodiscard]] bool finishOnOnlineStatusSet() const;
    /** @brief Updates online-state completion behavior. */
    void setFinishOnOnlineStatusSet(bool finish);
    /** @brief Returns the production Online icon. */
    [[nodiscard]] QUrl onlineIcon() const;
    /** @brief Returns the production Away icon. */
    [[nodiscard]] QUrl awayIcon() const;
    /** @brief Returns the production Do Not Disturb icon. */
    [[nodiscard]] QUrl dndIcon() const;
    /** @brief Returns the production Busy icon. */
    [[nodiscard]] QUrl busyIcon() const;
    /** @brief Returns the production Invisible icon. */
    [[nodiscard]] QUrl invisibleIcon() const;

    /** @brief Returns readable clearing text from a predefined status map. */
    Q_INVOKABLE QString clearAtReadable(const QVariant &status) const;
    /** @brief Completes a fictional status update without network access. */
    Q_INVOKABLE void setUserStatus();
    /** @brief Clears the in-memory fictional message and emoji. */
    Q_INVOKABLE void clearUserStatus();
    /** @brief Selects a production-compatible clearing choice. */
    Q_INVOKABLE void setClearAt(const QVariant &clearStageType);
    /** @brief Applies a fictional predefined status map. */
    Q_INVOKABLE void setPredefinedStatus(const QVariant &predefinedStatus);

signals:
    /** @brief Notifies QML that the selected user changed. */
    void userIndexChanged();
    /** @brief Notifies QML that status data changed. */
    void userStatusChanged();
    /** @brief Notifies QML that the clearing display text changed. */
    void clearAtDisplayStringChanged();
    /** @brief Notifies QML that completion behavior changed. */
    void finishOnOnlineStatusSetChanged();
    /** @brief Mirrors successful completion of a production status operation. */
    void finished();

private:
    Q_DISABLE_COPY_MOVE(ScreenshotUserStatusSelectorModel)

    int _userIndex = -1;
    int _onlineStatus = 0;
    bool _finishOnOnlineStatusSet = true;
    QString _message = QStringLiteral("Working on documentation");
    QString _emoji = QStringLiteral("📝");
    UserStatusSelectorModel::ClearStageType _clearStageType = UserStatusSelectorModel::ClearStageType::OneHour;
};

}

#endif // SCREENSHOTUSERSTATUSSELECTORMODEL_H
