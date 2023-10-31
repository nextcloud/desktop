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

namespace OCC {

namespace Mac {

QString FileProviderItemMetadata::identifier() const
{
    return _identifier;
}

QString FileProviderItemMetadata::parentItemIdentifier() const
{
    return _parentItemIdentifier;
}

QString FileProviderItemMetadata::domainIdentifier() const
{
    return _domainIdentifier;
}

QString FileProviderItemMetadata::filename() const
{
    return _filename;
}

QString FileProviderItemMetadata::typeIdentifier() const
{
    return _typeIdentifier;
}

QString FileProviderItemMetadata::symlinkTargetPath() const
{
    return _symlinkTargetPath;
}

QString FileProviderItemMetadata::uploadingError() const
{
    return _uploadingError;
}

QString FileProviderItemMetadata::downloadingError() const
{
    return _downloadingError;
}

QString FileProviderItemMetadata::mostRecentEditorName() const
{
    return _mostRecentEditorName;
}

QString FileProviderItemMetadata::ownerName() const
{
    return _ownerName;
}

QDateTime FileProviderItemMetadata::contentModificationDate() const
{
    return _contentModificationDate;
}

QDateTime FileProviderItemMetadata::creationDate() const
{
    return _creationDate;
}

QDateTime FileProviderItemMetadata::lastUsedDate() const
{
    return _lastUsedDate;
}

QByteArray FileProviderItemMetadata::contentVersion() const
{
    return _contentVersion;
}

QByteArray FileProviderItemMetadata::metadataVersion() const
{
    return _metadataVersion;
}

QByteArray FileProviderItemMetadata::tagData() const
{
    return _tagData;
}

QHash<QString, QByteArray> FileProviderItemMetadata::extendedAttributes() const
{
    return _extendedAttributes;
}

int FileProviderItemMetadata::capabilities() const
{
    return _capabilities;
}

int FileProviderItemMetadata::fileSystemFlags() const
{
    return _fileSystemFlags;
}

unsigned int FileProviderItemMetadata::childItemCount() const
{
    return _childItemCount;
}

unsigned int FileProviderItemMetadata::typeOsCode() const
{
    return _typeOsCode;
}

unsigned int FileProviderItemMetadata::creatorOsCode() const
{
    return _creatorOsCode;
}

unsigned long long FileProviderItemMetadata::documentSize() const
{
    return _documentSize;
}

bool FileProviderItemMetadata::mostRecentVersionDownloaded() const
{
    return _mostRecentVersionDownloaded;
}

bool FileProviderItemMetadata::uploading() const
{
    return _uploading;
}

bool FileProviderItemMetadata::uploaded() const
{
    return _uploaded;
}

bool FileProviderItemMetadata::downloading() const
{
    return _downloading;
}

bool FileProviderItemMetadata::downloaded() const
{
    return _downloaded;
}

bool FileProviderItemMetadata::shared() const
{
    return _shared;
}

bool FileProviderItemMetadata::sharedByCurrentUser() const
{
    return _sharedByCurrentUser;
}

QString FileProviderItemMetadata::userVisiblePath() const
{
    return _userVisiblePath;
}

QString FileProviderItemMetadata::fileTypeString() const
{
    return _fileTypeString;
}

bool operator==(const FileProviderItemMetadata &lhs, const FileProviderItemMetadata &rhs)
{
    return lhs.identifier() == rhs.identifier() &&
        lhs.contentVersion() == rhs.contentVersion() &&
        lhs.metadataVersion() == rhs.metadataVersion();
}

} // namespace Mac

} // namespace OCC
