<!--
SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Chunked upload lifecycle

## Overview

Chunk handling belongs to the shared upload layer. Item creation and content
modification both provide a local file, an item identifier, and content dates
to `upload()`. That function chooses a standard or chunked transfer and owns
chunk identity, resume bookkeeping, and cleanup. `Item.modify()` handles File
Provider modification concerns such as changed fields, conflict headers,
progress, item status, and final metadata; it does not generate or delete
individual chunks.

Directories are never uploaded in chunks. Directory deletion participates in
chunk cleanup only because deleting a directory can also delete descendant
files with interrupted uploads.

## Upload identity

A chunked upload identifier is scoped to the item and the version of its
contents. When a modification date is available, `chunkUploadIdentifier()`
derives it from:

- the File Provider item identifier;
- the local file size; and
- the modification date, rounded to seconds.

The item identifier forms a prefix shared by all upload attempts for that
item. Repeating an upload with the same size and modification date produces
the same identifier and can resume the existing attempt. A changed size or
modification date produces a different identifier, so chunks from an older
version are not reused for new contents. The File Provider model is expected
to provide a changed content modification date when contents change.

If no modification date is available, the upload uses a unique identifier.
That attempt cannot be resumed by deriving the same identifier later, but it
also cannot accidentally reuse chunks from different contents.

## Local and persisted state

An interrupted chunked upload has three related forms of local state:

| State | Owner | Purpose |
| --- | --- | --- |
| Chunk files | The `RemoteInterface` implementation | The concrete files generated for transfer. |
| `RemoteFileChunk` rows | The upload layer | The chunks that have not completed, grouped by upload identifier. |
| `RealmItemMetadata.chunkUploadId` | The item database | Associates an existing file item with its current resumable upload. |

The protocol deliberately does not require callers to know where an adapter
stores chunk files. The NextcloudKit adapter currently creates a directory
named with the upload identifier under `FileManager.default.temporaryDirectory`.
Other adapters can use a different layout.

`RemoteInterface.chunkedUpload()` returns the concrete parent directory it
used. The upload layer deletes that exact directory after the attempt when it
is available. Cleanup performed later, when only the persisted identifier is
known, calls `RemoteInterface.removeLocalChunks()` and lets the adapter resolve
its own storage layout.

## Transfer and resume

Before a chunked transfer starts, `upload()` performs these steps:

1. Derive the identifier for the current file version.
2. Find uploads associated with the same item, including legacy bookkeeping
   found through the per-item identifier prefix.
3. Delete every matching upload except the current identifier. These are
   stale attempts for older contents.
4. Store the current identifier on existing item metadata.
5. Load only `RemoteFileChunk` rows with the current identifier and pass them
   to `RemoteInterface.chunkedUpload()` as the remaining chunks.

The adapter reports the complete chunk list when a new transfer starts. The
upload layer stores those chunks as `RemoteFileChunk` rows. Each completed
chunk removes its corresponding row. If an attempt with the same identifier
is retried, the remaining rows are supplied to the adapter so completed chunks
are not sent again.

For content modification, `modifyContents()` takes an item-metadata snapshot
before calling `upload()`. A failed upload can update the persisted
`chunkUploadId` while that call is running. Before the modification error path
writes its snapshot back, it refreshes `chunkUploadId` from Realm so it does
not overwrite the resumable identifier with the snapshot's older value.

## Completion and failure

After `RemoteInterface.chunkedUpload()` returns, the upload layer applies the
following rules:

| Result | Local directory | Chunk rows and metadata association |
| --- | --- | --- |
| Successful upload with a returned file | Removed | Removed or cleared. |
| Failed upload with remaining chunk rows | Preserved for resume | Preserved for resume. |
| Failed upload with no remaining chunk rows | Removed | Removed or cleared. |

Cleanup uses `defer` in `upload()` so the decision is applied at every return
from the chunked-transfer path. Bookkeeping is cleared only after local file
removal succeeds. If directory removal fails, the rows and metadata association
remain available for a later cleanup attempt.

## Item deletion

An item deletion records the affected file identifiers before deleting their
metadata. After the remote deletion succeeds, it discards chunk uploads for
those identifiers. A failed remote deletion preserves the chunks and their
bookkeeping.

For a file, only that file's identifier is considered. For a recursive
directory deletion, the extension queries the directory's descendant files
before their metadata is removed and cleans their uploads after the directory
deletion succeeds. The directory itself cannot own a chunked upload.

## Startup cleanup

After account authentication and database creation, extension setup runs one
cleanup pass. It collects upload identifiers from both `RemoteFileChunk` rows
and item metadata. An upload is retained only when its owning metadata:

- is not marked as deleted;
- still points to that upload identifier; and
- has status `inUpload`, `uploading`, or `uploadError`.

The pass therefore removes uploads for deleted or settled items, metadata-less
uploads, and superseded attempts that are no longer the current identifier.
As with other cleanup paths, failed local directory removal leaves bookkeeping
intact so cleanup can be retried.

> Important: Initial item creation does not currently persist item metadata
> until its upload succeeds. After an extension restart, an interrupted initial
> create therefore has chunk rows but no metadata proving that it is resumable,
> so startup cleanup removes it. Supporting resume for this case requires
> persisting pending-create ownership, or another durable distinction between
> pending initial creates and abandoned uploads.

## Regression coverage

The File Provider Kit tests cover:

- successful chunk generation and cleanup;
- retention and resumption after a partial failure;
- removal of stale chunks when an item's contents produce a new identifier;
- preservation of a failed modification's current metadata association;
- cleanup after successful file and recursive directory deletion;
- startup removal of deleted, settled, metadata-less, and superseded uploads;
- preservation of resumable uploads during startup cleanup; and
- retention of bookkeeping when local directory removal fails.
