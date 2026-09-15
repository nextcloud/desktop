<!--
SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Safe deletion after excluding an item from sync

## Overview

Returning `NSFileProviderError(.excludedFromSync)` from an item creation or
modification callback starts a system-managed removal sequence. It does not
only report that the operation was rejected.

For a materialized item, macOS can perform the following callbacks:

1. The extension enumerates an item that already exists on the server.
2. The user changes the item locally and macOS calls `modifyItem`.
3. The extension returns `.excludedFromSync` because it cannot synchronize the
   changed item.
4. macOS fetches the item and its descendants so the package is locally
   available.
5. macOS calls `deleteItem` for the same item identifier to remove it from the
   File Provider domain.

The final callback looks like an ordinary user-requested deletion. If it is
handled by the normal deletion path, the extension sends a WebDAV deletion to
the server. The item is then removed for every client, even though the
extension intended only to exclude the unsupported local item. This behavior
caused [desktop issue #10521](https://github.com/nextcloud/desktop/issues/10521).

## Provider-owned exclusion intent

The extension must establish the reason for the future deletion before it
returns `.excludedFromSync`.

When a bundle or package modification is rejected, `Item.modify` stores a
`RealmExcludedFromSyncItem` record keyed by the item's `ocId`. Only after that
write succeeds does it return `.excludedFromSync`. If the record cannot be
written, the modification fails with `.cannotSynchronize` instead of starting
a deletion sequence that cannot be recognized safely.

The exclusion record is stored separately from `RealmItemMetadata`. Fetching,
materializing, and enumerating an item can replace its item metadata with a
fresh server-derived value before the deletion callback arrives. A field on
that replaceable value could therefore be lost at exactly the point where it
is needed. The separate Realm object survives those writes and extension
process restarts. It was introduced with Realm schema version 205.

When `Item.delete` receives the subsequent callback, it checks for this record
before invoking the remote interface:

- If the record exists, only the local item metadata and descendants are
  marked as deleted. No WebDAV deletion is sent. The exclusion record is
  removed after local cleanup succeeds.
- If the record does not exist, deletion follows the ordinary path and is
  propagated to the server. This includes an explicit user deletion of a
  bundle or package.

Failures during local cleanup or removal of the exclusion record return
`.cannotSynchronize`. The record remains available for a retry, preventing a
retry from falling through to remote deletion.

## Why item type is not sufficient

The deletion path must not skip every bundle or package. A user can explicitly
delete a package and reasonably expect that deletion to synchronize. The item
type describes what is being deleted, but not why macOS requested deletion.

The durable exclusion record supplies that missing intent. It is created only
by a provider-controlled operation that will return `.excludedFromSync` and
is consumed only by the resulting deletion callback.

For the same reason, the extension does not vend
`NSFileProviderItemCapabilities.allowsExcludingFromSync`. Finder can represent
“Do not synchronize” as an ordinary deletion callback without first passing
through the provider-controlled modification path. There would be no durable
intent record with which to distinguish it from a real deletion.

Ignored files are handled independently. Their deletion path re-evaluates the
ignore matcher and performs local-only cleanup without using an exclusion
record.

## Diagnosing this sequence

Use the File Provider extension's JSONL log, not only the main desktop-client
log. A failing sequence can be identified by the same item identifier appearing
in this order:

1. A request to modify a bundle or package.
2. Completion of the modification with error code `-2010`
   (`NSFileProviderError.excludedFromSync`).
3. A request to fetch the item's contents, followed by successful
   materialization.
4. A request to delete the same item.
5. A successful remote deletion for its WebDAV path.

After this safeguard, the fourth event is followed by local metadata cleanup
and no remote deletion request.

## Rules for future changes

Any new path that returns `.excludedFromSync` for an item that may already
exist on the server must follow the same contract:

1. Persist provider-owned exclusion intent before returning the error.
2. Do not store that intent only in metadata that remote synchronization can
   replace.
3. Do not issue a remote deletion when consuming the intent.
4. Remove the intent only after local cleanup succeeds.
5. Preserve ordinary remote deletion for items without exclusion intent.

Regression coverage is provided by
`ItemModifyTests.testModifyRemoteBundleExclusionDoesNotDeleteRemoteBundle`,
which exercises the complete modification, metadata replacement, and deletion
sequence. `ItemDeleteTests.testDeleteExcludedBundleDoesNotPropagateToServer`
and `testDeleteUnexcludedBundlePropagatesToServer` directly cover both branches
of the deletion contract.
