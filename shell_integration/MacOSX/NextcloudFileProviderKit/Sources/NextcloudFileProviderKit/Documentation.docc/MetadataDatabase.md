<!--
SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Metadata Database

How the extension stores item metadata, and what happens when a database written by another build is found.

## Overview

Each File Provider domain keeps its metadata in one SQLite database, `<domain identifier>.sqlite`, next to its logs in the domain's support directory. ``FilesDatabaseManager`` is the only type which opens it. It uses GRDB with a database pool: one writer at a time, readers served from snapshots, write-ahead logging on.

Every method of ``FilesDatabaseManager`` enters the database exactly once and does its work in a worker which receives the open connection. Workers only call other workers. This matters because GRDB refuses nested access on the same pool, so a method must never call another public method while it holds the connection.

Rows leave the database as ``SendableItemMetadata`` and the other value types; the record types under `Database/Records` stay inside the database layer.

## Storage choices

- Dates are stored as `REAL` seconds since the reference date, the same number a `Date` holds, so they round-trip exactly. Predicates bind that number, never a `Date`, because GRDB would bind a `Date` as text.
- SQLite's `serverUrl` column holds a DAV parent path such as `files:/Docs` or `trash:`. The row decoder combines that path with the server base URL and backend user ID to build the WebDAV request URL. If migration cannot identify a legacy row's DAV root, the row keeps its full parent URL in that column. See <doc:UnicodePathNormalization> for the lookup keys.
- Items below a directory are matched with a byte-wise range over the location index rather than a pattern, so names with pattern characters or different case never match by accident.
- Array fields are stored as JSON text. Booleans are integers.
- Recorded upload chunks are keyed by upload identifier and chunk number and returned in chunk order.
- The `fileId` column is indexed for the push-notification lookup, which asks for thousands of identifiers at once.

## Durability

Writes run with `PRAGMA synchronous = FULL` by default: when a write returns, its commit is on disk. This covers everything the server cannot give back, such as pins, exclusion markers, lock tokens and upload bookkeeping. Bulk writes of server-derived rows, that is the depth-1 and paginated enumeration writes, the startup repair and the materialized-item mirror, run with `synchronous = NORMAL`; in write-ahead-log mode that survives a crash of the process and risks only the last commits before a power loss. A change-delivery session is created with `FULL`, because the item rows it describes are written before it and a session lost after them would leave nothing to deliver; its batch bookkeeping runs with `NORMAL`, since a lost acknowledgement is repeated and a lost batch marker expires the anchor. The level is a property of the writer connection and cannot change inside a transaction, so `FilesDatabaseManager.write(durability:)` sets it right before each transaction.

## When the database cannot be read

Lookups return the empty value when a read fails, and the failure is logged. Two lookups guard an action which must not happen on a guess: `excludedFromSyncMarkerExists(ocId:)` throws, and `Item.delete` then refuses the deletion with `cannotSynchronize` instead of deleting on the server; `hasRemoteFileChunks(uploadId:)` answers `true`, so local chunks are kept; `remoteFileChunks(uploadId:)` and `remoteChunkStoreFolderNames()` return `nil`, so an upload is not started from scratch and no stale upload is discarded on a guess. The two `changeDeliverySession` lookups and `prepareChangeDeliveryBatch` throw, so a failed read is never taken for an absent session or an empty batch. `pendingWorkingSetChanges(since:)` returns `nil`, which the working-set enumeration treats like a failed server scan: it keeps the incoming anchor so the changes are retried on the next signal.

A single damaged value does not take a query with it. A list column holding malformed JSON decodes as empty and is logged with the item's identifier; a mistyped scalar reads as the column type's zero value, which is how SQLite coerces it. Item rows are decoded one by one, so a row which cannot be decoded at all is skipped and logged rather than failing the result; such a row behaves like a missing one, and the next write of that item replaces it. `materialisedItemMetadatas()` returns `nil` when the read itself fails; the working-set enumeration then finishes with an error, the working-set scan keeps its sync point, and the materialized-item reconciliation does nothing.

