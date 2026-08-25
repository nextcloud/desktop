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

When the lifecycle guarantee matters, describe it separately from storage: “persisted in Realm and survives an extension restart” states both facts clearly.

## State, results, and configuration

| Term | Use it for | Do not use it for |
| --- | --- | --- |
| **state** | The current condition of an object, account, or operation. | A requested setting; use **policy** or **configuration**. |
| **status** | The result or progress of an operation, especially a transfer. | Every kind of state. Use the exact enum name when one exists. |
| **mode** | A selected implementation or operating configuration, such as a VFS mode. | A temporary result or transfer status. |
| **policy** | A rule or preference that controls what should happen, such as pinning or keeping a file downloaded. | Proof that the requested result has already happened. |
| **configuration** | Values that select or set up how a component operates. | A live operation result. |
| **error** | A failure or failure result. | A conflict or warning unless the code treats it as an error. |
| **conflict** | A specific sync outcome where changes cannot be applied together automatically. | A general failure. |

Use the exact enum or property name when the distinction matters. For example, File Provider transfer status and the standard engine's sync result status are different concepts even though both use the word “status.”

## Standard sync lifecycle

| Term | Use it for |
| --- | --- |
| **sync run** | One complete standard-engine cycle for a folder. |
| **discovery** | Reading local and remote state and building the items that need action. |
| **local discovery** | Reading the local filesystem or local database during discovery. |
| **remote discovery** | Reading the server state during discovery. |
| **sync item** | One file or folder operation represented in a sync run. |
| **propagation** | Applying accepted sync items through operations such as upload, download, move, or delete. |
| **reconcile** | Comparing incoming state with existing state and deciding how to combine or resolve it. |

Use **enumeration** for File Provider requests. Do not use it as a general replacement for standard-engine **discovery**.

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
| **enumeration** | A File Provider request that lists items or reports changes. |
| **enumerator** | The object handling an enumeration request. |
| **change batch** | A group of item changes delivered in one enumeration response. |
| **materialized** | An item in File Provider's local materialized set. This can include a downloaded file or a visited directory. |
| **downloaded** | The local file-content flag used for a File Provider file. |
| **visitedDirectory** | A directory that has been enumerated locally; it does not mean the directory was downloaded. |
| **dataless** | A File Provider item with no local materialized content, typically after eviction. |
| **evict** | Removing an item's local File Provider representation without treating it as a server deletion. |
| **keepDownloaded** | The stored intent behind “Always keep downloaded”; it is not the current downloaded or materialized state. |
| **anchor** | The framework value passed between enumeration requests to identify where enumeration should continue. |
| **cursor** | The position within a stored change list or delivery session. |

An anchor may identify the container's change position or a pending batch. The specific anchor formats belong in the change-enumeration documentation, not in the general vocabulary list.

Use **materialized** in new prose. Preserve exact existing identifiers, including `MaterializedEnumerationObserver`.

The terms do not map one-to-one between engines:

| Standard sync engine | File Provider engine | Notes |
| --- | --- | --- |
| virtual file / placeholder | File Provider item | Both are local representations, but `item` is the framework object. |
| hydrated / dehydrated | materialized / dataless | These describe related local-content states, but are not interchangeable API terms. |
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

## Data representations

| Term | Use it for | Do not use it for |
| --- | --- | --- |
| **record** | A stored database object, such as a sync-journal or Realm record. | An arbitrary in-memory object. |
| **row** | A specific SQL or table row. | Every database record. |
| **metadata** | Stored descriptive data about an item or operation. Qualify it when the source matters. | The item or file itself. |
| **properties** | Values exposed by a platform or API object, such as a File Provider item. | Stored database data unless the API calls it properties. |
| **attributes** | Filesystem or platform attributes, such as size, dates, or permissions. | Sync metadata in general. |
| **snapshot** | A copied view of values used after the live object may have changed. | The live object or database record. |
