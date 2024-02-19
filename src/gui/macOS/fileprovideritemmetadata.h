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

#pragma once

#include <QObject>
#include <QDateTime>

namespace OCC {

namespace Mac {

class FileProviderItemMetadata
{
    Q_GADGET

	Q_PROPERTY(QString identifier READ identifier CONSTANT)
	Q_PROPERTY(QString parentItemIdentifier READ parentItemIdentifier CONSTANT)
    Q_PROPERTY(QString domainIdentifier READ domainIdentifier CONSTANT)
	Q_PROPERTY(QString filename READ filename CONSTANT)
	Q_PROPERTY(QString typeIdentifier READ typeIdentifier CONSTANT)
	Q_PROPERTY(QString symlinkTargetPath READ symlinkTargetPath CONSTANT)
	Q_PROPERTY(QString uploadingError READ uploadingError CONSTANT)
	Q_PROPERTY(QString downloadingError READ downloadingError CONSTANT)
	Q_PROPERTY(QString mostRecentEditorName READ mostRecentEditorName CONSTANT)
	Q_PROPERTY(QString ownerName READ ownerName CONSTANT)
	Q_PROPERTY(QDateTime contentModificationDate READ contentModificationDate CONSTANT)
	Q_PROPERTY(QDateTime creationDate READ creationDate CONSTANT)
	Q_PROPERTY(QDateTime lastUsedDate READ lastUsedDate CONSTANT)
	Q_PROPERTY(QByteArray contentVersion READ contentVersion CONSTANT)
	Q_PROPERTY(QByteArray metadataVersion READ metadataVersion CONSTANT)
	Q_PROPERTY(QByteArray tagData READ tagData CONSTANT)
	Q_PROPERTY(QHash<QString, QByteArray> extendedAttributes READ extendedAttributes CONSTANT)
	Q_PROPERTY(int capabilities READ capabilities CONSTANT)
	Q_PROPERTY(int fileSystemFlags READ fileSystemFlags CONSTANT)
	Q_PROPERTY(unsigned int childItemCount READ childItemCount CONSTANT)
	Q_PROPERTY(unsigned int typeOsCode READ typeOsCode CONSTANT)
	Q_PROPERTY(unsigned int creatorOsCode READ creatorOsCode CONSTANT)
	Q_PROPERTY(unsigned long long documentSize READ documentSize CONSTANT)
	Q_PROPERTY(bool mostRecentVersionDownloaded READ mostRecentVersionDownloaded CONSTANT)
	Q_PROPERTY(bool uploading READ uploading CONSTANT)
	Q_PROPERTY(bool uploaded READ uploaded CONSTANT)
	Q_PROPERTY(bool downloading READ downloading CONSTANT)
	Q_PROPERTY(bool downloaded READ downloaded CONSTANT)
	Q_PROPERTY(bool shared READ shared CONSTANT)
	Q_PROPERTY(bool sharedByCurrentUser READ sharedByCurrentUser CONSTANT)

    Q_PROPERTY(QString userVisiblePath READ userVisiblePath CONSTANT)

public:
    static FileProviderItemMetadata fromNSFileProviderItem(const void *const nsFileProviderItem, const QString &domainIdentifier);

    [[nodiscard]] QString identifier() const;
    [[nodiscard]] QString parentItemIdentifier() const;
    [[nodiscard]] QString domainIdentifier() const;
    [[nodiscard]] QString filename() const;
    [[nodiscard]] QString typeIdentifier() const;
    [[nodiscard]] QString symlinkTargetPath() const;
    [[nodiscard]] QString uploadingError() const;
    [[nodiscard]] QString downloadingError() const;
    [[nodiscard]] QString mostRecentEditorName() const;
    [[nodiscard]] QString ownerName() const;
    [[nodiscard]] QDateTime contentModificationDate() const;
    [[nodiscard]] QDateTime creationDate() const;
    [[nodiscard]] QDateTime lastUsedDate() const;
    [[nodiscard]] QByteArray contentVersion() const;
    [[nodiscard]] QByteArray metadataVersion() const;
    [[nodiscard]] QByteArray tagData() const;
    [[nodiscard]] QHash<QString, QByteArray> extendedAttributes() const;
    [[nodiscard]] int capabilities() const;
    [[nodiscard]] int fileSystemFlags() const;
    [[nodiscard]] unsigned int childItemCount() const;
    [[nodiscard]] unsigned int typeOsCode() const;
    [[nodiscard]] unsigned int creatorOsCode() const;
    [[nodiscard]] unsigned long long documentSize() const;
    [[nodiscard]] bool mostRecentVersionDownloaded() const;
    [[nodiscard]] bool uploading() const;
    [[nodiscard]] bool uploaded() const;
    [[nodiscard]] bool downloading() const;
    [[nodiscard]] bool downloaded() const;
    [[nodiscard]] bool shared() const;
    [[nodiscard]] bool sharedByCurrentUser() const;

    [[nodiscard]] QString userVisiblePath() const;
    [[nodiscard]] QString fileTypeString() const;

    // Check equality via identifier, contentVersion, and metadataVersion
    friend bool operator==(const FileProviderItemMetadata &lhs, const FileProviderItemMetadata &rhs);

private:
    [[nodiscard]] QString getUserVisiblePath() const;

	QString _identifier;
	QString _parentItemIdentifier;
    QString _domainIdentifier;
    QString _filename;
    QString _typeIdentifier;
    QString _symlinkTargetPath;
    QString _uploadingError;
    QString _downloadingError;
    QString _mostRecentEditorName;
    QString _ownerName;
    QDateTime _contentModificationDate;
    QDateTime _creationDate;
    QDateTime _lastUsedDate;
    QByteArray _contentVersion;
    QByteArray _metadataVersion;
    QByteArray _tagData;
    QHash<QString, QByteArray> _extendedAttributes;
    quint64 _favoriteRank = 0;
    int _capabilities = 0;
    int _fileSystemFlags = 0;
	unsigned int _childItemCount = 0;
    unsigned int _typeOsCode = 0;
    unsigned int _creatorOsCode = 0;
    unsigned long long _documentSize = 0;
    bool _mostRecentVersionDownloaded = false;
    bool _uploading = false;
    bool _uploaded = false;
    bool _downloading = false;
    bool _downloaded = false;
    bool _shared = false;
    bool _sharedByCurrentUser = false;
    bool _trashed = false;

    QString _userVisiblePath;
    QString _fileTypeString;
};

}

}
