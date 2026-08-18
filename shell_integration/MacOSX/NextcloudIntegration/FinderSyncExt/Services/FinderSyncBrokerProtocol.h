/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FinderSyncBrokerProtocol_h
#define FinderSyncBrokerProtocol_h

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * @brief The APIs exposed by the FinderSync broker login item, called by both the
 * main app and the FinderSync extension.
 *
 * The broker exists because neither the app nor the extension is allowed to publish
 * a named XPC endpoint. Only a launchd job may do that: the Mach service name has to
 * be declared in a launchd.plist, and `xpc_connection_create(3)` states that "new
 * service names may NOT be dynamically registered" and that the allowances XPC makes
 * for this "in debug scenarios" will "absolutely NOT be made in the production
 * scenario". That is why a plain `-[NSXPCListener initWithMachServiceName:]` in the
 * app works under Xcode and fails once the app is launched from Finder — the failure
 * mode that left FinderSync completely inert in 34.0.0.
 *
 * A Service Management login item *is* a launchd job, so it can vend a name that both
 * sandboxed peers are permitted to look up: the broker's Mach service name equals its
 * own bundle identifier, which is prefixed by the shared App Group identifier, and the
 * App Sandbox grants `mach-lookup` for any name under an App Group the process claims.
 * No `temporary-exception` entitlement is involved on any side.
 *
 * The broker is only a rendezvous point. It stores the endpoint of the app's anonymous
 * listener and hands it to the extension; all FinderSync traffic then flows directly
 * between app and extension over that endpoint, so no message is ever relayed here.
 *
 * @note `NSXPCListenerEndpoint` needs no `-setClasses:forSelector:argumentIndex:ofReply:`
 * allow-list. That API constrains the contents of *collection* arguments; a directly
 * typed parameter takes its class from the method signature, and endpoints conform to
 * `NSSecureCoding`. Passing an endpoint as `id`, or nested inside a dictionary, would
 * require an allow-list — so do not do that.
 */
@protocol FinderSyncBrokerProtocol

/**
 * @brief Publish the endpoint of the app's anonymous listener.
 *
 * Called by the main app on startup, and again whenever its own connection to the
 * broker is interrupted or invalidated (a new broker process has no stored endpoint).
 * The broker pushes the new endpoint to every connected extension and answers
 * subsequent -fetchAppEndpointWithReply: calls with it.
 *
 * @param endpoint Endpoint of an `NSXPCListener` created with +anonymousListener.
 * @param reply Invoked with the generation number assigned to this endpoint.
 */
- (void)publishAppEndpoint:(NSXPCListenerEndpoint *)endpoint
                     reply:(void(^)(uint64_t generation))reply;

/**
 * @brief Retrieve the currently published app endpoint.
 *
 * Called by the extension once per connection attempt. The extension is launched by
 * Finder at login and routinely runs long before the app has published anything, so a
 * nil endpoint is an ordinary transient state, not an error: wait for
 * -appEndpointDidChange:generation: or retry.
 *
 * @param reply Invoked with the stored endpoint, or nil when the app has not published
 * one (or its connection to the broker has since dropped), and the generation number.
 * The generation is 0 when there is no endpoint, which distinguishes "nothing published
 * yet" from a stale endpoint the caller has already seen.
 */
- (void)fetchAppEndpointWithReply:(void(^)(NSXPCListenerEndpoint *_Nullable endpoint,
                                           uint64_t generation))reply;

/**
 * @brief The broker's own bundle version.
 *
 * A `.pkg` that replaces the app bundle does not restart an already-running broker, so
 * it keeps executing the previous binary. The app compares this against its own version
 * and, on a mismatch, unregisters and re-registers the login item — awaiting the
 * unregister completion handler before re-registering, because registering too early
 * fails with SMAppServiceErrorDomain code 1.
 *
 * @param reply Invoked with the broker's CFBundleShortVersionString.
 */
- (void)brokerVersionWithReply:(void(^)(NSString *version))reply;

/**
 * @brief Liveness probe.
 *
 * `-[NSXPCListener resume]` returns void and reports activation failure only to the XPC
 * subsystem's own log, and `initWithMachServiceName:` never returns nil, so neither
 * proves anything about the channel. Only a completed round-trip does: a reply here
 * establishes that the Mach name really is in the bootstrap namespace and that the
 * broker's listener delegate is accepting connections.
 *
 * @param reply Invoked by the broker on receipt.
 */
- (void)pingWithReply:(void(^)(void))reply;

@end

NS_ASSUME_NONNULL_END

#endif /* FinderSyncBrokerProtocol_h */
