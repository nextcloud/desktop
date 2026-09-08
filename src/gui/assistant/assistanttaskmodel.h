/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QAbstractListModel>
#include <QJsonDocument>

#include <optional>

namespace OCC {

/** @brief Exposes Assistant task-processing tasks as typed QML roles. */
class AssistantTaskModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role : int {
        TaskIdRole = Qt::UserRole + 1,
        TypeRole,
        AppIdRole,
        InputRole,
        OutputRole,
        StatusRole,
        StatusTextRole,
        DateTextRole,
    };
    Q_ENUM(Role)

    /** @brief Creates an empty task model. */
    explicit AssistantTaskModel(QObject *parent = nullptr);

    /** @brief Returns the number of tasks. */
    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    /** @brief Returns task data for a model role. */
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    /** @brief Returns the QML role names. */
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    /** @brief Replaces the model with Assistant tasks matching the requested type. */
    void replaceFromResponse(const QJsonDocument &json, const QString &taskType);
    /** @brief Removes all tasks from the model. */
    void clear();
    /** @brief Returns the input of a task, or an empty string when absent. */
    [[nodiscard]] QString inputForTask(qint64 taskId) const;
    /** @brief Returns whether a task is running, or no value when it is absent. */
    [[nodiscard]] std::optional<bool> runningState(qint64 taskId) const;

private:
    struct Item {
        qint64 taskId = -1;
        QString type;
        QString appId;
        QString input;
        QString output;
        QString status;
        QString statusText;
        QString dateText;
        bool running = false;
    };

    QList<Item> _items;

    Q_DISABLE_COPY_MOVE(AssistantTaskModel)
};

}
