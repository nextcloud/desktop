<!--
SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Performance benchmarks

How to reproduce the scan, ingestion and enumeration measurements, and what each
one actually measures.

## Overview

Two kinds of measurement appear in this package's history, and they are not
interchangeable.

**Counts** are reproducible from a checkout. How many PROPFINDs one scan issues,
how many database rows one enumeration rewrites, and how many reads overlap are
all properties of the code, not of the machine or the account it runs against.
`PerformanceBenchmarkTests` asserts them exactly, so a regression fails the
suite rather than merely reading slower.

**Durations** are not. The figures quoted in commit messages and pull requests
were taken on one live account with roughly 17,000 items over a real network,
and nothing reproduces them but that account. What does carry over is the ratio
between two revisions measured the same way on the same machine, which is what
the timing benchmark below is for.

Prefer the counts. A duration that improves without its count improving usually
means the machine was quieter, not that the code got better.

## Running them

From the package root:

```bash
swift test --filter PerformanceBenchmarkTests
```

The timing benchmark prints its numbers with a `[benchmark]` prefix:

```
[benchmark] full working-set walk over 30 directories (6 files each, 50ms per read)
[benchmark]   best of 3: 0.312381 seconds
[benchmark]   sequential floor: 1.5 seconds
[benchmark]   peak read overlap: 6
```

To compare two revisions, run the same suite on both. The benchmarks depend on
one test-only addition to `MockRemoteInterface` — `enumerateLatency` and
`maxConcurrentEnumerations` — so on a revision that predates them, copy
`Tests/Interface/MockRemoteInterface.swift` and
`Tests/NextcloudFileProviderKitTests/PerformanceBenchmarkTests.swift` across
before running. Benchmarks that cover a feature the older revision does not have
will not compile there; drop those and keep the rest.

## What each benchmark measures

### Read volume

`testFullWalkReadsEachMaterialisedDirectoryOnce` asserts that a full working-set
walk issues exactly one read per materialised directory and none for the
materialised files inside them, whose state their parent's depth-1 read already
carries. It is the baseline the targeted figure below is a ratio against, and
the guard against the walk regaining per-file reads.

`testPushTargetedWalkReadsOnlyTheNamedContainers` asserts that a walk answering a
push reads only the containers the push named — one, against the thirty a full
walk reads — and still reports the change. This is the mechanism behind the
largest improvement measured on the live account: a push identifies a changed
file within a second or so, and the extension used to answer it by walking the
entire materialised set.

### Write volume

`testUnchangedPaginatedEnumerationWritesNoRows` and
`testChangedPaginatedEnumerationWritesOnlyTheChangedRow` page through a
200-file directory and count the rows whose persisted `syncTime` moved. A
skipped write leaves the seeded timestamp untouched, so the count is exact.

Pagination matters here: servers from Nextcloud 31 answer every enumeration with
a paginated listing, and that is the ingestion path that used to write every row
it read. The mock advertises server major version 28, so the benchmarks drive
``Enumerator/readServerUrl(_:pageSettings:account:remoteInterface:dbManager:domain:enumeratedItemIdentifier:depth:log:)``
with explicit page settings rather than going through
``Enumerator/enumerateItems(for:startingAt:)``, whose pagination is gated on that
version.

The second test is the control. "Writes no rows" is also satisfied by a guard
that suppresses genuine changes, which is the failure mode that matters — a
suppressed write is a change the user never sees.

### Read overlap

`testWalkReadsOverlap` gives each mocked read a fixed duration and records how
many were in flight at once. A sequential scan reads 1 no matter how large the
tree is.

`testFullWalkWallClock` times the same walk and asserts only that it beats the
sequential floor — the duration issuing every read one after another would take.
That is a ceiling a sequential scan cannot meet by construction, not a tuned
threshold, so it does not turn into a flaky test on a loaded machine. The
absolute numbers it prints are for comparing revisions, not for asserting.

## Measured on this package

Run on an Apple silicon Mac against `stable-34.0` (34.0.3) and against the
branch, same machine, same invocation.

| Benchmark | 34.0.3 | Branch |
| --- | --- | --- |
| Reads per full walk (30 materialised directories, 180 files) | 30 | 30 |
| Rows rewritten by an unchanged 200-file paginated enumeration | 201 | 0 |
| Rows rewritten when one of those 200 files changed | 201 | 1 |
| Peak overlapping reads during a full walk | 1 | 6 |
| Full walk wall clock, 30 reads × 50 ms | 1.80 s | 0.31 s |
| Reads to answer a push naming one container | not implemented | 1 |

The read-volume row is unchanged deliberately: the walk already read one
directory at a time, and the benchmark exists to keep it that way.

## Measuring on a live account

The counts above are mechanisms. Their effect on a real domain depends on how
much is materialised, how deep the tree is and how the server responds, so the
figures in the commit messages were taken from the extension's own logs.

The extension writes JSON Lines to its domain log directory, and every line
carries a category and level. To watch a running extension instead:

```bash
log stream --predicate 'subsystem CONTAINS "FileProviderExt"' --level debug
```

The specific things worth counting in a captured window:

- **Dataless transitions** — `Updating item state to dataless.` Each one is an
  item the extension told the system to drop. A steady stream of them while
  nothing is being evicted is the materialisation flip-flop described in
  ``MaterializedEnumerationObserver``.
- **Redundant writes** — `Skipping item metadata write; database already holds
  this remote state.` against the number of items enumerated. The ratio is the
  ingestion guard's hit rate.
- **Scan span** — the interval between `Checking materialised items for changes
  on the server...` and its matching completion line is one full walk.
- **Targeted scans** — `Scanning N push-targeted container(s) instead of M
  materialised item(s).` gives the ratio the push path achieved on that domain.
- **Discovery-to-report latency** — the interval between the log line naming a
  new item during a scan and the line reporting it to the change observer. This
  is what streaming delivery shortens; before it, a change found early in a walk
  waited for the walk to end.

Take the same capture before and after a change, on the same account, with the
same set of folders materialised. Comparing across accounts is not meaningful:
every figure here scales with the materialised set.
