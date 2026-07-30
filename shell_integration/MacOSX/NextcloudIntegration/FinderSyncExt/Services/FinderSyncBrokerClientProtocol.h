/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FinderSyncBrokerClientProtocol_h
#define FinderSyncBrokerClientProtocol_h

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * @brief The APIs the FinderSync extension exposes to the broker login item.
 *
 * This is the push half of the broker contract; see FinderSyncBrokerProtocol.h for the
 * pull half and for why a broker is needed at all.
 *
 * Without a push the extension would have to poll. Its reconnect backoff runs up to
 * eight seconds, so a user who launches the app would wait that long for badges to
 * appear even though the endpoint became available immediately. The extension therefore
 * fetches once when it connects to the broker — covering the case where the app
 * published first — and relies on this callback for everything after that.
 */
@protocol FinderSyncBrokerClientProtocol

/**
 * @brief The published app endpoint changed.
 *
 * Sent to every connected extension when the app publishes an endpoint, and again with
 * a nil endpoint when the app's connection to the broker drops. Acting on the nil case
 * matters: an endpoint outlives the connection that carried it but not the listener that
 * created it, so continuing to hand out or reuse a dead endpoint would leave the
 * extension reconnecting forever without ever converging.
 *
 * @param endpoint The new endpoint, or nil when no app is currently reachable.
 * @param generation Monotonic generation of this endpoint, 0 when endpoint is nil. The
 * extension ignores a generation it has already acted on, which keeps a redundant push
 * racing an in-flight fetch from tearing down a healthy connection.
 */
- (void)appEndpointDidChange:(NSXPCListenerEndpoint *_Nullable)endpoint
                  generation:(uint64_t)generation;

@end

NS_ASSUME_NONNULL_END

#endif /* FinderSyncBrokerClientProtocol_h */
