//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import NextcloudFileProviderKitMocks
import Testing
import UniformTypeIdentifiers

///
/// Coverage for the process-global `blockSync` user default, which stops the extension from talking to the server.
///
/// The gates run before the account is looked at, which is what makes these tests possible without one: an extension with no account still reaches the gate, so the two cases are told apart by which error comes back. That ordering is deliberate and is asserted here, because moving a gate below the account guard would silently turn a blocked request into an authentication failure.
///
/// The suite is serialized and clears the key after every test because the gates read the user defaults of whichever process they run in, which under test is this one. Tests running in parallel would otherwise block each other's extensions.
///
@Suite("Blocking synchronization", .serialized)
struct BlockSyncTests {
    ///
    /// The key as it is written by an administrator and read by the extension.
    ///
    static let key = "blockSync"

    init() {
        UserDefaults.standard.removeObject(forKey: Self.key)
    }

    ///
    /// An extension for a domain which exists only for the duration of one test.
    ///
    /// - Returns: The extension.
    ///
    private func makeExtension() -> FileProviderExtension {
        let domain = NSFileProviderDomain(identifier: NSFileProviderDomainIdentifier(UUID().uuidString), displayName: "Test")

        return FileProviderExtension(domain: domain)
    }

    ///
    /// The least an item can be and still be handed to the framework methods under test.
    ///
    /// These requests are refused before anything reads more than the identifier and the name, so nothing more has to be true of it.
    ///
    private final class StubItem: NSObject, NSFileProviderItem {
        var itemIdentifier: NSFileProviderItemIdentifier {
            .rootContainer
        }

        var parentItemIdentifier: NSFileProviderItemIdentifier {
            .rootContainer
        }

        var filename: String {
            "stub.txt"
        }

        var contentType: UTType {
            .plainText
        }
    }

    @Test("An absent key does not block synchronization.")
    func absentKeyDoesNotBlock() {
        #expect(makeExtension().blockSync == false)
    }

    @Test("The key blocks synchronization when it is set.")
    func setKeyBlocks() {
        UserDefaults.standard.set(true, forKey: Self.key)

        #expect(makeExtension().blockSync)
    }

    @Test("An explicit negative reads the same as an absent key.")
    func explicitFalseDoesNotBlock() {
        UserDefaults.standard.set(false, forKey: Self.key)

        #expect(makeExtension().blockSync == false)
    }

    ///
    /// A value which cannot be read as a boolean must never be a reason to stop synchronizing.
    ///
    @Test("A value which is not a boolean does not block synchronization.")
    func malformedValueDoesNotBlock() {
        UserDefaults.standard.set("yes please", forKey: Self.key)

        #expect(makeExtension().blockSync == false)
    }

    ///
    /// This pins the ordering of the gate against the account guard. Without the key set, the same call must fall through to the account check and fail differently.
    ///
    @Test("Fetching contents is refused as unreachable rather than unauthenticated.")
    func fetchContentsIsRefused() async {
        UserDefaults.standard.set(true, forKey: Self.key)

        let error = await withCheckedContinuation { continuation in
            _ = makeExtension().fetchContents(for: .rootContainer, version: nil, request: NSFileProviderRequest()) { _, _, error in
                continuation.resume(returning: error)
            }
        }

        #expect((error as? NSFileProviderError)?.code == .serverUnreachable)
    }

    @Test("Fetching contents without the key set reaches the account guard.")
    func fetchContentsWithoutBlockingReachesAccountGuard() async {
        let error = await withCheckedContinuation { continuation in
            _ = makeExtension().fetchContents(for: .rootContainer, version: nil, request: NSFileProviderRequest()) { _, _, error in
                continuation.resume(returning: error)
            }
        }

        #expect((error as? NSFileProviderError)?.code == .notAuthenticated)
    }

    @Test("Deleting an item is refused as unreachable.")
    func deleteItemIsRefused() async {
        UserDefaults.standard.set(true, forKey: Self.key)

        let error = await withCheckedContinuation { continuation in
            _ = makeExtension().deleteItem(identifier: .rootContainer, baseVersion: NSFileProviderItemVersion(), request: NSFileProviderRequest()) { error in
                continuation.resume(returning: error)
            }
        }

        #expect((error as? NSFileProviderError)?.code == .serverUnreachable)
    }

