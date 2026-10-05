# Metadata Database

How the extension stores item metadata, and what happens when a database written by another build is found.

## Overview

Each File Provider domain keeps its metadata in one SQLite database, `<domain identifier>.sqlite`, next to its logs in the domain's support directory. ``FilesDatabaseManager`` is the only type which opens it. It uses GRDB with a database pool: one writer at a time, readers served from snapshots, write-ahead logging on.

Every method of ``FilesDatabaseManager`` enters the database exactly once and does its work in a worker which receives the open connection. Workers only call other workers. This matters because GRDB refuses nested access on the same pool, so a method must never call another public method while it holds the connection.

Rows leave the database as ``SendableItemMetadata`` and the other value types; the record types under `Database/Records` stay inside the database layer.

## Storage choices

- Dates are stored as `REAL` seconds since the reference date, the same number a `Date` holds, so they round-trip exactly. Predicates bind that number, never a `Date`, because GRDB would bind a `Date` as text.
- The location keys `normalizedServerUrl` and `normalizedFileName` hold the NFC form of the raw location. See <doc:UnicodePathNormalization>.
- Items below a directory are matched with a byte-wise range over the location index rather than a pattern, so names with pattern characters or different case never match by accident.
- Array fields are stored as JSON text. Booleans are integers.
- Recorded upload chunks are keyed by upload identifier and chunk number and returned in chunk order.

## When the database cannot be read

Lookups return the empty value when a read fails, and the failure is logged. Two lookups guard an action which must not happen on a guess: `excludedFromSyncMarkerExists(ocId:)` throws, and `Item.delete` then refuses the deletion with `cannotSynchronize` instead of deleting on the server; `hasRemoteFileChunks(uploadId:)` answers `true`, so local chunks are kept. `pendingWorkingSetChanges(since:)` returns `nil`, which the working-set enumeration treats like a failed server scan: it keeps the incoming anchor so the changes are retried on the next signal.

The change-delivery buffer treats a failed read of its stored items the same way: it finishes the enumeration with an error and keeps the session, instead of taking an empty read for the last batch and acknowledging changes which were never delivered. The startup cleanup of chunk uploads decides in one read and skips the cleanup when that read fails.

On open, a file SQLite reports as corrupt is set aside with the suffix `.unreadable-<timestamp>` and replaced. Any other open failure, and a failed import, keep the files untouched and make the open throw; the extension then reports the setup as failed and the domain stays unavailable until a later setup succeeds. Nothing is served from a substitute database, because nothing written to one would survive.

## Store version

`StoreVersion.current` names the generation of the on-disk format. It is bumped with every migration registered in `DatabaseSchema` and recorded twice after a successful open: in the file as `PRAGMA user_version`, and in the domain's defaults as `latestSeenDatabaseVersion`.

On open, the higher of the two recorded versions is compared with the running build:

- Absent: a new domain, or an import from Realm.
- Lower: the migrator brings the file up to date.
- Equal: nothing to do.
- Higher: the database was written by a newer build. It is removed and set up from scratch, because an older build cannot tell which of its assumptions the newer format broke. The marker is lowered to the running version.

The store version is part of every sync anchor, so after such a reset the framework re-enumerates every container and the database is repopulated from the server.

## Import from Realm

Builds before the switch kept the same tables in a Realm database, `<domain identifier>.realm`. When that file exists, it is imported before the SQLite database is opened: the Realm file is opened with the last Realm schema so Realm applies its own upgrades first, every table is copied into a staging file, and the staging file is moved into place in one step. The Realm files are removed only after the move succeeded, so a crash leaves either the untouched Realm file or a complete database behind. A Realm file which cannot be read is renamed with the suffix `.import-failed` and the database starts empty.

A Realm file next to an existing SQLite database means an older, Realm-based build ran last, so its content replaces the SQLite database. Only a Realm file which cannot be read at all is set aside; a file Realm cannot open for a passing reason, such as a permission or lock problem, is left in place and the open fails until the import succeeds.

The Realm dependency and the `LegacyRealm` models exist only for this import and are removed one release after the switch.

## Debug archives

The debug archive created from the desktop client's settings collects the `.sqlite` file and its `-wal` journal, and any Realm file which could not be imported.
