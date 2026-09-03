//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation

//
// Benchmark harness for NextcloudFileProviderKit.
//
// Every scenario runs the package's production code against a real Nextcloud server over HTTP. The
// only stand-ins are the File Provider observers, which cannot be real outside an extension host.
// See `Benchmarks/README.md`.
//

struct Arguments {
    var scenarios: [Scenario] = Scenario.allCases
    var repetitions = 3
    var json = false
    var outputPath: String?

    static func parse(_ arguments: [String]) throws -> Arguments {
        var parsed = Arguments()
        var requested = [Scenario]()
        var index = 0

        while index < arguments.count {
            let argument = arguments[index]
            index += 1

            switch argument {
                case "--json":
                    parsed.json = true
                case "--output":
                    guard index < arguments.count else {
                        throw BenchmarkError.unknownScenario("--output needs a path")
                    }
                    parsed.outputPath = arguments[index]
                    parsed.json = true
                    index += 1
                case "--repetitions":
                    guard index < arguments.count, let value = Int(arguments[index]) else {
                        throw BenchmarkError.unknownScenario("--repetitions needs a number")
                    }
                    parsed.repetitions = value
                    index += 1
                case "--list":
                    for scenario in Scenario.allCases {
                        print("\(scenario.rawValue)\n    \(scenario.summary)")
                    }
                    exit(0)
                case "--help", "-h":
                    printUsage()
                    exit(0)
                default:
                    guard let scenario = Scenario(rawValue: argument) else {
                        throw BenchmarkError.unknownScenario(argument)
                    }
                    requested.append(scenario)
            }
        }

        if !requested.isEmpty {
            parsed.scenarios = requested
        }

        return parsed
    }
}

func printUsage() {
    print("""
    Usage: nfpk-bench [scenario...] [--repetitions N] [--json] [--output PATH] [--list]

    --output writes the JSON to a file. Prefer it over --json when the result is
    parsed: dependencies of this package print to standard output.

    Runs against the server named by NFPK_BENCH_URL (default http://localhost:8080),
    with NFPK_BENCH_USER / NFPK_BENCH_PASSWORD (default admin / benchmark).

    Bring the reference server up first:
      docker compose -f Benchmarks/docker-compose.yml up -d
      Benchmarks/provision.sh
    """)
}

func render(_ results: [BenchmarkResult]) {
    for result in results {
        print("")
        print("\(result.scenario)  [\(result.revision)]")
        let width = result.metrics.map(\.name.count).max() ?? 0
        for metric in result.metrics {
            let name = metric.name.padding(toLength: width, withPad: " ", startingAt: 0)
            let value = metric.unit == "s"
                ? String(format: "%.3f", metric.value)
                : String(format: "%g", metric.value)
            print("  \(name)  \(value) \(metric.unit)")
        }
    }
}

func currentRevision() -> String {
    let process = Process()
    process.executableURL = URL(fileURLWithPath: "/usr/bin/env")
    process.arguments = ["git", "rev-parse", "--short", "HEAD"]

    let pipe = Pipe()
    process.standardOutput = pipe
    process.standardError = FileHandle.nullDevice

    do {
        try process.run()
        process.waitUntilExit()
    } catch {
        return "unknown"
    }

    let data = pipe.fileHandleForReading.readDataToEndOfFile()
    let revision = String(data: data, encoding: .utf8)?
        .trimmingCharacters(in: .whitespacesAndNewlines)

    return revision?.isEmpty == false ? revision! : "unknown"
}

do {
    let arguments = try Arguments.parse(Array(CommandLine.arguments.dropFirst()))
    let revision = ProcessInfo.processInfo.environment["NFPK_BENCH_REVISION"] ?? currentRevision()

    var results = [BenchmarkResult]()

    for scenario in arguments.scenarios {
        // A fresh environment per scenario: a database carried over would decide how much work the
        // next scenario has to do, which is what several of them measure.
        let environment = try BenchmarkEnvironment.make()
        defer { environment.tearDown() }

        try await environment.verifyReachable()

        let runner = ScenarioRunner(
            environment: environment, revision: revision, repetitions: arguments.repetitions
        )
        try await results.append(runner.run(scenario))
    }

    if arguments.json {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        let data = try encoder.encode(results)

        if let outputPath = arguments.outputPath {
            try data.write(to: URL(fileURLWithPath: outputPath))
        } else {
            print(String(data: data, encoding: .utf8) ?? "[]")
        }
    } else {
        render(results)
    }
} catch {
    FileHandle.standardError.write(Data("\(error)\n".utf8))
    exit(1)
}
