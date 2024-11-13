====

macOS Virtual Files

====

.. index:: macosvfs

Introduction
============

Virtual file-based synchronisation for Nextcloud desktop users is now
available on macOS.

Unlike on Windows, virtual files support on macOS is provided by a
separate client version. This allows us to maintain the best possible
experience for classically-synced files, including sync status
integration and context menu actions, for users who want to keep using
this sync method. Just like our classic sync client, the macOS virtual
files client is released alongside the desktop client for Windows and
Linux, and will benefit from regular bug-fix and feature updates that
improve the user experience.

Supported features
------------------

- Per-file local retention and eviction
- Intelligent local copy eviction
- Integration with Spotlight
- File previews within Finder for virtual files
- Support for Apple-specific formats such as app bundles and iWork
  (Pages, Numbers, Keynote) bundles
- Remote file locking compatibility
- “Edit locally” support
- File sharing with other users
- Automatic synchronisation of remote changes **NOTE: we recommend the
  use of ``notify_push`` on the server!**
- More!

