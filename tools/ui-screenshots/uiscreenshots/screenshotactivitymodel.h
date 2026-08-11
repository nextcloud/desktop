/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SCREENSHOTACTIVITYMODEL_H
#define SCREENSHOTACTIVITYMODEL_H

#include <QAbstractListModel>

namespace OCC {

/** @brief Supplies fictional activities through the role surface used by the target production QML. */
class ScreenshotActivityModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(quint32 maxActionButtons READ maxActionButtons CONSTANT)
    Q_PROPERTY(bool hasSyncConflicts READ hasSyncConflicts CONSTANT)
    Q_PROPERTY(QVariantList allConflicts READ allConflicts CONSTANT)

public:
    /** @brief Creates the deterministic activity source model. */
    explicit ScreenshotActivityModel(QObject *parent = nullptr);

    /** @brief Returns the number of fictional activity rows. */
    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    /** @brief Returns the requested production-compatible activity role. */
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    /** @brief Returns the production maximum for primary action buttons. */
    [[nodiscard]] quint32 maxActionButtons() const;
    /** @brief Returns false for the settled fixture. */
    [[nodiscard]] bool hasSyncConflicts() const;
    /** @brief Returns an empty conflict list. */
    [[nodiscard]] QVariantList allConflicts() const;

    /** @brief No-op default activity action. */
    Q_INVOKABLE void slotTriggerDefaultAction(int activityIndex);
    /** @brief No-op explicit activity action. */
    Q_INVOKABLE void slotTriggerAction(int activityIndex, int actionIndex);
    /** @brief No-op activity dismissal action. */
    Q_INVOKABLE void slotTriggerDismiss(int activityIndex);
    /** @brief No-op Talk reply action. */
    Q_INVOKABLE void sendReplyMessage(int activityIndex, const QString &conversationToken, const QString &message, const QString &replyTo);

signals:
    /** @brief Mirrors the production model's live-activity notification. */
    void interactiveActivityReceived();

protected:
    /** @brief Returns the production activity role names. */
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

private:
    Q_DISABLE_COPY_MOVE(ScreenshotActivityModel)
};

}

#endif // SCREENSHOTACTIVITYMODEL_H
