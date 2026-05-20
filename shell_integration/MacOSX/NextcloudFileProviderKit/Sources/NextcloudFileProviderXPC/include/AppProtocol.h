/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef AppProtocol_h
#define AppProtocol_h

#import <Foundation/Foundation.h>
NS_ASSUME_NONNULL_BEGIN

/**
 * @brief The main app APIs exposed through XPC.
 */
@protocol AppProtocol

/**
 * @brief The file provider extension can tell the main app to offer the user server-features for the given item.
 * @param fileId The ocId as provided by the server for item identification independent from path.
 * @param path The local and absolute path for the item to offer actions for.
 * @param remoteItemPath The server-side path of the item, used as a fallback when no sync folder is configured.
 * @param domainIdentifier The file provider domain identifier for the account that manages this file.
 */
- (void)presentFileActions:(NSString *)fileId path:(NSString *)path remoteItemPath:(NSString *)remoteItemPath withDomainIdentifier:(NSString *)domainIdentifier;

/**
 * @brief The file provider extension asks the main app to open the item's page in the user's web browser.
 *
 * The main app resolves the per-item private link via PROPFIND (with the
 * server's deprecated link as a fallback) and hands it to the platform browser.
 * This mirrors the classic-sync "Open in browser" entry exposed via
 * `SocketApi::command_OPEN_PRIVATE_LINK`. See nextcloud/desktop#10025.
 *
 * @param fileId The **numeric** server file id, equal to the WebDAV `fileid` property.
 *               This is NOT the ocId — naming differs from `presentFileActions:...`
 *               on purpose because `fetchPrivateLinkUrl` requires the numeric id
 *               for the deprecated fallback URL.
 * @param remoteItemPath The server-side path of the item, used to issue the
 *                       PROPFIND that resolves the per-item private link.
 * @param domainIdentifier The file provider domain identifier for the account that owns the item.
 */
- (void)openItemInBrowser:(NSString *)fileId
           remoteItemPath:(NSString *)remoteItemPath
      forDomainIdentifier:(NSString *)domainIdentifier
    NS_SWIFT_NAME(openItemInBrowser(_:remoteItemPath:forDomainIdentifier:));

/**
 * @brief The file provider extension can report its synchronization status as a string constant value to the main app through this method.
 * @param status The synchronization status string.
 * @param domainIdentifier The file provider domain identifier for which the status is reported.
 */
- (void)reportSyncStatus:(NSString *)status forDomainIdentifier:(NSString *)domainIdentifier;

/**
 * @brief The file provider extension reports an item it refused to sync because that kind of item isn't supported yet (currently: macOS bundles).
 *
 * The main app surfaces the message in its activity view in the systray's tray window — the same place the classic sync engine reports excluded items.
 *
 * @param relativePath The path of the item relative to the file provider domain root.
 * @param fileName The display name of the item.
 * @param reason A localized, human-readable explanation of why the item was excluded. Already translated extension-side.
 * @param domainIdentifier The file provider domain identifier for the account that owns the item.
 */
- (void)reportItemExcludedFromSync:(NSString *)relativePath
                          fileName:(NSString *)fileName
                            reason:(NSString *)reason
               forDomainIdentifier:(NSString *)domainIdentifier;

/**
 * @brief The file provider extension reports a single item that the server refused with HTTP 507 (or that the pre-flight quota check refused).
 *
 * The main app surfaces an entry per item in its activity view, identical in shape to the per-item error entries the classic sync engine produces (e.g. "“X” was not synchronized — Insufficient storage on the server."). See nextcloud/desktop#9598.
 *
 * @param relativePath The item's path relative to the file provider domain root.
 * @param fileName The display name of the item.
 * @param fileBytes Size of the local file the user tried to upload, in bytes. May be `nil` if not known at report time.
 * @param availableBytes Available quota the server reported via PROPFIND, in bytes. May be `nil` (e.g. when the refusal came from the server's 507 response rather than a pre-flight PROPFIND).
 * @param domainIdentifier The file provider domain identifier for the affected account.
 */
- (void)reportInsufficientQuotaForItem:(NSString *)relativePath
                              fileName:(NSString *)fileName
                             fileBytes:(NSNumber * _Nullable)fileBytes
                        availableBytes:(NSNumber * _Nullable)availableBytes
                   forDomainIdentifier:(NSString *)domainIdentifier;

/**
 * @brief The file provider extension reports that one or more uploads were refused by the server's quota for the given domain.
 *
 * The main app surfaces a single per-folder summary entry in its activity view with a "Retry all uploads" button — analogous to the entry the classic sync engine produces via `SyncEngine::slotInsufficientRemoteStorage`. The main app dedupes this report per domain.
 *
 * @param domainIdentifier The file provider domain identifier for the affected account.
 */
- (void)reportInsufficientQuotaSummaryForDomainIdentifier:(NSString *)domainIdentifier;

@end

NS_ASSUME_NONNULL_END
#endif /* AppProtocol_h */

