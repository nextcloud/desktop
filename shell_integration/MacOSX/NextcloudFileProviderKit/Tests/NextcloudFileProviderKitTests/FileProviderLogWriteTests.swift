//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
@testable import NextcloudFileProviderKit
import XCTest

///
/// Guards for the two substitutions that made log writing cheap enough for a bulk pass: the byte
/// counter that replaced a per-line `stat`, and the removal of the per-line `fsync`.
///
final class FileProviderLogWriteTests: XCTestCase {
    private var logsDirectory: URL!

    override func setUpWithError() throws {
        try super.setUpWithError()
        logsDirectory = FileManager.default.temporaryDirectory
            .appendingPathComponent("FileProviderLogWriteTests-\(UUID().uuidString)", isDirectory: true)
    }

    override func tearDownWithError() throws {
        try? FileManager.default.removeItem(at: logsDirectory)
        try super.tearDownWithError()
    }

    private func makeLog(maxLogFileSize: Int64 = 100 * 1024 * 1024) -> FileProviderLog {
        FileProviderLog(
            fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("test"),
            logsDirectory: logsDirectory,
            maxLogFileSize: maxLogFileSize
        )
    }

    private func write(_ count: Int, to log: FileProviderLog) async {
        for index in 0 ..< count {
            await log.write(
                category: "Test",
                level: .info,
                message: "line \(index)",
                details: [:],
                file: #file,
                function: #function,
                line: #line
            )
        }
    }

    /// The counter stands in for a `stat` of the log file, so rotation is only correct while it
    /// agrees with the file byte for byte.
    func testTheByteCounterAgreesWithTheFileOnDisk() async throws {
        let log = makeLog()
        await write(64, to: log)

        let currentFile = await log.file
        let path = try XCTUnwrap(currentFile)
        let onDisk = try XCTUnwrap(
            try FileManager.default.attributesOfItem(atPath: path.path)[.size] as? Int64
        )
        let counted = await log.bytesWrittenToCurrentFile

        XCTAssertGreaterThan(onDisk, 0, "The writes should have reached the file.")
        XCTAssertEqual(
            counted, onDisk,
            "The rotation counter must match the file exactly; it is what replaced stat-ing it."
        )
    }

    /// Dropping the per-line `synchronize()` is only safe because `FileHandle.write(contentsOf:)`
    /// is an unbuffered `write(2)`, so the line is in the file when the call returns.
    func testALineIsReadableAsSoonAsTheWriteReturns() async throws {
        let log = makeLog()

        await log.write(
            category: "Test",
            level: .info,
            message: "needle",
            details: [:],
            file: #file,
            function: #function,
            line: #line
        )

        let currentFile = await log.file
        let path = try XCTUnwrap(currentFile)
        let contents = try String(contentsOf: path, encoding: .utf8)

        XCTAssertTrue(
            contents.contains("needle"),
            "An unsynchronised write must still be readable immediately after it returns."
        )
    }

    /// Rotation has to reset the counter along with the file, and the counter dropping is the
    /// evidence, because log files are named to the second and rotations inside one second share a
    /// path.
    func testRotationResetsTheCounterRatherThanCarryingItOver() async throws {
        let limit: Int64 = 2 * 1024
        let log = makeLog(maxLogFileSize: limit)

        var previous: Int64 = 0
        var sawReset = false
        var peak: Int64 = 0

        for index in 0 ..< 512 {
            await log.write(
                category: "Test",
                level: .info,
                message: "line \(index)",
                details: [:],
                file: #file,
                function: #function,
                line: #line
            )

            let current = await log.bytesWrittenToCurrentFile

            if current < previous {
                sawReset = true
                peak = previous
                break
            }

            previous = current
        }

        XCTAssertTrue(sawReset, "Writing well past the size limit should have rotated at least once.")
        XCTAssertGreaterThanOrEqual(peak, limit, "Rotation should only happen once the limit is reached.")

        let afterRotation = await log.bytesWrittenToCurrentFile
        let currentFile = await log.file
        let path = try XCTUnwrap(currentFile)
        let onDisk = try XCTUnwrap(
            try FileManager.default.attributesOfItem(atPath: path.path)[.size] as? Int64
        )

        XCTAssertEqual(afterRotation, onDisk, "The counter must match the new file exactly.")
        XCTAssertLessThan(afterRotation, limit, "The new file starts below the limit, not above it.")
    }
}
