/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fileprovidermaterialiseditemsmodel.h"

#include <QFileInfo>

namespace OCC {

namespace Mac {

FileProviderMaterialisedItemsModel::FileProviderMaterialisedItemsModel(QObject * const parent)
    : QAbstractListModel(parent)
{
}

int FileProviderMaterialisedItemsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return _items.count();
}

QVariant FileProviderMaterialisedItemsModel::data(const QModelIndex &index, int role) const
{
    const auto item = _items.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
    case FilenameRole:
        return item.filename();
    case IdentifierRole:
        return item.identifier();
    case ParentItemIdentifierRole:
        return item.parentItemIdentifier();
    case DomainIdentifierRole:
        return item.domainIdentifier();
    case TypeIdentifierRole:
        return item.typeIdentifier();
    case SymlinkTargetPathRole:
        return item.symlinkTargetPath();
    case UploadingErrorRole:
        return item.uploadingError();
    case DownloadingErrorRole:
        return item.downloadingError();
    case MostRecentEditorNameRole:
        return item.mostRecentEditorName();
    case OwnerNameRole:
        return item.ownerName();
    case ContentModificationDateRole:
        return item.contentModificationDate();
    case CreationDateRole:
        return item.creationDate();
    case LastUsedDateRole:
        return item.lastUsedDate();
    case ContentVersionRole:
        return item.contentVersion();
    case MetadataVersionRole:
        return item.metadataVersion();
    case TagDataRole:
        return item.tagData();
    case CapabilitiesRole:
        return item.capabilities();
    case FileSystemFlagsRole:
        return item.fileSystemFlags();
    case ChildItemCountRole:
        return item.childItemCount();
    case TypeOsCodeRole:
        return item.typeOsCode();
    case CreatorOsCodeRole:
        return item.creatorOsCode();
    case DocumentSizeRole:
        return item.documentSize();
    case MostRecentVersionDownloadedRole:
        return item.mostRecentVersionDownloaded();
    case UploadingRole:
        return item.uploading();
    case UploadedRole:
        return item.uploaded();
    case DownloadingRole:
        return item.downloading();
    case DownloadedRole:
        return item.downloaded();
    case SharedRole:
        return item.shared();
    case SharedByCurrentUserRole:
        return item.sharedByCurrentUser();
    case UserVisiblePathRole:
        return item.userVisiblePath();
    case FileTypeStringRole:
        return item.fileTypeString();
    case FileSizeStringRole:
        return _locale.formattedDataSize(item.documentSize());
    }
    return {};
}

QHash<int, QByteArray> FileProviderMaterialisedItemsModel::roleNames() const
{
    auto roleNames = QAbstractListModel::roleNames();
    roleNames.insert({
        { IdentifierRole, "identifier" },
        { ParentItemIdentifierRole, "parentItemIdentifier" },
        { DomainIdentifierRole, "domainIdentifier" },
        { FilenameRole, "fileName" },
        { TypeIdentifierRole, "typeIdentifier" },
        { SymlinkTargetPathRole, "symlinkTargetPath" },
        { UploadingErrorRole, "uploadingError" },
        { DownloadingErrorRole, "downloadingError" },
        { MostRecentEditorNameRole, "mostRecentEditorName" },
        { OwnerNameRole, "ownerName" },
        { ContentModificationDateRole, "contentModificationDate" },
        { CreationDateRole, "creationDate" },
        { LastUsedDateRole, "lastUsedDate" },
        { ContentVersionRole, "contentVersion" },
        { MetadataVersionRole, "metadataVersion" },
        { TagDataRole, "tagData" },
        { CapabilitiesRole, "capabilities" },
        { FileSystemFlagsRole, "fileSystemFlags" },
        { ChildItemCountRole, "childItemCount" },
        { TypeOsCodeRole, "typeOsCode" },
        { CreatorOsCodeRole, "creatorOsCode" },
        { DocumentSizeRole, "documentSize" },
        { MostRecentVersionDownloadedRole, "mostRecentVersionDownloaded" },
        { UploadingRole, "uploading" },
        { UploadedRole, "uploaded" },
        { DownloadingRole, "downloading" },
        { DownloadedRole, "downloaded" },
        { SharedRole, "shared" },
        { SharedByCurrentUserRole, "sharedByCurrentUser" },
        { UserVisiblePathRole, "userVisiblePath" },
        { FileTypeStringRole, "fileTypeString" },
        { FileSizeStringRole, "fileSizeString" },
    });
    return roleNames;
}

QVector<FileProviderItemMetadata> FileProviderMaterialisedItemsModel::items() const
{
    return _items;
}

void FileProviderMaterialisedItemsModel::setItems(const QVector<FileProviderItemMetadata> &items)
{
    if (items == _items) {
        return;
    }

    beginResetModel();
    // We are using this in the "Free up space" dialog so, sort by size.
    _items = items;
    std::ranges::sort(_items, [](const auto &a, const auto &b) {
        return a.documentSize() > b.documentSize();
    });
    endResetModel();

    Q_EMIT itemsChanged();
}

} // namespace Mac

} // namespace OCC
