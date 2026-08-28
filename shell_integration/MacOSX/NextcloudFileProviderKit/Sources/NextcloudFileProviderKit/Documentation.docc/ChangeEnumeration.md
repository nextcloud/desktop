<!--
SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Change enumeration

Change enumeration reports items that changed after a sync anchor. It is
different from item enumeration:

- Item enumeration reports the current contents of a container through
  `NSFileProviderEnumerationObserver`.
- Change enumeration reports updates and deletions through
  `NSFileProviderChangeObserver`.

Server-side WebDAV pagination, where used by a remote read, is also separate
from File Provider change batching. The remote read must finish before its
changes can be delivered to the observer.

The main entry point is
``Enumerator/enumerateChanges(for:from:)``. It performs three steps:

1. Recognises a continuation from an earlier batch.
2. Validates a normal sync anchor when the request is not a continuation.
3. Dispatches the request to the working set, trash, or a regular directory.

## Containers

The enumerator has a different change source for each container type.

| Container | Change source | Final anchor |
| --- | --- | --- |
| Working set | Scans materialised items on the server and combines those results with pending local changes. | A new current anchor when the scan completes. If a remote read fails, the incoming anchor is retained. |
| Regular directory | Reads the directory from WebDAV and compares it with local metadata. | The incoming anchor. |
| Trash | Lists the server trash and finds local trash rows that no longer exist there. | The incoming anchor. |

Only the working set is signalled by remote-change notifications. A working-set
scan can read changed descendants of materialised directories, so it may find
changes that are not represented by the local pending-change query alone.

## Normal sync anchors

Normal anchors use this format:

```text
<extension-version>|<ISO-8601-date>
```

`Enumerator.syncAnchor(at:)` creates them. The extension checks the format and
the embedded version in `validatedSyncAnchorDate(_:reportingTo:)`.

An invalid or old-version anchor is reported as
`NSFileProviderError(.syncAnchorExpired)`. File Provider can then discard its
cached state and request a fresh item enumeration.

The final anchor is not necessarily the anchor passed to the request. A
successful working-set scan advances to the enumerator's current anchor. A
scan with skipped remote reads keeps the incoming anchor so the next working-set
signal retries the missed work.

## Deriving and storing changes

The working-set and regular-directory scans update local metadata while they
run. Repeating a scan for every batch could therefore hide changes that were
already written to the database.

Each scan first creates one ordered change list:

1. Created and updated items are sorted by remote path length, so parents come
   before children.
2. Deleted items follow the updates.
3. The complete list is stored in the change-delivery buffer.

The buffer stores the list in the File Provider domain's Realm database. A
`RealmChangeDeliverySession` records the current anchor, cursor, final anchor,
and incomplete-scan state. Each `RealmChangeDeliveryItem` stores one metadata
value, its sequence number, and whether it is a deletion.

This state is separate from the live item metadata. It preserves the exact
value that was derived, including metadata for an item that may be removed from
the live database before the next batch is requested.

## Batches

File Provider supplies `suggestedBatchSize` on the change observer. The
enumerator uses that value, falls back to 1,000 when it is absent or zero, and
limits it to 4,000.

`ChangeDeliveryBuffer.takeBatch(maxItems:)` then:

1. Reads the next `maxItems + 1` stored changes.
2. Reports at most `maxItems` changes.
3. Uses the extra item to determine `moreComing`.
4. Advances the stored cursor.

Updates and deletions share the same limit. For a limit of three, two updates
and one deletion consume the whole batch.

One call to `enumerateChanges(for:from:)` reports one batch. The enumerator
does not loop through all batches. It calls
`finishEnumeratingChanges(upTo:moreComing:)`; when `moreComing` is true, File
Provider requests the next batch in a later call.

For example:

```text
Stored changes: 0 1 2 3 4 5 6 7 8 9
Batch size:     3

Request 1:      0 1 2, moreComing = true
Request 2:      3 4 5, moreComing = true
Request 3:      6 7 8, moreComing = true
Request 4:      9,     moreComing = false
```

## Continuation anchors

An intermediate batch returns an opaque continuation anchor. It is not a
normal version-tagged sync anchor. The anchor identifies the saved pending
change list and its next position.

The next request must check for this continuation before normal sync-anchor
validation. When the saved list is found, the enumerator reads the next batch
without contacting the server again. The final batch returns the container's
final sync anchor instead of a continuation anchor.

The framework may invalidate the current `Enumerator` after an intermediate
batch and create another one for the next request. The extension process may
also be restarted. Pending changes must therefore be stored outside the
enumerator instance or be exactly reconstructible from the anchor. This
implementation uses the Realm-backed list for both cases.

Do not assume that a process restart causes File Provider to provide the
original anchor again. The last anchor returned for a batch may be the anchor
used for the next request. See Apple's documentation for
[`NSFileProviderChangeObserver`](https://developer.apple.com/documentation/fileprovider/nsfileproviderchangeobserver),
[`finishEnumeratingChanges(upTo:moreComing:)`](https://developer.apple.com/documentation/fileprovider/nsfileproviderchangeobserver/finishenumeratingchanges%28upto%3Amorecoming%3A%29),
and [`NSFileProviderSyncAnchor`](https://developer.apple.com/documentation/fileprovider/nsfileprovidersyncanchor).

## Incomplete working-set scans

The working-set scan continues after an individual remote read fails. It
records that the scan was incomplete and skips that item for the current pass.
The changes found from other items are still delivered.

When the stored list is exhausted, an incomplete scan returns the incoming
anchor. The next working-set notification can then retry the unreadable item.
This prevents the extension from claiming that it has synchronised past a
change it did not inspect.

Regular-directory and trash enumeration use their own remote-read error paths.
A missing regular directory is reported as a deletion, a no-change response
finishes without changes, and other read errors finish with an error.

## Diagnostics

The following messages describe one multi-batch sequence:

```text
Reporting change batch. updated: 200, deleted: 0, moreComing: true
Enumerator is being invalidated.
System requested enumerator.
Enumerating changes (anchor: ...).
```

The last request may be handled by a different `Enumerator` instance. That is
expected. The important check is that the returned anchor is recognised and
the next batch is delivered without re-running the remote change scan.

If a remote read fails, look for the corresponding
`Read of materialised item failed during working-set scan` message. An
incomplete scan should also log that it retained the incoming sync anchor.

## Tests

The change-enumeration tests are in
`Tests/NextcloudFileProviderKitTests/EnumeratorTests.swift`:

- `testWorkingSetChangesDeliveredInBatchesRespectingSuggestedBatchSize`
  checks the combined batch limit, ordering, unique continuation anchors, and
  that the server scan runs once.
- `testWorkingSetChangesResumeAcrossNewEnumeratorInstances` creates a new
  enumerator for every continuation request and checks that no changes are
  lost or duplicated.
- `testWorkingSetChangesBatchMixingUpdatesAndDeletions` checks the shared
  update/deletion budget.
- `testContainerChangesDeliveredInBatches` checks regular-directory changes.
- `testTrashChangesDeliveredInBatches` checks batched trash deletions.

These tests exercise the delivery logic in the package. A signed macOS build
is still required to verify the interaction with the real `fileproviderd`.
