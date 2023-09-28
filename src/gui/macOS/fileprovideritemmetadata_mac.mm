/*
 * Copyright (C) 2023 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "fileprovideritemmetadata.h"

#import <Foundation/Foundation.h>
#import <FileProvider/NSFileProviderItem.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

namespace {

QString nsNameComponentsToLocalisedQString(NSPersonNameComponents *const nameComponents)
{
    if (nameComponents == nil) {
        return {};
    }

    NSString *const name = [NSPersonNameComponentsFormatter localizedStringFromPersonNameComponents:nameComponents style:NSPersonNameComponentsFormatterStyleDefault options:0];
    return QString::fromNSString(name);
}

QHash<QString, QByteArray> extendedAttributesToHash(NSDictionary<NSString *, NSData *> *const extendedAttributes)
{
    QHash<QString, QByteArray> hash;
    for (NSString *const key in extendedAttributes) {
        NSData *const value = [extendedAttributes objectForKey:key];
        hash.insert(QString::fromNSString(key), QByteArray::fromNSData(value));
    }
    return hash;
}

}

namespace OCC {

namespace Mac {

FileProviderItemMetadata FileProviderItemMetadata::fromNSFileProviderItem(const void *const nsFileProviderItem)
{
    FileProviderItemMetadata metadata;
    const id<NSFileProviderItem> bridgedNsFileProviderItem = (__bridge id<NSFileProviderItem>)nsFileProviderItem;
    if (bridgedNsFileProviderItem == nil) {
        return {};
    }

    metadata._identifier = QString::fromNSString(bridgedNsFileProviderItem.itemIdentifier);
    metadata._parentItemIdentifier = QString::fromNSString(bridgedNsFileProviderItem.parentItemIdentifier);
    metadata._filename = QString::fromNSString(bridgedNsFileProviderItem.filename);
    metadata._typeIdentifier = QString::fromNSString(bridgedNsFileProviderItem.contentType.identifier);
    metadata._symlinkTargetPath = QString::fromNSString(bridgedNsFileProviderItem.symlinkTargetPath);
    metadata._uploadingError = QString::fromNSString(bridgedNsFileProviderItem.uploadingError.localizedDescription);
    metadata._downloadingError = QString::fromNSString(bridgedNsFileProviderItem.downloadingError.localizedDescription);
    metadata._mostRecentEditorName = nsNameComponentsToLocalisedQString(bridgedNsFileProviderItem.mostRecentEditorNameComponents);
    metadata._ownerName = nsNameComponentsToLocalisedQString(bridgedNsFileProviderItem.ownerNameComponents);
    metadata._contentModificationDate = QDateTime::fromNSDate(bridgedNsFileProviderItem.contentModificationDate);
    metadata._creationDate = QDateTime::fromNSDate(bridgedNsFileProviderItem.creationDate);
    metadata._lastUsedDate = QDateTime::fromNSDate(bridgedNsFileProviderItem.lastUsedDate);
    metadata._contentVersion = QByteArray::fromNSData(bridgedNsFileProviderItem.itemVersion.contentVersion);
    metadata._metadataVersion = QByteArray::fromNSData(bridgedNsFileProviderItem.itemVersion.metadataVersion);
    metadata._tagData = QByteArray::fromNSData(bridgedNsFileProviderItem.tagData);
    metadata._extendedAttributes = extendedAttributesToHash(bridgedNsFileProviderItem.extendedAttributes);
    metadata._capabilities = bridgedNsFileProviderItem.capabilities;
    metadata._fileSystemFlags = bridgedNsFileProviderItem.fileSystemFlags;
    metadata._childItemCount = bridgedNsFileProviderItem.childItemCount.unsignedIntegerValue;
    metadata._typeOsCode = bridgedNsFileProviderItem.typeAndCreator.type;
    metadata._creatorOsCode = bridgedNsFileProviderItem.typeAndCreator.creator;
    metadata._documentSize = bridgedNsFileProviderItem.documentSize.unsignedLongLongValue;
    metadata._mostRecentVersionDownloaded = bridgedNsFileProviderItem.mostRecentVersionDownloaded;
    metadata._uploading = bridgedNsFileProviderItem.uploading;
    metadata._uploaded = bridgedNsFileProviderItem.uploaded;
    metadata._downloading = bridgedNsFileProviderItem.downloading;
    metadata._downloaded = bridgedNsFileProviderItem.downloaded;
    metadata._shared = bridgedNsFileProviderItem.shared;
    metadata._sharedByCurrentUser = bridgedNsFileProviderItem.sharedByCurrentUser;

    return metadata;
}

}

}
