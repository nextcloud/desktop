/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#import <FileProvider/FileProvider.h>

@interface TestFileProviderItem : NSObject <NSFileProviderItem>
@property (nonatomic, copy) NSFileProviderItemIdentifier itemIdentifier;
@property (nonatomic, copy) NSFileProviderItemIdentifier parentItemIdentifier;
@property (nonatomic, copy) NSString *filename;
@property (nonatomic, copy) UTType *contentType;
@property (nonatomic) NSFileProviderTypeAndCreator typeAndCreator;
@property (nonatomic) NSFileProviderItemCapabilities capabilities;
@property (nonatomic) NSFileProviderFileSystemFlags fileSystemFlags;
@property (nonatomic, copy) NSNumber *documentSize;
@property (nonatomic, copy) NSNumber *childItemCount;
@property (nonatomic, copy) NSDate *creationDate;
@property (nonatomic, copy) NSDate *contentModificationDate;
@property (nonatomic, copy) NSDate *lastUsedDate;
@property (nonatomic, copy) NSData *tagData;
@property (nonatomic, retain) NSDictionary<NSString *, NSData *> *extendedAttributes;
@property (nonatomic, getter=isUploaded) BOOL uploaded;
@property (nonatomic, getter=isUploading) BOOL uploading;
@property (nonatomic, copy) NSError *uploadingError;
@property (nonatomic, getter=isDownloaded) BOOL downloaded;
@property (nonatomic, getter=isDownloading) BOOL downloading;
@property (nonatomic, copy) NSError *downloadingError;
@property (nonatomic, getter=isMostRecentVersionDownloaded) BOOL mostRecentVersionDownloaded;
@property (nonatomic, getter=isShared) BOOL shared;
@property (nonatomic, getter=isSharedByCurrentUser) BOOL sharedByCurrentUser;
@property (nonatomic, retain) NSPersonNameComponents *ownerNameComponents;
@property (nonatomic, retain) NSPersonNameComponents *mostRecentEditorNameComponents;
@property (nonatomic, retain) NSFileProviderItemVersion *itemVersion;
@property (nonatomic, copy) NSString *symlinkTargetPath;
@end
