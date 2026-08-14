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
 * @brief Exposes unified sharing section headers, actions, and shares in display order.
 */
class UnifiedShareListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(SharingController *sharingController READ sharingController WRITE setSharingController NOTIFY sharingControllerChanged)

public:
    /** @brief Identifies the delegate needed for a list row. */
    enum class ItemType {
        SectionHeader,
        Share,
        InternalLink,
        CreatePublicLink,
    };
    Q_ENUM(ItemType)

    /** @brief Roles exposed to the sharing list delegates. */
    enum Role {
        ShareRole = Qt::UserRole + 1,
        SectionRole,
        RecipientNamesRole,
        ItemTypeRole,
        PublicLinkRole,
        PublicLinkUrlRole,
    };
    Q_ENUM(Role)

    /** @brief Creates an empty model. */
    explicit UnifiedShareListModel(QObject *parent = nullptr);

    /** @brief Returns the controller that supplies the shares. */
    [[nodiscard]] SharingController *sharingController() const;

    /** @brief Sets the controller whose shares are exposed by this model. */
    void setSharingController(SharingController *sharingController);

    /** @brief Returns the number of headers, actions, and shares in the root list. */
    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;

    /** @brief Returns the requested display data for a model index. */
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;

    /** @brief Returns the QML role names exposed by the model. */
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

Q_SIGNALS:
    /** @brief Emitted when the source controller changes. */
    void sharingControllerChanged();

private:
    struct Item
    {
        ItemType type;
        QString section;
        QPointer<Share> share;
    };

    QPointer<SharingController> _sharingController;
    QList<Item> _items;
    QList<QMetaObject::Connection> _shareConnections;

    void rebuild();
    [[nodiscard]] static QString sectionForShare(const Share *share);
    [[nodiscard]] static bool isInternalShare(const Share *share);
    [[nodiscard]] static bool isExternalShare(const Share *share);
};

}
