/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QString>

namespace OCC::Mac {

/**
 * @brief Identifiers shared by the desktop client, the FinderSync extension and the
 * FinderSync broker login item.
 *
 * These three bundles have to agree on two strings exactly, and they derive them by three
 * different routes: the client from the CMake-generated config.h, the extension from the
 * NCApplicationGroupIdentifier key in its Info.plist, and the broker from its own bundle
 * identifier. Any disagreement is invisible at build time and manifests only as an XPC
 * lookup that never resolves — the same silent failure class that made 34.0.0's broken
 * FinderSync integration so hard to spot.
 *
 * Keeping the client's derivation here, free of Objective-C and of any bundle lookup, means
 * it can be asserted against the other two routes by a unit test rather than by a human.
 */
namespace FinderSyncBrokerIdentity {

/**
 * @brief The App Group shared by client, extension and broker.
 *
 * Team-ID-prefixed, which on macOS is what makes the group unrestricted (no provisioning
 * profile needed) and what grants unprompted access to the group container.
 */
[[nodiscard]] QString appGroupIdentifier();

/**
 * @brief Bundle identifier of the broker login item, which is also the Mach service name
 * it vends.
 *
 * Those are necessarily the same string: a Service Management login item may only advertise
 * a Mach service named after its own bundle identifier. Because the identifier is the App
 * Group followed by a component, the name also falls inside the App Group prefix, which is
 * what lets the sandboxed client and the sandboxed extension look it up with no
 * temporary-exception entitlement.
 */
[[nodiscard]] QString brokerServiceName();

/**
 * @brief Code signing requirement used to authenticate the other side of a FinderSync XPC
 * connection.
 *
 * The client needs this as much as the broker does. It hands the broker an endpoint that
 * grants full access to the FinderSync protocol — including executeMenuCommand — so it has to
 * know it is talking to our own login item and not to something squatting the Mach name.
 *
 * Pass the result to -[NSXPCConnection setCodeSigningRequirement:] or
 * -[NSXPCListener setConnectionCodeSigningRequirement:] (both macOS 13+) and let the system
 * evaluate it against the message's audit token. Do not evaluate it by hand from a process
 * identifier: PIDs are recycled, so a peer can exit between the check and the use and be
 * replaced by a different, legitimately signed binary at the same PID.
 *
 * The two certificate clauses are combined with `or` so one string covers both Developer ID
 * and Mac App Store distribution. Debug builds additionally accept Apple Development
 * signatures, which are deliberately *not* compatible with the production requirement.
 *
 * @note Must produce the same text as PeerRequirement.swift in the broker target.
 */
[[nodiscard]] QString peerRequirement();

} // namespace FinderSyncBrokerIdentity

} // namespace OCC::Mac
