Sync Algorithm
==============

Overview
--------

This is a technical description of the synchronization (sync) algorithm used by the Nextcloud client.

The sync algorithm is the thing that looks at the local and remote file system trees and the sync journal and decides which steps need to be taken to bring the two trees into synchronization. It's different from the propagator, whose job it is to actually execute these steps.


Definitions
-----------

  - local tree: The files and directories on the local file system that shall be kept in sync with the remote tree.
  - remote tree: The files and directories on the Nextcloud server that shall be kept in sync with the local tree.
  - sync journal (journal): A snapshot of file and directory metadata that the sync algorithm uses as a baseline to detect local or remote changes. Typically stored in a database.
  - file and directory metadata:
    - mtimes
    - sizes
    - inodes (journal and local only): Representation of filesystem object. Useful for rename detection.
    - etags (journal and remote only): The server assigns a new etag when a file or directory changes.
    - checksums (journal and remote only): Checksum algorithm applied to a file's contents.
    - permissions (journal and remote only)


Phases
------

### Discovery (aka Update)

The discovery phase collects file and directory metadata from the local and remote trees, detecting differences between each tree and the journal.

Afterwards, we have two trees that tell us what happened relative to the journal. But there may still be conflicts if something happened to an entity both locally and on the remote.

  - Input: file system, server data, journal
  - Output: two FileMap (std::map<QByteArray, std::unique_ptr<csync_file_stat_t>>), representing the local and remote trees

  - Note on remote discovery: Since a change to a file on the server causes the etags of all parent folders to change, folders with an unchanged etag can be read from the journal directly and don't need to be walked into.

  - Details
    - csync_update() uses csync_ftw() on the local and remote trees, one after the other.
    - csync_ftw() iterates through the entities in a tree and calls csync_walker() for each.
    - csync_walker() calls _csync_detect_update() on each.
    - _csync_detect_update() compares the item to its journal entry (if any) to detect new, changed or renamed files. This is the main function of this pass.



### Reconcile

The reconcile phase compares and adjusts the local and remote trees (in both directions), detecting conflicts.

Afterwards, there are still two trees, but conflicts are marked in them.

  - Input: FileMap for the local and remote trees, journal (for some rename-related queries)
  - Output: changes FileMap in-place

  - Details
    - csync_reconcile() runs csync_reconcile_updates() for the local and remote trees, one after the other.
    - csync_reconcile_updates() iterates through the entries, calling _csync_merge_algorithm_visitor() for each.
    - _csync_merge_algorithm_visitor() checks whether the other tree also has an entry for that node and merges the actions, detecting conflicts. This is the main function of this pass.


### Post-Reconcile

The post-reconcile phase merges the two trees into one set of SyncFileItems.

Afterwards, there is a list of items that can tell the propagator what needs to be done.

  - Input: FileMap for the local and remote trees
  - Output: QMap<QString, SyncFileItemPtr>

  - Note that some "propagations", specifically cheap metadata-only updates, are already done at this stage.

  - Details
    - csync_walk_local_tree() and csync_walk_remote_tree() are called.
    - They use _csync_walk_tree() to run SyncEngine::treewalkFile() on each entry.
    - treewalkFile() creates and fills SyncFileItems for each entry, ensuring that each file only has a single instance. This is the main function of this pass.


See Also
--------

An overview of the propagation steps is still missing. The sync protocol is documented at https://github.com/cernbox/smashbox/blob/master/protocol/protocol.md.
