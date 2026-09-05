<!--
SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: LGPL-3.0-or-later
-->

# NextcloudFileProviderKit benchmarks

End-to-end measurements of this package's enumeration and synchronisation paths, run against a
real Nextcloud server, comparable between two revisions on one command.

## What is real and what is not

Everything below the File Provider observer is production code:

| Layer | In the benchmark |
| --- | --- |
| Server | A real Nextcloud (the reference container below, or any instance you point it at) |
| Transport | Real HTTP, real `NextcloudKit`, real WebDAV and real XML parsing |
| Sync engine | The package's own `Enumerator`, ingestion and `FilesDatabaseManager` |
| Database | A real Realm database in a temporary directory, fresh per scenario |
| File Provider observers | **Stand-ins.** `NSFileProviderChangeObserver` and `NSFileProviderEnumerationObserver` are supplied by the framework inside a running extension and cannot be obtained outside one. The harness implements them to do what the framework does — drain `moreComing` batches until the enumerator reports the last one — and to timestamp what arrives. |

Request traffic is measured by `CountingRemoteInterface`, a passthrough that forwards every call to
the real interface and records how many requests were issued and how many overlapped. It replaces
nothing.

## Running

```bash
docker compose -f Benchmarks/docker-compose.yml up -d
Benchmarks/provision.sh
swift run -c release NextcloudFileProviderKitBenchmarks
```

`--list` names the scenarios, `--repetitions N` changes how many passes are taken (the median is
reported), and `--output PATH` writes JSON.

To measure against a different server:

```bash
export NFPK_BENCH_URL=https://cloud.example.com
export NFPK_BENCH_USER=benchmark
export NFPK_BENCH_PASSWORD=…
```

Any instance works, but only against a fixture of the same shape are two runs comparable. The
numbers below scale with the size of the materialised set.

## Comparing two revisions

This is the point of the harness. One command builds an unmodified baseline from a detached
worktree, copies this directory into it, builds both in release, runs both against the same server
and prints the difference:

```bash
git fetch https://github.com/nextcloud/desktop.git stable-34.0:refs/remotes/upstream/stable-34.0
Benchmarks/compare.sh upstream/stable-34.0
```

Only the harness crosses into the baseline worktree; the code being measured there is the baseline's
own. Both sides are built `-c release`, because a debug build measures the optimiser.

## The fixture

`provision.sh` writes the files into the container's data directory and indexes them with
`occ files:scan`, which is much faster than several thousand WebDAV uploads and leaves the same
server-side state.

```
nfpk-bench/
  tree/dir-000 … dir-199/file-000 … file-014.txt    200 directories x 15 files
  wide/file-0000 … file-0499.txt                    500 files in one listing
```

Override with `TREE_DIRS`, `FILES_PER_DIR` and `WIDE_FILES`. The tree is what a working-set walk
crosses; the wide directory is what a paginated enumeration pages through.

## Scenarios

| Scenario | What it measures |
| --- | --- |
| `working-set-scan` | One full working-set change enumeration over the materialised tree, nothing changed on the server. Reports walk duration, PROPFINDs issued, and their peak overlap. |
| `working-set-scan-change` | The same walk with one file genuinely changed on the server, adding how soon that change reaches the observer. |
| `item-enumeration-cold` | First enumeration of the wide directory into an empty database. |
| `item-enumeration-repeat` | A second enumeration of the same unchanged directory. `rows rewritten` counts rows whose `syncTime` moved, which only a real write does; on an unchanged listing the correct number is zero. |

Durations and counts are reported side by side deliberately. A duration alone cannot separate "the
code got smarter" from "the server was warmer" — the counts are what a claim can be held to, and
they do not move between machines.

## Reference numbers

Apple silicon Mac, Nextcloud 31.0.14 in the reference container, default fixture (3,204 materialised
items), `Benchmarks/compare.sh upstream/stable-34.0`.

### working-set-scan

| Metric | 34.0.3 | Branch | Change |
| --- | --- | --- | --- |
| walk duration | 7.921 s | 1.774 s | 4.47x lower |
| PROPFINDs issued | 203 | 203 | unchanged |
| peak concurrent PROPFINDs | 1 | 6 | 6x higher |
| changes reported | 12 | 12 | unchanged |

The request count is deliberately unchanged: the walk reads the same containers, and the time is
recovered from overlapping the waiting rather than from doing less.

### working-set-scan-change

| Metric | 34.0.3 | Branch | Change |
| --- | --- | --- | --- |
| walk duration | 7.892 s | 1.787 s | 4.42x lower |
| time to first change | 7.891 s | 0.161 s | 49x lower |
| first change at | 99.99 % of walk | 9.00 % of walk | — |
| changes reported | 17 | 17 | unchanged |

### item-enumeration-repeat

| Metric | 34.0.3 | Branch | Change |
| --- | --- | --- | --- |
| enumeration duration | 1.082 s | 0.709 s | 1.52x lower |
| rows rewritten | 501 | 0 | eliminated |
| items reported | 500 | 500 | unchanged |
| PROPFINDs issued | 1 | 1 | unchanged |

### item-enumeration-cold

Unchanged within noise, as expected: a first enumeration into an empty database has to persist
every row on both revisions.

## Limitations

- The File Provider observers are stand-ins, so nothing here measures the framework's own
  scheduling, its `update-item` retry behaviour, or how quickly Finder reflects a change. Those are
  observable only from a running extension, through the log lines described in the
  `PerformanceBenchmarks` article.
- The container runs SQLite and no Redis. That is deliberate — the benchmarks measure the client,
  and a heavier server stack adds variance — but it means absolute durations are not a prediction
  for a production server.
- Wall-clock figures are comparable only between two runs on one machine against one fixture. The
  counts are comparable everywhere.
- The reference image is pinned by digest so the server does not drift between runs. Only
  Nextcloud 31 has been exercised so far.
