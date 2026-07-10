/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QAbstractListModel>
#include <QJsonDocument>

namespace OCC {

/** @brief Exposes supported Assistant task-processing types as typed QML roles. */
class AssistantTaskTypeModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role : int {
        TypeIdRole = Qt::UserRole + 1,
        NameRole,
        DescriptionRole,
        IsChatRole,
    };
    Q_ENUM(Role)

    /** @brief Creates an empty task-type model. */
    explicit AssistantTaskTypeModel(QObject *parent = nullptr);

    /** @brief Returns the number of supported task types. */
    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    /** @brief Returns task-type data for a model role. */
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    /** @brief Returns the QML role names. */
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    /** @brief Replaces the model from a task-type response. */
    void replaceFromResponse(const QJsonDocument &json);
    /** @brief Returns whether the model contains a task type. */
    [[nodiscard]] bool contains(const QString &typeId) const;
    /** @brief Returns the first task type identifier, or an empty string. */
    [[nodiscard]] QString firstTypeId() const;
    /** @brief Returns the display name for a task type. */
    [[nodiscard]] QString nameForType(const QString &typeId) const;
    /** @brief Returns the description for a task type. */
    [[nodiscard]] QString descriptionForType(const QString &typeId) const;

private:
    struct Item {
        QString typeId;
        QString name;
        QString description;
        bool isChat = false;
    };

    QList<Item> _items;

    Q_DISABLE_COPY_MOVE(AssistantTaskTypeModel)
};

}
