/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QAbstractListModel>
#include <QJsonDocument>
#include <QJsonObject>

namespace OCC {

/** @brief Exposes Assistant chat messages as typed QML roles. */
class AssistantMessageModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role : int {
        MessageIdRole = Qt::UserRole + 1,
        SessionIdRole,
        MessageRole,
        TextRole,
        TimestampRole,
        DateTextRole,
    };
    Q_ENUM(Role)

    /** @brief Creates an empty message model. */
    explicit AssistantMessageModel(QObject *parent = nullptr);

    /** @brief Returns the number of messages. */
    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    /** @brief Returns message data for a model role. */
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    /** @brief Returns the QML role names. */
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    /** @brief Replaces the model from a chat-message response. */
    void replaceFromResponse(const QJsonDocument &json);
    /** @brief Appends one chat message. */
    void append(const QJsonObject &message);
    /** @brief Removes all messages. */
    void clear();
    /** @brief Returns whether the final message was sent by a human. */
    [[nodiscard]] bool lastMessageIsHuman() const;

private:
    struct Item {
        qint64 messageId = -1;
        qint64 sessionId = -1;
        QString role;
        QString text;
        qint64 timestamp = 0;
        QString dateText;
    };

    [[nodiscard]] static Item itemFromJson(const QJsonObject &message);

    QList<Item> _items;

    Q_DISABLE_COPY_MOVE(AssistantMessageModel)
};

}
