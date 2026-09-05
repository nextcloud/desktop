<!--
SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Unicode path normalization

## Overview

File names that look identical can have different Unicode representations.
For example, the name `prêt` can be encoded as either:

- NFC: `ê` (`U+00EA`)
- NFD: `e` (`U+0065`) followed by a combining circumflex (`U+0302`)

These representations render the same way, but they are different Swift
strings. Realm equality predicates compare their stored values, so NFC and
NFD values do not match unless the application normalizes them first.

This matters particularly on macOS. The server, NextcloudKit, File Provider,
and the macOS file system are separate boundaries and must not be assumed to
return the same Unicode form. A server response can therefore be followed by
a File Provider callback containing a canonically equivalent, but byte-wise
different, URL or file name.

(This is particularly problematic as, unlike most other systems, macOS
uses NFD normalisation).

https://eclecticlight.co/2021/05/08/explainer-unicode-normalization-and-apfs/

## The data model

`RealmItemMetadata` stores both the original values and comparison values:

- `serverUrl` and `fileName` retain the spelling received from the server or
  operating-system boundary.
- `normalizedServerUrl` and `normalizedFileName` contain NFC values produced
  by Foundation's `precomposedStringWithCanonicalMapping` property.

The original values must not be replaced globally. They are used for server
requests, downloads, uploads, deletes, logging, and user-visible metadata.
The normalized values are local identity keys only.

New objects populate both forms. Realm schema migration version 203 backfills
the normalized properties for rows created by earlier versions, and
`FilesDatabaseManager.repairDriftedNormalizedLocationKeys()` runs at every
open to repair any row whose keys have drifted from its raw columns.

Drift is a mismatch between a stored key and the normalization of the raw
column it is derived from, so it cannot be expressed as a Realm query: an
index can only be probed for a value, and the value a drifted key should hold
is whatever `precomposedStringWithCanonicalMapping` returns for that row. The
repair therefore walks the whole table once per open and normalizes both raw
columns of every row. Rows are collected into an array rather than a lazy
`Results`, which is also what makes the subsequent rewrite safe, because
mutating the columns a live query reads would let it skip rows. Deleted rows,
lock files of local origin, and the synthetic root container are repaired
alongside everything else: the exclusions that `cleanupPreexistingLogicalDuplicates()`
applies are about which rows may be soft-deleted, not about which rows have to
be findable by location.

## Where normalization is required

Normalization is relevant anywhere the package decides whether two persisted
items refer to the same location:

- looking up an item by account, parent URL, and file name;
- comparing metadata locations in memory;
- detecting a path change during enumeration;
- evicting a duplicate with a different `ocId`;
- cleaning up duplicate rows already present at startup;
- finding direct children and descendants of a directory;
- recursively deleting or renaming directory contents;
- finding trash items and propagating working-set changes.

Realm query closures cannot call ordinary Swift methods because Realm
translates them into database queries. The reusable query expressions in
`RealmItemMetadata+Queries.swift` centralize the persisted-field predicates:
`hasLocation` compares an account, normalized parent URL, and normalized file
name; `hasServerUrl` compares an exact URL or a slash-delimited descendant
path. Both helpers compare the normalized properties alone; the exact-match
forms are answered from their indexes, while the descendant form stays a prefix
disjunction that Realm cannot drive off an index.

For ordinary in-memory values, `ItemMetadata.hasSameLocation(as:)` provides the
corresponding comparison without exposing normalization details at each call
site.

## Duplicate cleanup

The startup method
`FilesDatabaseManager.cleanupPreexistingLogicalDuplicates()` scans existing
rows and groups them by account, normalized parent URL, and normalized file
name. This is important even after the migration:

1. Before the fix, two canonically equivalent paths could have been persisted
   as different raw strings.
2. Grouping those rows by raw strings would keep them in separate buckets.
3. Grouping by normalized properties makes them one logical location.
4. The newest settled row wins; other settled rows are soft-deleted.
5. In-flight rows are not deleted because active transfers refer to their
   specific `ocId`.

The related `evictLogicalDuplicates(of:in:now:)` path applies the same
normalized-location rule while processing newly received metadata. The raw
server spelling of the surviving row is preserved.

## Relationship to server deletion

A failed local Realm lookup does not by itself issue a server-side delete.
The explicit File Provider deletion path is responsible for remote deletion
and ultimately calls the remote interface's delete operation. Normalization
prevents local identity and reconciliation errors, but it is not a claim that
every historical deletion was caused by a lookup miss.

The File Provider extension has its own JSONL logging. The main desktop Qt log
does not necessarily contain extension-level failures, so extension behavior
must be investigated using the File Provider logs and the relevant remote
requests.

## Compatibility rules

When changing metadata persistence or adding a database predicate:

1. Preserve the raw `serverUrl` and `fileName`.
2. Populate or update their normalized counterparts in the same write.
3. Use `hasLocation`, `hasServerUrl`, or `hasSameLocation` instead of
   duplicating normalization logic.
4. Keep slash boundaries when matching descendants so `folder` does not match
   `folder2`.
5. Include migration or legacy-row behavior when adding new normalized fields.

## Regression coverage

The File Provider Kit tests cover:

- finding an item through an NFC/NFD-equivalent remote path;
- deduplicating canonically equivalent paths while preserving raw spelling;
- comparing metadata locations with different Unicode forms;
- startup cleanup of pre-existing canonical duplicates;
- directory, descendant, trash, rename, and working-set queries.
