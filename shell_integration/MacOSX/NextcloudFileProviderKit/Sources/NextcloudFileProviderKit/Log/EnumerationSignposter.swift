//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
import os

///
/// Process-global `OSSignposter` for the enumeration hot path.
///
/// The enumeration + persistence work runs across `static func`s that receive no `self`, so a
/// shared signposter is the least invasive way to instrument them without threading a handle
/// through every signature. `OSSignposter` (and the `OSSignpostID`s it vends) are `Sendable`, so a
/// global `let` is safe under Swift 6 strict concurrency and usable from any isolation domain.
///
/// The subsystem mirrors ``FileProviderLogger`` (the extension bundle identifier) so signposts and
/// log messages share the same subsystem in Instruments and `log stream`. The `"PointsOfInterest"`
/// category makes intervals appear in the built-in *Points of Interest* instrument with no extra
/// configuration.
///
/// Signposts are near-zero cost when no Instruments trace (or `log stream --signpost`) is attached:
/// `signposter.signpostsEnabled` is `false` and the interval calls become cheap branches, and the
/// message interpolations are only evaluated when a consumer is present. They are therefore safe to
/// ship enabled — no `#if DEBUG` gate.
///
enum EnumerationSignposter {
    static let signposter = OSSignposter(
        subsystem: Bundle.main.bundleIdentifier ?? "",
        category: "PointsOfInterest"
    )
}

extension Duration {
    ///
    /// This duration expressed as fractional seconds, for human-readable performance logging.
    ///
    /// `components` yields whole seconds plus attoseconds; recombine them into a `Double`. Precision
    /// loss at the attosecond scale is irrelevant for wall-clock timings measured in milliseconds.
    ///
    var fpSeconds: Double {
        let parts = components
        return Double(parts.seconds) + Double(parts.attoseconds) / 1e18
    }
}
