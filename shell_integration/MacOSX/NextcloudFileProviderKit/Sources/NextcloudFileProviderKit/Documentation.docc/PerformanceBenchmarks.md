<!--
SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Performance benchmarks

Measuring the enumeration and synchronisation paths against a real server, and comparing two
revisions.

## Overview

Performance claims about this package are easy to make and hard to check. A figure taken from one
person's account on one network reproduces nowhere, and a figure taken from a mocked server measures
the mock. The benchmark harness in `Benchmarks/` exists to make such claims checkable: it runs the
package's own code against a real Nextcloud over real HTTP, and it can run the same measurement on
an unmodified baseline revision and print the difference.

`Benchmarks/README.md` is the operating manual. This article covers what the harness cannot see, and
how to measure that instead.

## Running it

```bash
docker compose -f Benchmarks/docker-compose.yml up -d
Benchmarks/provision.sh
Benchmarks/compare.sh upstream/stable-34.0
```

The comparison builds a detached worktree at the baseline ref, copies only the harness into it,
builds both revisions in release, runs both against the same server, and prints a table per
scenario.

## What it measures

Each scenario reports durations and counts together. A duration on its own cannot separate code that
got smarter from a server that was warmer; the counts — requests issued, requests overlapping, rows
written — are the part a claim can be held to, and they do not move between machines.

- `working-set-scan` and `working-set-scan-change` drive
  ``Enumerator/enumerateChanges(for:from:)`` on the working set, which is the entire path a remote
  change signal takes inside the extension.
- `item-enumeration-cold` and `item-enumeration-repeat` drive
  ``Enumerator/enumerateItems(for:startingAt:)`` over a wide directory. The repeat pass reports
  `rows rewritten`, counted from rows whose `syncTime` moved — which only a real write does.

## What it cannot measure

The File Provider framework supplies the observers, the scheduler and the `update-item` retry
machinery, and none of it exists outside a running extension. Everything that lives on that side has
to be measured from an installed build, through the extension's own log:

```bash
log stream --predicate 'subsystem CONTAINS "FileProviderExt"' --level debug
```

Worth counting in a captured window:

- **Dataless transitions** — `Updating item state to dataless.` Each one is an item the extension
  told the system to drop. A steady stream while nothing is being evicted is a materialisation
  flip-flop; see ``MaterializedEnumerationObserver``.
- **Redundant writes** — `Skipping item metadata write; database already holds this remote state.`
  against the number of items enumerated, which is the ingestion guard's hit rate.
- **Container churn** — repeated `update-item` jobs for the same container with
  `diffs:lastUsedDate|btime|mtime` mean the container's reported identity is not converging.

Compare the same capture before and after a change, on one account, with the same folders
materialised. Figures from different accounts are not comparable: everything here scales with the
size of the materialised set.