    ///
    /// Refusing here is what stops remote changes arriving, because the signal the app sends reaches the extension as a request for an enumerator.
    ///
    @Test("Providing an enumerator is refused as unreachable.")
    func enumeratorIsRefused() {
        UserDefaults.standard.set(true, forKey: Self.key)

        #expect(throws: NSFileProviderError(.serverUnreachable)) {
            _ = try makeExtension().enumerator(for: .workingSet, request: NSFileProviderRequest())
        }
    }

    @Test("Providing an enumerator without the key set reaches the account guard.")
    func enumeratorWithoutBlockingReachesAccountGuard() {
        #expect(throws: NSFileProviderError(.notAuthenticated)) {
            _ = try makeExtension().enumerator(for: .workingSet, request: NSFileProviderRequest())
        }
    }

    ///
    /// A refused request is not a synchronization action, so nothing about it may reach the app. Were the gates placed after the bookkeeping instead of before it, a blocked client would report a stream of failures rather than staying quiet.
    ///
    @Test("A refused request is not reported to the app as synchronization activity.")
    func refusedRequestIsNotReported() async {
        UserDefaults.standard.set(true, forKey: Self.key)

        let fileProviderExtension = makeExtension()
        let proxy = SyncStatusCapturingAppProxy()
        fileProviderExtension.app = proxy

        _ = await withCheckedContinuation { continuation in
            _ = fileProviderExtension.deleteItem(identifier: .rootContainer, baseVersion: NSFileProviderItemVersion(), request: NSFileProviderRequest()) { error in
                continuation.resume(returning: error)
            }
        }

        #expect(fileProviderExtension.syncActions.isEmpty)
        #expect(fileProviderExtension.errorActions.isEmpty)
        #expect(proxy.reportedSyncStatuses.isEmpty)
    }

    ///
    /// Looking up an item is answered from the database, so blocking must not stop the Finder showing the name and size of something already known. Classic synchronization does not hide what it has while it is paused either.
    ///
    @Test("Looking up an item is not refused.")
    func itemLookupIsNotRefused() async {
        UserDefaults.standard.set(true, forKey: Self.key)

        let error = await withCheckedContinuation { continuation in
            _ = makeExtension().item(for: .rootContainer, request: NSFileProviderRequest()) { _, error in
                continuation.resume(returning: error)
            }
        }

        #expect((error as? NSFileProviderError)?.code != .serverUnreachable)
    }

    ///
    /// The value belongs to the process rather than to a domain, so two domains must see the same thing and neither may carry it in its own dictionary.
    ///
    @Test("The value is shared by every domain rather than stored per domain.")
    func valueIsProcessGlobal() {
        var defaults = FileProviderDomainDefaults(identifier: NSFileProviderDomainIdentifier("one"), log: FileProviderLogMock())
        defaults.blockSync = true

        let other = FileProviderDomainDefaults(identifier: NSFileProviderDomainIdentifier("two"), log: FileProviderLogMock())
        #expect(other.blockSync == true)

        #expect(UserDefaults.standard.dictionary(forKey: "one")?[Self.key] == nil)
        #expect(UserDefaults.standard.dictionary(forKey: "two")?[Self.key] == nil)
    }

    @Test("Creating an item is refused as unreachable.")
    func createItemIsRefused() async {
        UserDefaults.standard.set(true, forKey: Self.key)

        let error = await withCheckedContinuation { continuation in
            _ = makeExtension().createItem(basedOn: StubItem(), fields: [], contents: nil, request: NSFileProviderRequest()) { _, _, _, error in
                continuation.resume(returning: error)
            }
        }

        #expect((error as? NSFileProviderError)?.code == .serverUnreachable)
    }

    @Test("Modifying an item is refused as unreachable.")
    func modifyItemIsRefused() async {
        UserDefaults.standard.set(true, forKey: Self.key)

        let error = await withCheckedContinuation { continuation in
            _ = makeExtension().modifyItem(StubItem(), baseVersion: NSFileProviderItemVersion(), changedFields: [], contents: nil, request: NSFileProviderRequest()) { _, _, _, error in
                continuation.resume(returning: error)
            }
        }

        #expect((error as? NSFileProviderError)?.code == .serverUnreachable)
    }

    @Test("Removing the value returns the extension to synchronizing.")
    func removingTheValueUnblocks() {
        var defaults = FileProviderDomainDefaults(identifier: NSFileProviderDomainIdentifier("one"), log: FileProviderLogMock())
        defaults.blockSync = true
        defaults.blockSync = nil

        #expect(defaults.blockSync == nil)
        #expect(makeExtension().blockSync == false)
    }
}
