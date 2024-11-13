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

Installation and initial setup
==============================

The virtual files client is distributed as an installer package that
resembles the macOS classic sync client. The desktop client can be
installed by following the steps presented by the installer.

The virtual files desktop client is interchangeable with the classic
sync desktop client. This means your existing accounts and settings will
carry over to this client and vice-versa, should you ever decide to go
back to the classic sync client. This includes any pre-existing standard
sync folders, as the virtual files client also supports classic sync.

.. note::
    Due to technical limitations in macOS we are unable to provide
    integration in Finder for both classic sync folders and virtual file
    sync folders. Classic sync folders in the virtual files client will
    therefore not have Finder integrations such as sync state icons or
    context menu actions.


Any existing or newly-configured accounts will have virtual files
automatically enabled. On macOS, each account’s virtual files live under
their own domain, separate from any pre-existing classic sync folders.
These domains can be found listed under the “Locations” group in the
Finder sidebar.

.. image:: images/macosvfs-finder-sidebar.png
   :alt: Finder sidebar showing virtual files domains

Upon first accessing one of these domains, the desktop client will being
to request information on remote files from the server. This first
synchronisation may take some time depending on the quantity of files
hosted on the server.

