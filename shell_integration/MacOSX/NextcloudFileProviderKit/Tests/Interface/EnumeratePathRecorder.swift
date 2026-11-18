// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

/// Thread-safe collector for the remote paths PROPFINDs are issued for. The mock's `enumerateCallHandler` may run off the test's thread, so the storage is guarded by a lock.
public final class EnumeratePathRecorder: @unchecked Sendable {
    private let lock = NSLock()
    private var storage: [String] = []

    public init() {}

    public func add(_ path: String) {
        lock.lock()
        defer { lock.unlock() }
        storage.append(path)
    }

    public var paths: [String] {
        lock.lock()
        defer { lock.unlock() }
        return storage
    }

    public var count: Int {
        paths.count
    }

    public func reset() {
        lock.lock()
        defer { lock.unlock() }
        storage.removeAll()
    }

    /// Attach the recorder to a mock so every read is recorded.
    public func record(on remoteInterface: MockRemoteInterface) {
        remoteInterface.enumerateCallHandler = { [self] remotePath, _, _, _, _, _, _, _ in
            add(remotePath)
        }
    }
}
