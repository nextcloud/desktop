============
Introduction
============

Available for Windows, Mac OS X, and various Linux distributions, the ownCloud 
Desktop Sync client enables you to:

- Specify one or more directories on your computer that you want to synchronize
  to the ownCloud server.
- Always have the latest files synchronized, wherever they are located.

Your files are always automatically synchronized between your ownCloud server 
and local PC.

.. note:: Because of various technical issues, desktop sync clients older than 
   1.7 will not allowed to connect and sync with the ownCloud 8.1 server. It is 
   highly recommended to keep your client updated.
   
Improvements and New Features
-----------------------------

The 2.0 release of the ownCloud desktop sync client has many new features and 
improvements.

  * Multi-account support
  * Many UI improvements
  * Accessibility improvements (high contrast schemes)
  * Automatic bandwidth throttling
  * No redundant directory entries in activity log
  * Remove deleted accounts properly from toolbar
  * File manager integration: show hidden files as ignored
  * Do not sync new big folders from server without user's consent
  * Integrate selective sync into the default UI
  * More reliable reconnect after timeout
  * Improve progress reporting during sync
  * Sharing: Do not allow sharing the root folder
  * Sharing: Show thumbnail
  * Client Updater: Check for updates periodically, not only once per run
  * Quota: Only refresh from server when UI is shown
  * SSL Button: Show more information
  * System proxy: Ask user for credentials if needed
  * Several fixes and performance improvements in the sync engine
  * OS X: Show file name in UI if file has invalid UTF-8 in file name 
  * OS X: Support native finder integration for 10.10 Yosemite  
  * Network: Try to use SSL session tickets/identifiers
  * Windows: Support paths >255 characters
  * Windows, OS X: Allow to not sync hidden files
  * Windows: Remove misleading option to remove sync data
  * Windows: Do not provoke Active Directory account locking if password changes
  * Windows: Fix installer when installing unprivileged

.. note:: When you upgrade from 1.8, restart Windows to ensure that all new 
   features are visible.
   