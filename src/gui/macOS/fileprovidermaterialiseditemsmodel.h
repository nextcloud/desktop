/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QAbstractListModel>
#include <QLocale>

#include "gui/macOS/fileprovideritemmetadata.h"

namespace OCC {

namespace Mac {

class FileProviderMaterialisedItemsModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(QVector<FileProviderItemMetadata> items READ items WRITE setItems NOTIFY itemsChanged)

public:
    enum Roles {
        IdentifierRole = Qt::UserRole + 1,
        ParentItemIdentifierRole,
        DomainIdentifierRole,
        FilenameRole,
        TypeIdentifierRole,
        SymlinkTargetPathRole,
        UploadingErrorRole,
        DownloadingErrorRole,
        MostRecentEditorNameRole,
        OwnerNameRole,
        ContentModificationDateRole,
        CreationDateRole,
        LastUsedDateRole,
        ContentVersionRole,
        MetadataVersionRole,
        TagDataRole,
        CapabilitiesRole,
        FileSystemFlagsRole,
        ChildItemCountRole,
        TypeOsCodeRole,
        CreatorOsCodeRole,
        DocumentSizeRole,
        MostRecentVersionDownloadedRole,
        UploadingRole,
        UploadedRole,
        DownloadingRole,
        DownloadedRole,
        SharedRole,
        SharedByCurrentUserRole,
        UserVisiblePathRole,
        FileTypeStringRole,
        FileSizeStringRole,
    };
    Q_ENUM(Roles)

    explicit FileProviderMaterialisedItemsModel(QObject *parent = nullptr);
    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QVector<FileProviderItemMetadata> items() const;

signals:
    void itemsChanged();

public slots:
    void setItems(const QVector<FileProviderItemMetadata> &items);
    void evictItem(const QString &identifier, const QString &domainIdentifier);

private:
    QVector<FileProviderItemMetadata> _items;
    QLocale _locale;
};

} // namespace Mac

} // namespace OCC
