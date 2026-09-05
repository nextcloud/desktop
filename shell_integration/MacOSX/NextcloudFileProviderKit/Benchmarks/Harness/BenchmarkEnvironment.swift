//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import NextcloudFileProviderKit
import NextcloudKit
import os

///
/// Silent logger. The benchmarks measure the code, and the extension's own file logging is itself
/// one of the things under measurement elsewhere, so it must not add noise here.
///
actor BenchmarkLog: FileProviderLogging {
    let debugLoggingEnabled = false
    let performanceLoggingEnabled = false

    func write(
        category _: String,
        level _: OSLogType,
        message _: String,
        details _: [FileProviderLogDetailKey: (any Sendable)?],
        file _: StaticString,
        function _: StaticString,
        line _: UInt
    ) {}
}

///
/// Connection details and a fresh database for one benchmark run.
///
/// The server is a real Nextcloud. `Benchmarks/docker-compose.yml` brings up the reference one the
/// published numbers were taken against; `NFPK_BENCH_URL`, `NFPK_BENCH_USER` and
/// `NFPK_BENCH_PASSWORD` point the same harness at any other instance.
///
struct BenchmarkEnvironment {
    let account: Account
    let remoteInterface: CountingRemoteInterface
    let dbManager: FilesDatabaseManager
    let databaseDirectory: URL

    static func make() throws -> BenchmarkEnvironment {
        let environment = ProcessInfo.processInfo.environment
        let serverUrl = environment["NFPK_BENCH_URL"] ?? "http://localhost:8080"
        let user = environment["NFPK_BENCH_USER"] ?? "admin"
        let password = environment["NFPK_BENCH_PASSWORD"] ?? "benchmark"

        let account = Account(user: user, id: user, serverUrl: serverUrl, password: password)

        // NextcloudKit narrates every request at its default level, which would dominate the
        // harness output and add its own per-request cost to the numbers.
        NextcloudKit.configureLogger(logLevel: .disabled)

        let kit = NextcloudKit.shared
        kit.appendSession(
            account: account.ncKitAccount,
            urlBase: account.serverUrl,
            user: account.username,
            userId: account.id,
            password: account.password,
            userAgent: "NextcloudFileProviderKitBenchmarks",
            groupIdentifier: ""
        )

        // A fresh database per run. Carrying one over would let an earlier run's rows decide how
        // much work the next one has to do, which is exactly what several scenarios measure.
        let databaseDirectory = FileManager.default.temporaryDirectory
            .appendingPathComponent("nfpk-bench-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(
            at: databaseDirectory, withIntermediateDirectories: true
        )

        let dbManager = FilesDatabaseManager(
            account: account,
            databaseDirectory: databaseDirectory,
            fileProviderDomainIdentifier: NSFileProviderDomainIdentifier("benchmark"),
            log: BenchmarkLog()
        )

        return BenchmarkEnvironment(
            account: account,
            remoteInterface: CountingRemoteInterface(wrapping: kit),
            dbManager: dbManager,
            databaseDirectory: databaseDirectory
        )
    }

    func tearDown() {
        try? FileManager.default.removeItem(at: databaseDirectory)
    }

    /// Fail early and legibly rather than reporting a scenario's numbers for a server that is not
    /// reachable or not accepting the credentials.
    func verifyReachable() async throws {
        let (_, _, _, error) = await remoteInterface.enumerate(
            remotePath: account.davFilesUrl,
            depth: .target,
            showHiddenFiles: true,
            includeHiddenFiles: [],
            requestBody: nil,
            account: account,
            options: .init(),
            taskHandler: { _ in }
        )

        guard error == .success else {
            throw BenchmarkError.serverUnreachable(
                url: account.serverUrl, detail: error.errorDescription
            )
        }

        remoteInterface.reset()
    }
}

enum BenchmarkError: Error, CustomStringConvertible {
    case serverUnreachable(url: String, detail: String)
    case fixtureMissing(path: String)
    case unknownScenario(String)

    var description: String {
        switch self {
            case let .serverUnreachable(url, detail):
                """
                Cannot reach \(url): \(detail)
                Start the reference server with:
                  docker compose -f Benchmarks/docker-compose.yml up -d
                  Benchmarks/provision.sh
                """
            case let .fixtureMissing(path):
                """
                The benchmark fixture is missing at \(path).
                Provision it with: Benchmarks/provision.sh
                """
            case let .unknownScenario(name):
                "Unknown scenario '\(name)'."
        }
    }
}
