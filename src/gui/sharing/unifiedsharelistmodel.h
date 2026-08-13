/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QAbstractListModel>
#include <QMetaObject>
#include <QPointer>

#include <qqmlintegration.h>

namespace OCC::Gui::Sharing
{

class Share;
class SharingController;

/**
 * @brief Exposes unified shares in the order needed by the categorized sharing list.
 */
class UnifiedShareListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(SharingController *sharingController READ sharingController WRITE setSharingController NOTIFY sharingControllerChanged)

public:
    /** @brief Roles exposed to the sharing list delegates. */
    enum Role {
        ShareRole = Qt::UserRole + 1,
        SectionRole,
    };
    Q_ENUM(Role)

    /** @brief Creates an empty model. */
    explicit UnifiedShareListModel(QObject *parent = nullptr);

    /** @brief Returns the controller that supplies the shares. */
    [[nodiscard]] SharingController *sharingController() const;

    /** @brief Sets the controller whose shares are exposed by this model. */
    void setSharingController(SharingController *sharingController);

    /** @brief Returns the number of shares in the root list. */
    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;

    /** @brief Returns the share or section value for a model index. */
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;

    /** @brief Returns the QML role names exposed by the model. */
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

Q_SIGNALS:
    /** @brief Emitted when the source controller changes. */
    void sharingControllerChanged();

private:
    QPointer<SharingController> _sharingController;
    QList<Share *> _shares;
    QList<QMetaObject::Connection> _shareConnections;

    void rebuild();
    [[nodiscard]] static bool isExternalShare(const Share *share);
};

}
