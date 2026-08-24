//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

///
/// Keeps active `moreComing` buffers alive across fresh ``Enumerator`` instances, isolated by container.
///
final class ChangeDeliveryBufferStore: @unchecked Sendable {
    private let lock = NSLock()
    private let log: any FileProviderLogging
    private var buffers: [String: ChangeDeliveryBuffer] = [:]

    init(log: any FileProviderLogging) {
        self.log = log
    }

    /// Return the active buffer for `key`, or an unretained buffer for a new enumeration.
    func buffer(for key: String) -> ChangeDeliveryBuffer {
        lock.lock()
        if let buffer = buffers[key] {
            lock.unlock()
            return buffer
        }
        lock.unlock()

        return ChangeDeliveryBuffer(log: log) { [weak self] buffer, moreComing in
            self?.setActive(buffer, for: key, active: moreComing)
        }
    }

    private func setActive(_ buffer: ChangeDeliveryBuffer, for key: String, active: Bool) {
        lock.lock()
        defer { lock.unlock() }

        if active {
            buffers[key] = buffer
            return
        }

        guard buffers[key] === buffer else {
            return
        }
        buffers.removeValue(forKey: key)
    }
}