The change-delivery buffer treats a failed read of its stored session or items the same way: it finishes the enumeration with an error and keeps the session, instead of taking an empty read for the last batch and acknowledging changes which were never delivered. The startup cleanup of chunk uploads decides in one read and skips the cleanup when that read fails.

On open, a file SQLite reports as corrupt is set aside with the suffix `.unreadable-<timestamp>` and replaced. The replacement carries a new store identity, so every anchor the framework kept for the domain expires and the containers are enumerated afresh. Any other open failure, and a failed import, keep the files untouched and make the open throw; the extension then reports the setup as failed and the domain stays unavailable until a later setup succeeds. Nothing is served from a substitute database, because nothing written to one would survive.

## Store version

`StoreVersion.current` names the generation of the on-disk format. It is bumped with every migration registered in `DatabaseSchema` and recorded twice after a successful open: in the file as `PRAGMA user_version`, and in the domain's defaults as `latestSeenDatabaseVersion`.

On open, the higher of the two recorded versions is compared with the running build:

- Absent: a new domain, or an import from Realm.
- Lower: the migrator brings the file up to date.
- Equal: nothing to do.
- Higher: the database was written by a newer build. It is removed and set up from scratch, because an older build cannot tell which of its assumptions the newer format broke. The marker is lowered to the running version.

The store version and the store identity are part of every sync anchor, so after such a reset the framework re-enumerates every container and the database is repopulated from the server.

The first SQLite schema creates the final tables directly. SQLite files written by earlier test builds use different migration identifiers and are treated as unsupported by the version check.

## Store identity

Every database file carries a random identity in its `storeIdentity` table, minted the first time the file is opened and read once per `FilesDatabaseManager`. Together with the store version it is part of every sync anchor the enumerators hand to the framework. An anchor from another file, whether that file was set aside as corrupt, removed after a downgrade or deleted by hand, is answered with `syncAnchorExpired`, so the framework re-enumerates the container instead of taking an empty store for a store without changes. Nothing is reimported, because the item identifiers are the server's `ocId` values and survive the replacement.

When the domain opens with different account details, the database can update rows it can match to the previous account. It changes their account fields, assigns a new store identity, and discards pending change batches saved for the previous account in one transaction. The rows keep their item IDs and local state. Writes and DAV reads check the store identity so a delayed response from the previous account cannot overwrite rows.

## Recovering item metadata after a server URL change

Older extensions saved the incoming account in the domain settings before opening the database. After a hostname change, the settings could therefore say `https://new.example.com` while cached rows still had `https://old.example.com` in their `urlBase` fields. The Realm import uses each row's account details to shorten its parent URL to a path like `files:/Team` and retains the row's `urlBase`.

The bulk update uses the previous account from SQLite or the domain settings when available. During Realm import, if the settings already show the new URL, the import can use the old URL from the rows. All live DAV rows must agree on one old account, their parent URLs must have converted to relative paths, and their login and backend user ID must match the current account. The import updates those rows before publishing the database, so normal enumeration uses the new URL.

When the rows disagree on the old account, startup leaves rows it cannot match alone. Requests for those items can retry, and the working set does not report a partial snapshot as complete.

## Import from Realm

Builds before the switch kept the metadata in a Realm database, `<domain identifier>.realm`. The importer opens it with the last Realm schema so Realm applies its own upgrades. It creates a staging SQLite file with the final schema, copies the rows and queued changes, and binds them to the current account in one transaction. It then moves the finished SQLite file into place and removes the Realm files. A Realm file which cannot be read is renamed with the suffix `.import-failed` and the database starts empty.

A Realm file next to an existing SQLite database means an older, Realm-based build ran last, so its content replaces the SQLite database. Only a Realm file which cannot be read at all is set aside; a file Realm cannot open for a passing reason, such as a permission or lock problem, is left in place and the open fails until the import succeeds.

The Realm dependency and the `LegacyRealm` models exist only for this import and are removed one release after the switch.

## Debug archives

The debug archive created from the desktop client's settings collects the `.sqlite` file and its `-wal` journal, and any Realm file which could not be imported.
