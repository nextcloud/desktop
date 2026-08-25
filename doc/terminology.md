<!--
  - SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->

# Codebase terminology

Use these terms consistently in comments and documentation. The client has two related but different engines, so vocabulary is scoped below:

- The **standard sync engine** is the C++ `SyncEngine`/`Folder` path, including its VFS integrations.
- The **File Provider engine** is the macOS Swift extension built on Apple's File Provider framework.

Do not automatically carry a term from one engine into the other. The framework's terms are often the right choice for File Provider code, even when the standard sync engine has a similar concept with a different name.

## State and lifetime

| Term | Use it for | Do not use it for |
| --- | --- | --- |
| **persisted** | Data saved in Realm, settings, a journal, or a file. | Data held only in memory. |
| **cached** | A copy that can be stale, discarded, or rebuilt. | The authoritative record. |
| **runtime-only** | State that is not saved across the relevant boundary. | State saved for recovery. |
| **retained** | An object, reference, or resource kept alive. | A field copied through a merge; use **preserved**. |
| **preserved** | A value copied into an updated record without being overwritten. | Object ownership or lifetime. |
| **survives** | A value or object that remains after a named event. | An unqualified claim about persistence. |

When the lifecycle guarantee matters, describe it separately from storage: “persisted in Realm and survives an extension restart” is clearer than “durable state.”

## Standard sync engine and VFS

| Term | Use it for | Do not use it for |
| --- | --- | --- |
| **virtual file** | A VFS-managed local file representation whose contents may not be available locally. | The file's contents themselves. |
| **placeholder** | The filesystem representation of a virtual file. | The download operation itself. |
| **hydrated** | A placeholder whose contents are available locally. | A file that is merely pinned. |
| **dehydrated** | A placeholder whose contents are not available locally. | A file that has been deleted. |
| **hydration** | Downloading the contents of a virtual file. | Changing its pin state. |
| **pin state** | The VFS availability policy, represented by `PinState` and related APIs. | The current hydration state. |

In this engine, a placeholder can be hydrated or dehydrated. Hydration is the download operation; pinning is the user's availability preference. They are related, but one does not imply the other.

## File Provider engine

File Provider uses its own vocabulary. Prefer these terms in the Swift package and extension rather than translating them into standard-sync terminology:

| Term | Use it for |
| --- | --- |
| **domain** | A registered File Provider account scope. |
| **container** | An enumeration scope within a domain, such as root, working set, or trash. |
| **item** | A File Provider object identified by an item identifier. Use **file** or **folder** when its type matters. |
| **enumerator** | The object handling an enumeration request. |
| **materialized** | An item that File Provider treats as locally present. Keep the existing `Materialised...` spelling in public API names. |
| **evict** | Removing an item's local File Provider representation without treating it as a server deletion. |
| **keepDownloaded** | The local metadata flag behind the “Always keep downloaded” behavior. |
| **anchor** | The framework value passed between enumeration requests to identify where enumeration should continue. |
| **cursor** | The position within a stored change list or delivery session. |
| **snapshot** | A copied value used after the live value may have changed. |
| **metadata** | Qualify this when needed: `item metadata`, `server metadata`, or `persisted metadata`. |

An anchor may identify the container's change position or a pending batch. The specific anchor formats belong in the change-enumeration documentation, not in the general vocabulary list.

The terms do not map one-to-one between engines:

| Standard sync engine | File Provider engine | Notes |
| --- | --- | --- |
| virtual file / placeholder | File Provider item | Both are local representations, but `item` is the framework object. |
| hydrated / dehydrated | materialized / evicted | These describe related local-content states, but are not interchangeable API terms. |
| `PinState` | `keepDownloaded` / content policy | Both express availability intent, but belong to different implementations. |
| hydration | File Provider materialization or download | Use the term exposed by the code path being discussed. |

## Paths and deletion

| Term | Use it for |
| --- | --- |
| **local filesystem** | Files and directories on the computer. |
| **local database** | Client-side stored metadata or state. |
| **remote server** | Data and operations on the Nextcloud server. |
| **directory** | A filesystem or path structure. |
| **folder** | A user-facing or sync-folder concept. |
| **delete** | A deletion operation. |
| **soft-deleted** | A record marked as deleted but not yet removed. |
| **evict** | A provider or operating-system eviction operation. |
| **trash** | The client or server's deleted-item area and its related operations. Describe permanent removal directly when needed. |


