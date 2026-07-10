/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QAbstractListModel>
#include <QJsonDocument>
#include <QJsonObject>

namespace OCC {

/** @brief Exposes Assistant chat conversations as typed QML roles. */
class AssistantConversationModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role : int {
        ConversationIdRole = Qt::UserRole + 1,
        TitleRole,
        TimestampRole,
        SelectedRole,
    };
    Q_ENUM(Role)

    /** @brief Creates an empty conversation model. */
    explicit AssistantConversationModel(QObject *parent = nullptr);

    /** @brief Returns the number of conversations. */
    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    /** @brief Returns conversation data for a model role. */
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    /** @brief Returns the QML role names. */
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    /** @brief Replaces the model from a chat-session response. */
    void replaceFromResponse(const QJsonDocument &json, qint64 selectedConversationId);
    /** @brief Prepends a newly created conversation. */
    void prepend(const QJsonObject &conversation, qint64 selectedConversationId);
    /** @brief Updates the selected role for all conversations. */
    void select(qint64 conversationId);
    /** @brief Returns a conversation title, or an empty string when absent. */
    [[nodiscard]] QString titleForConversation(qint64 conversationId) const;

private:
    struct Item {
        qint64 conversationId = -1;
        QString title;
        qint64 timestamp = 0;
        bool selected = false;
    };

    [[nodiscard]] static Item itemFromJson(const QJsonObject &conversation, qint64 selectedConversationId);

    QList<Item> _items;

    Q_DISABLE_COPY_MOVE(AssistantConversationModel)
};

}
