//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import Testing

struct RemoteChangeTargetsTests {
    let folder = NSFileProviderItemIdentifier("folder")
    let other = NSFileProviderItemIdentifier("other")

    @Test func walksEverythingUntilAFullWalkHasCompleted() {
        let targets = RemoteChangeTargets()
        targets.record(targets: [folder])

        #expect(targets.takeScope() == .fullWalk)
    }

    @Test func readsTheRecordedContainersAfterAFullWalk() {
        let targets = RemoteChangeTargets()
        targets.noteFullWalkCompleted(startedAt: Date())
        targets.record(targets: [folder])
        targets.record(targets: [other])

        #expect(targets.takeScope() == .targets([folder, other]))
    }

    @Test func takingTheScopeClearsTheRecordedContainers() {
        let targets = RemoteChangeTargets()
        targets.noteFullWalkCompleted(startedAt: Date())
        targets.record(targets: [folder])
        _ = targets.takeScope()

        #expect(targets.takeScope() == .fullWalk)
    }

    @Test func aRequestedFullWalkWinsOverRecordedContainers() {
        let targets = RemoteChangeTargets()
        targets.noteFullWalkCompleted(startedAt: Date())
        targets.record(targets: [folder])
        targets.requestFullWalk()

        #expect(targets.takeScope() == .fullWalk)
    }

    @Test func takingTheScopeClearsTheRequest() {
        let targets = RemoteChangeTargets()
        targets.noteFullWalkCompleted(startedAt: Date())
        targets.requestFullWalk()
        _ = targets.takeScope()
        targets.record(targets: [folder])

        #expect(targets.takeScope() == .targets([folder]))
    }

    @Test func walksEverythingOnceTheIntervalHasElapsed() {
        let completed = Date()
        let targets = RemoteChangeTargets()
        targets.noteFullWalkCompleted(startedAt: completed)

        targets.record(targets: [folder])
        #expect(targets.takeScope(now: completed.addingTimeInterval(RemoteChangeTargets.fullWalkInterval - 1)) == .targets([folder]))

        targets.record(targets: [folder])
        #expect(targets.takeScope(now: completed.addingTimeInterval(RemoteChangeTargets.fullWalkInterval)) == .fullWalk)
    }

    @Test func walksEverythingWhenTheClockWentBack() {
        let completed = Date()
        let targets = RemoteChangeTargets()
        targets.noteFullWalkCompleted(startedAt: completed)
        targets.record(targets: [folder])

        #expect(targets.takeScope(now: completed.addingTimeInterval(-1)) == .fullWalk)
    }

    @Test func anEarlierWalkDoesNotMoveTheIntervalBack() {
        let later = Date()
        let targets = RemoteChangeTargets()
        targets.noteFullWalkCompleted(startedAt: later)
        targets.noteFullWalkCompleted(startedAt: later.addingTimeInterval(-RemoteChangeTargets.fullWalkInterval))
        targets.record(targets: [folder])

        #expect(targets.takeScope(now: later.addingTimeInterval(1)) == .targets([folder]))
    }
}
