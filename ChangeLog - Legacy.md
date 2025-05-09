<!--
  - SPDX-FileCopyrightText: 2019 Nextcloud GmbH and Nextcloud contributors
  - SPDX-FileCopyrightText: 2012 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->
ChangeLog - Legacy
====================
For details check https://nextcloud.com/blog/category/release.

2.6 Series ChangeLog
====================

version 2.6.4 (release 2020-03-04)
* Fix Explorer pinning: Add fallbacks for Shell commands (fixes #1599)
* WebFlowCredentials: Make username comparison case-insensitive (fix #1741)
* ActivityListModel: Show full date and time as a Tooltip only
* Disable HTTP/2 for now due to Qt bug, allow enabling it via env var
* Fix Explorer integration re-save and hide option on non-Windows
* l10n: Changes to improve source strings
* Updater: Add query-parameter 'updatesegment' to the update check
* Allow Desktop translation merging and custom backport branches

version 2.6.3 (release 2020-02-17)
* Add UserInfo class and fetch quota via API instead of PropfindJob
* l10n: Changed grammar and triple dots to ellipsis
* l10n: Changed spelling of "user name" to "username"
* Start the client in background if activated by D-Bus
* Do not install files related to cloud providers under Xenial
* Make sure that the libcloudprovider integration is using a valid D-Bus path
* Changed product name to Nextcloud
* Update autoupdate.rst
* Use system proxy by default if no config file is present
* Install libcloudproviders files by default on debian
* Windows 7 is out of support
* BugFix: Handle broken shared file error gracefully
* Fix Explorer integration on Windows and the crash on other systems
* Welcome to 2020
* Updated year in legalnotice.cpp
* apply http2 qt resend patch from owncloud

version 2.6.2 (release 2019-12-24)
* Revert DEFAULT_REMOTE_POLL_INTERVAL back from 5 seconds to 30 seconds
* Use user-provided username in displayName() - Fix #836
* Fix typo
* Build with libcloudproviders on debian and in the AppImage
* Fix tests for Windows platform
* Merge the list of ignored files/symlinks into one Activity notification.
* Fix clang's variadic macro warnings
* Add libdbus-1-dev to the build dependencies
* Compare QDateTime objects more efficient
* Improve the translation of "Share via …"
* Upgrade for Qt 5.12.5 in docker-ci
* Bump Qt 5.12.5 image
* Add timestamp to Mac installer code signing
* MacOS build: Avoid the Get-Task-Allow Entitlement (Notarization)
* Build for Debian stable and oldstable
* Happy new year
* Fall back to old login flow on GS as this is not yet ready
* [stable-2.6] macOS build: Avoid the Get-Task-Allow Entitlement (Notarization)
* Fix remote wipe keychain storage (issue #1592)
* Fix copyright year in MacOSXBundleInfo.plist.in for 2019
* Fix macOS GUI (Qt 5.12)
* Windows: Workaround for storing >= 4k (4096 bit) client-cert SSL keys
* Fix Activity List: Add check to avoid first empty entry
* Fix macOS GUI (Qt 5.12) No. 2 (continuation of #1651)
* Show date and time in activity log (fixes issue #1683)
* Login Flow v2 enhancements
* Activity List: Fix crash and folder opening
* Fix issue #1237: White text on almost-white background
* Heavy refactoring: Windows workaround for >= 4k (4096 bit) client-cert SSL keys and large certs
* Fix date in ActivityWidget and remove unnecessary string conversion

version 2.6.1 (release 2019-11-04)
* Changed max GUI bandwidth limits
* Add sync date next to "Synchronized with local folder"
* Slideshow buttons
* Fix qt warning about registering a URL sheme first.
* Issue #1216: added sync-exclude entry for emacs recovery files
* Race condition in the remote size loading logic
* Review of msvc/gcc warnings -> code cleanup, prevention of implicit casts, variadic macros
* Fix double slashes
* Fixing E2E CSR transmission failure for new connections
* Fixed missing 'translatable' exclusions, added missing window titles
* Checks if exclude file is empty before creating the regular expressions.
* Add server info to menu
* Fixed grammar
* Prevent jumping of tray menu
* Don't run connection wizard when quitting the application
* Avoiding copying in range-based loops
* Add a 'Content-Length: 0' header to initial POST requests
* Remote wipe.
* Pick from upstream: Update qtmacgoodies for an OSX crash fix #6930
* fix for issue no. #1351
* Merge the list of ignored files/symlinks into one Activity notification.
* Update submodules for Qt 5.12.5 (qtmacgoodies)
* Fix duplicate items in Apps menu (a bug introduced in #1477)
* Fix #1182
* Fix remote wipe when a proxy is configured.
* Fix updater message: Download link instead of "use the system's update

version 2.6.0 Login flow v2 (release 2019-09-27)
* Reinstate Debian build in the new Drone config
* Typo
* Marking unused strings as unstranslatable
* Fixes #607
* Fixes issue #878
* Fixes issue #1187
* Displays FileIgnored activities with an info icon
* Minor text change in the link to help in the tab 'General'.
* Update Qt 5.5 compatibility patch for Xenial
* Remove Ubuntu Cosmic and add Eoan package
* Add proper CA to client side certificate connection
* Remove dependency on libgnome-keyring0 on Eoan
* Read .sync_exclude.lst in each subdirectory
* Updates ChangeLog.
* Login flow v2
* Adds SSL client cert storage to webflow + Login Flow v2
* Windows: Workaround for CredWriteW used by QtKeychain
* Integrated registry check on windows when hasDarkSystray is called.
* Logo update
* Updated .gitignore to integrate unwanted files when working with VSC …
* Full-Scaled new logo in Windows 10 start menu tile
* Qt5.5 compatiblity patch for login flow V2 + UI improvement (Use newer digest algorithms in TLS error dialog)
* Fix for #1382 "linux client crashes for no discernable reason"
* UI improvement: Message box: Delete / Keep all files
* Improve wording of the context menu in the file manager extension.
* Changes wording in the share context menu.
* Fix White Window issue on Windows by upgrading to Qt 5.12.5

2.5 Series ChangeLog
====================

version 2.5.3 (release 2019-07-22)
* Fix empty file wording in error log (small)
* Add Qt-5.12 to CI
* Fix a minor typo
* Libcloudproviders: Add missing check for Qt5DBus
* Fix several memory leaks in cloudproviders and add translation support
* Share link fixing
* New drone config
* Uses configuraion to determine if it should show empty folder popup.
* Simplify cmake command to make copy-pastable
* Updated default remote poll to 5 seconds #1115
* Fix memory leak with device pointer
* Added a nice UI for the E2E-enabled account first connect
* This should fix issue #1000
* Adds parameter to retrieve shares with its reshares.
* Fixed typo
* Fixed typo in "certificate"
* WebView: Properly handle usernames with spaces and plus signs in it
* Add error category for http file lock error status 423.
* Displays the uid_owner of a shared file.
* Minor text change in the link to help in the tab 'General'.

version 2.5.2 (release 2019-04-11)
* Handle spaces in username properly in login flow
* Wizard: show an error message if there is no enough free space in the local folder
* Removed whitespace from string
* Do not add double slash to login flow url
* Fix login flow with system proxy
* Start with easier theming
* Do not display dismissed notifications
* Fixed l18n issue. Added space for separating string
* Add invalid certiticate messagebox
* Correct app passwords link
* Be less verbose with logging
* Fix typo in translation string
* Add a command line option to launch the client in the background
* Support Ubuntu Disco Dingo
* Added missing Include
* Make sure _profile and _page are deleted in the correct order
* Fix KDEInstallDirs deprecation warnings
* Removed Stylesheet

version 2.5.1  (release 2019-01-06)
* Fixup the port in server notification URLs
* GUI: let Clang-Tidy modernize nullptr & override usage
* Improve the slide show
* Libsync: let Clang-Tidy modernize nullptr & override usage
* SettingsDialog: fix a little glitch in the account tool button size
* SettingsDialog: tweak color aware icons
* More verbose error and proper app name on configuration read error
* Fix cmake build using WITH_PROVIDERS=OFF
* Debian/Ubuntu target repository update
* Change man page names and contents for nextcloud
* Share dialog alignment
* Fixed typo
* Change link to docs for NC 15
* Do not fetch activities if they are not enabled
* Do not read system exclude list if user exclude is present
* Fix the activity loop
* Write the actual folder to the log
* Fix appname for Nautilus integration script

version 2.5.0  (release 2018-11-14)
* End to end encryption
* New Web login flow
* UI improvements: Notifications
* UI improvements: refactoring of Activities
* SyncJournal: Clear etag filter before sync
* Partial local discovery: Fix scheduling logic
* Sync hidden files by default
* Larger Windows App Icon
* Show a tray message when a folder watcher becomes unreliable #6119
* Create symlinks for the small-letter application icon file names
* In setup wizard put link to nextcloud installation
* Web view scales vertically
* Add a WebFlowCredentialsAccessManager
* Mac Application Icon
* Ensure GETFileJob notices finishing #6581
* OAuth2: Try to refresh the token even if the credentials weren't ready.
* Tray workarounds #6545
* UpdateInfo: Remove unused code
* OAuth: Remove the timeout
* TestOAuth: Don't have global static QObject
* Log: Adjust update/reconcile log verbosity
* Reconcile: When detecting a local move, keep the local mtime
* Wizard enhancement
* FolderMan::checkPathValidityForNewFolder: make sure to work when fold…
* Update: Report on readdir() errors #6610
* Use encode()/decode() with Python 3 only
* Sqlite: Update bundled version to 3.24.0
* Do not require server replies to contain an mtime
* Settings: Attempt to fix rename issue on old macOS
* Support higher resolution theme icons
* OAuth: Fix infinite loop when the refresh token is expired
* Windows: Don't ignore files with FILE_ATTRIBUTE_TEMPORARY
* Data-Fingerprint: Fix backup detection when fingerprint is empty
* Nautilus: Fix GET_MENU_ITEMS with utf8 filenames #6643
* Windows: Release handle/fd when file open fails #6699
* SettingsDialog: Show the page for the newly created account
* Updates submodule qtmacgoodies.
* Fixes #665 Adds slot for confirmShare button.
* Rename INSTALL to INSTALL.md for Preview :)
* Add cmake temporary stuff
* Inform user that configuration is not writable
* Uses QByteArray to store private key.
* Fix cmake command for linux in README too
* Build fix: remove an unused QtSvg/QSvgRenderer include
* Qtkeychain: 0.8.0 -> 0.9.1
* Setup wizard: implement an animated and interactive slide show
* Theming for general settings ui
* Make the "Add Folder Sync Connection" button act like a button
* Allow to use the login flow with a self signed certificate
* Fix warning in ShareUserGroupWidget
* Copy over config file to new location on windows
* Update to translate strings
* Migrate http auth to webflow
* Margins
* Qt 5.5 compatibility patch for Xenial
* Fix cmake build of documentation
* Use Nextcloud
* Update isntaller background for OSX
* Fix ActivityWidget palette
* SettingsDialog: disable unnecessary wrapping for the about label
* Added default scheme when server returns just a host
* Removed explicit initialization; Fixed RAND_bytes not found
* Actually open the activity view on a click for more info
* Use a format that supports alpha channels for avatars
* L10n. Added space for correct grammar.


2.4 Series ChangeLog
====================

version 2.4.1 (2017-02-xx)
* Ignore files with file names that can't be encoded for the filesystem (#6287, #5676, #5719)
* Issues: Speed up insertion and add hard upper limit (#6272)
* Notifications: Fix "Dismiss" action
* Notifications: Fix timer invocation on macOS
* Notifications: Immediately poll when account online
* Protocol: Remove entries for auto resolved conflicts (#6316)
* owncloudcmd: Set proxy before capabilities call (#6281)
* owncloudcmd: Do not do the capability call when --nonshib is passed
* Avatars: Use old location for servers <10 (#6279)
* Link shares: Change default share name (#6298)
* Nautilus integration: Work with python2 and python3
* HTTP2: Only allow with Qt 5.9.4 (#6285)
* Crash fixes

version 2.4.0 (2017-12-21)
* If you're using 2.4.0 alpha1, please upgrade as previous alphas/rcs had an issue with hidden files and renames!
* OAuth2 authentication support by opening external browser (#5668)
* Shibboleth: Change to use OAuth2 if supported (#6198)
* Sharing: Add support for multiple public link shares (#5655)
* Sharing: Add option to copy/email private links (#5023, #5627)
* Sharing: Add option "show file listing" (#5837)
* Sharing: Show warning that links are public (#5747)
* Sharing: Sharing dialog redesign: multiple share links support (#5695)
* Sharing: Make "can edit" partially checked sometimes (#5642)
* Sharing: Trigger a sync for folder affected by a change of sharing (#6098)
* Wizard: Never propose an existing folder for syncing (#5597)
* Wizard: Don't show last page anymore, go to settings directly (#5726)
* Wizard: Handle url-shortener redirects (#5954)
* Wizard: Resolve url/ redirects only if url/status.php not found (#5954)
* Wizard: Add explanation text when server error is shown (#6157)
* Wizard: Update the window size on high dpi screen (#6156)
* Wizard: Don't report confusing error message (#6116)
* Gui: Display the user server avatar (#5482)
* Gui: Use display name of user, not internal name
* Server URL: Update configuration in case of permanent redirection (#5972)
* Gui: Allow to add multiple sync folder connection of the same folder (#6032)
* Tray Menu: More detailed status messages
* Tray Menu: Shibboleth: raise the browser when clicking on the tray (#6105)
* Activity: Link errors from the account tab, allow filtering by account/folder (#5861)
* Activity: Present conflicts more prominently (#5894)
* Activity: Allow sorting the columns in issues and protocol tabs (#6093, #6086)
* Selective Sync: Open sub folder context menu (#5596)
* Selective Sync: Skip excluded folders when reading db (#5772)
* Selective Sync: Remove local files of unselected folder despite other modified files (#5783)
* Excludes: Remove .htaccess form list of excluded files (#5701)
* Excludes: Hardcode desktop.ini
* Excludes: Allow escaping "#" (#6012)
* Excludes: Use faster matching via QRegularExpression (#6063)
* Discovery: Increase the MAX_DEPTH and show deep folders as ignored (#1067)
* Discovery: General speed improvements
* Downloads: Remove empty temporary if disk space full (#5746)
* Downloads: Read Content-MD5 header for object store setups
* Checksums: Add global disable environment variable (#5017)
* Quota: PropagateUpload: Model of remote quota, avoid some uploads (#5537)
* Create favorite also in folder wizard (#455)
* Windows: Use the application icon for the Windows 8 sidebar favorite (#2446, #5690)
* macOS: Finder sidebar icon (#296)
* Overlay Icons: Consider also the "shared by me" as shared (#4788)
* Overlay Icons: Update right after sharing (#6115)
* Overlay Icons: Fix different case paths not matching (#5257)
* Overlay Icons: Detect changes in the shared flag (#6098)
* Windows Overlay Icons: Potential hang fixes
* Linux Overlay Icons: fix branded nemo and caja shell integration (#5966)
* Credentials: Fix behavior for bad password (#5989)
* Credentials: Don't create empty client cert keychain entries (#5752)
* Credentials: Namespace windows cred keys (#6125)
* Credentials: Use per-account keychain entries (#5830, #6126)
* AccountSettings: Triggering log in re-ask about previously rejected certificates (#5819)
* owncloudcmd: Added bandwidth limit parameter (#5707)
* owncloudcmd: Fix timestamps, Fix --logdebug
* AccountSettings: Sync with clean discovery on Ctrl-F6 (#5666)
* Sync: Dynamic sizing of chunks in chunked uploads for improved big file upload performance (#5852)
* Sync: Introduce overall errors that are not tied to a file (#5746)
* Sync: Better messaging for 507 Insufficient Storage (#5537)
* Sync: Create conflicts by comparing the hash of files with identical mtime/size (#5589)
* Sync: Avoid downloads by comparing the hash of files with identical mtime/size (#6153)
* Sync: Upload conflict files if OWNCLOUD_UPLOAD_CONFLICT_FILES environment variable is set (#6038)
* Sync: Blacklist: Don't let errors become warnings (#5516)
* Sync: Check etag again after active sync (#4116)
* Sync: Rename handling fixes: duplicate file ids (#6096, #6212)
* Sync: Rename handling fixes: File size must be equal
* Sync: Rename handling: Fix duplicate files on abort/resume sync (#5949)
* Sync: Add capability for invalid filename regexes (#6092)
* SyncJournalDB: Fall back to DELETE journal mode if WAL mode does not seem to work (#5723)
* SyncJournalDB: Don't crash if the db file is readonly (#6050)
* SyncJournalDB: DB close error is not fatal
* Fix at least one memory leak
* Documentation improvements
* Logging improvements (With Qt logging categories) (#5671)
* Logging filtering per account (#5672)
* Crash fixes
* Test improvements
* Small UI layout fixes
* Performance improvements
* Maintenance Mode: Detect maintenance mode (#4485)
* Maintenance Mode: Add a 1 to 5 min reconnection delay (#5872)
* HTTP: Send a unique X-Request-ID with each request (#5853)
* HTTP: Support HTTP2 when built and running with Qt 5.9.x (Official packages still on Qt 5.6.x) (#5659)
* owncloudcmd: Don't start if connection or auth fails (#5692)
* csync: Switch build from C to C++ (#6033)
* csync: Refactor a lot to use common data structures to save memory and memory copying
* csync: Switch some data structures to Qt data structures
* csync: Switch to using upper layer SyncJournalDB (#6087)
* Switch 3rdparty/json usage to Qt5's QJson (#5710)
* OpenSSL: Don't require directly, only via Qt (#5833)
* Remove iconv dependency, use Qt for file system locale encoding/decoding (emoji filename support on macOS) (#5875)
* Compilation: Remove Qt 4 code (#6025, #5702, #5505)
* Harmonize source code style with clang-format (#5732)
* Switch over to Qt 5 function pointer signal/slot syntax (#6041)
* Compile with stack-smashing protection
* Updater: Rudimentary support for beta channel (#6048)


2.3 Series ChangeLog
====================

version 2.3.4 (2017-11-02)
* Checksums: Use addData function to avoid endless loop CPU load issues with Office files
* Packaging: Require ZLIB

version 2.3.3 (2017-08-29)
* Chunking NG: Don't use old chunking on new DAV endpoint (#5855)
* Selective Sync: Skip excluded folders when reading DB, don't let them show errors (#5772)
* Settings: Make window bigger so Qt version is always visible (#5760)
* Share links: Show warning that public link shares are public (#5786)
* Downloads: Re-trigger folder discovery on HTTP 404 (#5799)
* Overlay Icons: Fix potential hangs on Windows
* SyncJournalDB: Don't use ._ as filename pattern if that does not work because of SMB storage settings (#5844)
* SyncJournalDB: Log reason for sqlite3 opening errors
* Notifications: Proapgate "Dismiss" button action to server (#5922)
* Switch Linux build also to Qt 5.6.2 (#5470)
* Stopped maintaining Qt 4 buildability

version 2.3.2 (2017-05-08)
* Fix more crashes (thanks to everyone submitting to our crash reporter!)
* Improve compatibility with server 10.0 (#5691, X-OC-Total-Size)
* Share dialog: UI improvements, Bring to front on tray click
* owncloudcmd: Align process return value with sync return value (#3936)
* Fix disk free check on Windows when opening the local DB

version 2.3.1 (2017-03-21)
* Fix several crashes (thanks to everyone submitting to our crash reporter!)
* Improve HTTP redirect handling (#5555)
* Blacklist: Escalate repeated soft error to normal error (#5500)
* NTFS: Do not attempt to upload two existing files with similar casing (#5544)
* Fix URL for linking to application password generation for ownCloud 10.0 (#5605)

version 2.3.0 (2017-03-03)
* Decreased memory usage during sync
* Overlay icons: Lower CPU usage
* Allow to not sync the server's external storages by default
* Switch Windows and OS X build to Qt 5.6.2
* Switch to new ownCloud server WebDAV endpoint
* Chunking NG: New file upload chunking algorithmn for ownCloud server 9.2
* Allow to sync a folder to multiple different servers (Filename change from .csync_journal.db to _sync_$HASH.db)
* Conflicts: Use the local mtime for the conflict file name (#5273)
* "Sync now" menu item
* SSL Client certificate support improved (Show UI, Store keys in keychain)
* Propagator: Upload more small files in parallel
* Sync Engine: Read data-fingerprint property to detect backups (#2325)
* GUI: Show link to ceate an app password/token for syncing
* Share dialog: Add 'Mail link' button
* Caja file manager plugin
* Make "backup detected" message to not trigger in wrong cases
* SyncEngine: Fix renaming of folder when file are changed (#5192)
* Fix reconnect bug if status.php intermittently returns wrong data (#5188)
* Improve sync scheduling (#5317)
* Overlay icons: Improvements in correctnes
* Tray menu: Only update on demand to fix Linux desktop integration glitches
* Progress: Better time/bandwidth estimations
* Network: Follow certain HTTP redirects (#2791)
* Network: Remove all cookies (including load balancers etc) when logging out
* Discovery thread: Low priority
* owncloudsync.log: Write during propagation
* Better error message for files with trailing spaces on Windows
* Excludes: Consider files in hidden folders excluded (#5163)
* Allow sync directory to be a symlinked directory
* Add manifest file on Windows to make the application UAC aware
* macOS: Improve monochrome tray icons
* Shibboleth bugfixes
* Fixes with regards to low disk space
* A ton of other bugfixes
* Refactorings
* Improved documentation
* Crash fixes


2.2 Series ChangeLog
====================

version 2.2.4 (release 2016-09-27)
* Dolphin Plugin: Use the Application name for the socket path (#5172)
* SyncEngine: Fix renaming of folder when file are changed (#5195)
* Selective Sync: Fix HTTP request loop and show error in view (#5154)
* ConnectionValidator: properly handle error in status.php request (#5188)
* Discovery: Set thread priority to low (#5017)
* ExcludeFiles: Fix when the folder casing is not the same in the settings and in the FS
* ShareLink: Ensure the password line edit is enabled (#5117)

version 2.2.3 (release 2016-08-08)
* SyncEngine: Fix detection of backup (#5104)
* Fix bug with overriding URL in config (#5016)
* Sharing: Fix bug with file names containing percent encodes (#5042, #5043)
* Sharing: Permissions for federated shares on servers >=9.1 (#4996, #5001)
* Overlays: Fix issues with file name casing on OS X and Windows
* Windows: Skip symlinks and junctions again (#5019)
* Only accept notification API Capability if endpoint is OCS-enabled (#5034)
* Fix windows HiDPI (#4994)
* SocketAPI: Use different pipe name to avoid unusual delay (#4977)
* Tray: Add minimal mode as workaround and testing tool for Linux issues (#4985, #4990)
* owncloudcmd: Fix --exclude regression #4979
* Small memleak: Use the full file stat destructors (#4992)
* Fix small QAction memleak (#5008)
* Fix crash on shutting down during propagation (#4979)
* Decrease memory usage during sync #4979
* Setup csync logging earlier to get all log output (#4991)
* Enable Shibboleth debug view with OWNCLOUD_SHIBBOLETH_DEBUG env

version 2.2.2 (release 2016-06-21)
* Excludes: Don't redundantly add the same exclude files (memleak) (#4967, #4988)
* Excludes: Only log if the pattern was really logged. (#4989)

version 2.2.1 (release 2016-06-06)
 * Fix out of memory error when too many uploads happen (#4611)
 * Fix display errors in progress display (#4803 #4856)
 * LockWatcher: Remember to upload files after they become unlocked (#4865)
 * Fix overlay icons for files with umlauts (#4884)
 * Certs: Re-ask for different cert after rejection (#4898, #4911)
 * Progress: Don't count items without propagation jobs (#4856, #4910)
 * Utility: Fix for the translation of minutes, second (#4855)
 * SyncEngine: invalid the blacklist entry when the rename destination change

version 2.2.0 (release 2016-05-12)
 * Overlay icons: Refactoring - mainly for performance improvements
 * Improved error handling with Sync Journal on USB storages (#4632)
 * Sharing Completion: Improved UI of completion in sharing from desktop. (#3737)
 * Show server notifications on the client (#3733)
 * Improved Speed with small files by dynamic parallel request count (#4529)
 * LockWatcher: Make sure to sync files after apps released exclusive locks on Windows.
 * Improved handling of Win32 file locks and network files
 * Workaround Ubuntu 16.04 tray icon bug (#4693)
 * Removed the Alias field from the folder definition (#4695)
 * Improved netrc parser (#4691)
 * Improved user notifications about ignored files and conflicts (#4761, #3222)
 * Add warnings for old server versions (#4523)
 * Enable tranportation checksums if the server supports based on server capabilities (#3735)

 * Default Chunk-size changed to 10MB (#4354)
 * Documentation Improvements, ie. about overlay icons
 * Translation fixes
 * Countless other bugfixes
 * Update of QtKeyChain to support Windows credential store
 * Packaging of dolphin overlay icon module for bleeding edge distros


2.1 Series ChangeLog
====================

version 2.1.1 (release 2016-02-10)
 * UI improvements for HiDPI screens, error messages, RTL languages
 * Fix occurences of "Connection Closed" when a new unauthenticated TCP socket is used
 * Fix undeliberate WiFi scanning done by Qt Network classes
 * Several fixes/improvements to the sharing dialog
 * Several fixes/improvements to the server activity tab
 * Create the directory when using --confdir and it does not exist
 * Windows Overlay icons: Fix DLL and icon oddities
 * Mac Overlay icons: Don't install legacy Finder plugin on >= 10.10
 * Linux Overlay icons: Nemo plugin
 * Overlay icons: Fix several wrong icon state computations
 * Allow changeable upload chunk size in owncloud.cfg
 * Crash fixes on account deletion
 * Forget password on explicit sign-out
 * OS X: Fix the file system watcher ignoring unicode paths (#4424)
 * Windows Installer: Update to NSIS 2.50, fixes possible DLL injection
 * Sync Engine: .lnk files
 * Sync Engine: symlinked syn directories
 * Sync Engine: Windows: Fix deleting and replacing of read-only files (#4308, #4277)
 * Sync Engine: Fixes for files becoming directories and vice versa (#4302)
 * Misc other fixes/improvements

version 2.1 (release 2015-12-03)
 * GUI: Added a display of server activities
 * GUI: Added a separate view for not synced items, ignores, errors
 * GUI: Improved upload/download progress UI (#3403, #3569)
 * Allowed sharing with ownCloud internal users and groups from Desktop
 * Changed files starting in .* to be considered hidden on all platforms (#4023)
 * Reflect read-only permissions in filesystem (#3244)
 * Blacklist: Clear on successful chunk upload (#3934)
 * Improved reconnecting after network change/disconnect (#4167 #3969 ...)
 * Improved performance in Windows file system discovery
 * Removed libneon-based propagator. As a consequence, The client can no
* longer provide bandwith limiting on Linux-distributions where it is
* using Qt < 5.4
 * Performance improvements in the logging functions
 * Ensured that local disk space problems are handled gracefully (#2939)
 * Improved handling of checksums: transport validation, db (#3735)
 * For *eml-files don't reupload if size and checksum are unchanged (#3235)
 * Ensured 403 reply code is handled properly (File Firewall) (#3490)
 * Reduced number of PROPFIND requests to server(#3964)
 * GUI: Added Account toolbox widget to keep account actions (#4139)
 * Tray Menu: Added fixes for Recent Activity menu (#4093, #3969)
 * FolderMan: Fixed infinite wait on pause (#4093)
 * Renamed env variables to include unit (#2939)
 * FolderStatusModel: Attempt to detect removed undecided files (#3612)
 * SyncEngine: Don't whipe the white list if the sync was aborted (#4018)
 * Quota: Handle special negative value for the quota (#3940)
 * State app name in update notification (#4020)
 * PropagateUpload: Fixed double-emission of finished (#3844)
 * GUI: Ensured folder names which are excluded from sync can be clicked
 * Shell Integration: Dolphin support, requires KF 5.16 and KDE Application 15.12
 * FolderStatusModel: Ensured reset also if a folder was renamed (#4011)
 * GUI: Fixed accessiblity of remaing items in full settings toolbar (#3795)
 * Introduced the term "folder sync connection" in more places (#3757)
 * AccountSettings: Don't disable pause when offline (#4010)
 * Fixed handling of hidden files (#3980)
 * Handle download errors while resuming as soft errors (#4000)
 * SocketAPI: Ensured that the command isn't trimmed (#3297)
 * Shutdown socket API before removing the db (#3824)
 * GUI: Made "Keep" default in the delete-all dialog (#3824)
 * owncloudcmd: Introduced return code 0 for --version and --help
 * owncloudcmd: Added --max-sync-retries (#4037)
 * owncloudcmd: Don't do a check that file are older than 2s (#4160)
 * Fixed getting size for selective sync (#3986)
 * Re-added close button in the settings window (#3713)
 * Added abililty to handle storage limitations gracefully (#3736)
 * Organized patches to our base Qt version into admin/qt/patches
 * Plus: A lot of unmentioned improvements and fixes


2.0 Series ChangeLog
====================

version 2.0.2 (release 2015-10-21)
  * csync_file_stat_s: Save a bit of memory
  * Shibboleth: Add our base user agent to WebKit
  * SelectiveSync: Increase folder list timeout to 60
  * Propagation: Try another sync on 423 Locked (#3387)
  * Propagation: Make 423 Locked a soft error (#3387)
  * Propagation: Reset upload blacklist if a chunk succeeds
  * Application: Fix crash on early shutdown (#3898)
  * Linux: Don't show settings dialog always when launched twice (#3273, #3771, #3485)
  * win32 vio: Add the OPEN_REPARSE_POINTS flag to the CreateFileW call. (#3813)
  * AccountSettings: only expand root elements on single click.
  * AccountSettings: Do not allow to expand the folder list when disconnected.
  * Use application SHORT name for the name of the MacOSX pkg file (ownBrander).
  * FolderMan: Fix for removing a syncing folder (#3843)
  * ConnectionMethodDialog: Don't be insecure on close (#3863)
  * Updater: Ensure folders are not removed (#3747)
  * Folder settings: Ensure path is cleaned (#3811)
  * Propagator: Simplify sub job finished counting (#3844)
  * Share dialog: Hide settings dialog before showing (#3783)
  * UI: Only expand 1 level in folder list (#3585)
  * UI: Allow folder expanding from button click (#3585)
  * UI: Expand folder treeview on single click (#3585)
  * GUI: Change tray menu order (#3657)
  * GUI: Replace term "sign in" with "Log in" and friends.
  * SetupPage: Fix crash caused by uninitialized Account object.
  * Use a themable WebDAV path all over.
  * Units: Back to the "usual" mix units (JEDEC standard).
  * csync io: Full UNC path support on Win (#3748)
  * Tray: Don't use the tray workaround with the KDE theme (#3706, #3765)
  * ShareDialog: Fix folder display (#3659)
  * AccountSettings: Restore from legacy only once (#3565)
  * SSL Certificate Error Dialog: show account name (#3729)
  * Tray notification: Don't show a message about modified folder (#3613)
  * PropagateLocalRemove:  remove entries from the DB even if there was an error.
  * Settings UI improvements (eg. #3713, #3721, #3619 and others)
  * Folder: Do not create the sync folder if it does not exist (#3692)
  * Shell integration: don't show share menu item for top level folders
  * Tray: Hide while modifying menus (#3656, #3672)
  * AddFolder: Improve remote path selection error handling (#3573)
  * csync_update: Use excluded_traversal() to improve performance (#3638)
  * csync_excluded: Add fast _traversal() function (#3638)
  * csync_exclude: Speed up significantly (#3638)
  * AccountSettings: Adjust quota info design (#3644, #3651)
  * Adjust buttons on remove folder/account questions (#3654)

version 2.0.1 (release 2015-09-01)
  * AccountWizard: fix when the theme specify a override URL (#3699)

version 2.0.0 (release 2015-08-25)
  * Add support for multiple accounts (#3084)
  * Do not sync down new big folders from server without users consent (#3148)
  * Integrate Selective Sync into the default UI
  * OS X: Support native finder integration for 10.10 Yosemite (#2340)
  * Fix situation where client would not reconnect after timeout (#2321)
  * Use SI units for the file sizes
  * Improve progress reporting during sync (better estimations, show all files, show all bandwidth)
  * Windows: Support paths >255 characters (#57) by using Windows API instead of POSIX API
  * Windows, OS X: Allow to not sync hidden files (#2086)
  * OS X: Show file name in UI if file has invalid UTF-8 in file name
  * Sharing: Make use of Capability API (#3439)
  * Sharing: Do not allow sharing the root folder (#3495)
  * Sharing: Show thumbnail
  * Client Updater: Check for updates periodically, not only once per run (#3044)
  * Windows: Remove misleading option to remove sync data (#3461)
  * Windows: Do not provoke AD account locking if password changes (#2186)
  * Windows: Fix installer when installing unprivileged (#2616, #2568)
  * Quota: Only refresh from server when UI is shown
  * SSL Button: Show more information
  * owncloudcmd: Fix --httpproxy (#3465)
  * System proxy: Ask user for credentials if needed
  * Several fixes and performance improvements in the sync engine
  * Network: Try to use SSL session tickets/identifiers. Check the SSL button to see if they are used.
  * Bandwidth Throttling: Provide automatic limit setting for downloads (#3084)
  * Systray: Workaround for issue with Qt 5.5.0 #3656


1.8 Series ChangeLog
====================

version 1.8.4 (release 2015-07-13)
  * Release to ship a security release of openSSL. No source changes of the ownCloud Client code.

version 1.8.3 (release 2015-06-23)
  * Fix a bug in the Windows Installer that could crash explorer (#3320)
  * Reduce 'Connection closed' errors (#3318, #3313, #3298)
  * Ignores: Force a remote discovery after ignore list change (#3172)
  * Shibboleth: Avoid crash by letting the webview use its own QNAM (#3359)
  * System Ignores: Removed *.tmp from system ignore again. If a user
*  wants to ignore *.tmp, it needs to be added to the user ignore list.

version 1.8.2 (release 2015-06-08)
 * Improve reporting of server error messages (#3220)
 * Discovery: Ignore folders with any 503 (#3113)
 * Wizard: Show server error message if possible (#3220)
 * QNAM: Fix handling of mitm cert changes (#3283)
 * Win32: Installer translations added (#3277)
 * Win32: Allow concurrent OEM (un-)installers (#3272)
 * Win32: Make Setup/Update Mutex theme-unique (#3272)
 * HTTP: Add the branding name to the UserAgent string
 * ConnectonValidator: Always run with new credentials (#3266)
 * Recall Feature: Admins can trigger an upload of a file from
* client to server again (#3246)
 * Propagator: Add 'Content-Length: 0' header to MKCOL request (#3256)
 * Switch on checksum verification through branding or config
 * Add ability for checksum verification of up and download
 * Fix opening external links for some labels (#3135)
 * AccountState: Run only a single validator, allow error message
* overriding (#3236, #3153)
 * SyncJournalDB: Minor fixes and simplificatons
 * SyncEngine: Force re-read of folder Etags for upgrades from
* 1.8.0 and 1.8.1
 * Propagator: Limit length of temporary file name (#2789)
 * ShareDialog: Password ui fixes (#3189)
 * Fix startup hang by removing QSettings lock file (#3175)
 * Wizard: Allow SSL cert dialog to show twice (#3168)
 * ProtocolWidget: Fix rename message (#3210)
 * Discovery: Test better, treat invalid hrefs as error (#3176)
 * Propagator: Overwrite local data only if unchanged (#3156)
 * ShareDialog: Improve error reporting for share API fails
 * OSX Updater: Only allow updates only if in /Applications (#2931)
 * Wizard: Fix lock icon (#1447)
 * Fix compilation with GCC 5
 * Treat any 503 error as temporary (#3113)
 * Work around for the Qt PUT corruption bug (#2425)
 * OSX Shell integration: Optimizations
 * Windows Shell integration: Optimizations
 .. more than 250 commits since 1.8.1

version 1.8.1 (release 2015-05-07)
 * Make "operation canceled" error a soft error
 * Do not throw an error for files that are scheduled to be removed,
* but can not be found on the server. #2919
 * Windows: Reset QNAM to proper function after hibernation. #2899 #2895 #2973
 * Fix argument verification of --confdir #2453
 * Fix a crash when accessing a dangling UploadDevice pointer #2984
 * Add-folder wizard: Make sure there is a scrollbar if folder names
* are too long #2962
 * Add-folder Wizard: Select the newly created folder
 * Activity: Correctly restore column sizes #3005
 * SSL Button: do not crash on empty certificate chain
 * SSL Button: Make menu creation lazy #3007 #2990
 * Lookup system proxy async to avoid hangs #2993 #2802
 * ShareDialog: Some GUI refinements
 * ShareDialog: On creation of a share always retrieve the share
* This makes sure that if a default expiration date is set this is reflected
* in the dialog. #2889
 * ShareDialog: Only show share dialog if we are connected.
 * HttpCreds: Fill pw dialog with previous password. #2848 #2879
 * HttpCreds: Delete password from old location. #2186
 * Do not store Session Cookies in the client cookie storage
 * CookieJar: Don't accidentally overwrite cookies. #2808
 * ProtocolWidget: Always add seconds to the DateTime locale. #2535
 * Updater: Give context as to which app is about to be updated #3040
 * Windows: Add version information for owncloud.exe. This should help us know
* what version or build number a crash report was generated with.
 * Fix a crash on shutdown in ~SocketApi #3057
 * SyncEngine: Show more timing measurements #3064
 * Discovery: Add warning if returned etag is 0
 * Fix a crash caused by an invalid DiscoveryDirectoryResult::iterator #3051
 * Sync: Fix sync of deletions during 503. #2894
 * Handle redirect of auth request. #3082
 * Discovery: Fix parsing of broken XML replies, which fixes local file disappearing #3102
 * Migration: Silently restore files that were deleted locally by bug #3102
 * Sort folder sizes SelectiveSyncTreeView numerically #3112
 * Sync: PropagateDownload: Read the mtime from the file system after writing it #3103
 * Sync: Propagate download: Fix restoring files for which the conflict file exists #3106
 * Use identical User Agents and version for csync and the Qt parts
 * Prevent another crash in ~SocketApi #3118
 * Windows: Fix rename of finished file. #3073
 * AccountWizard: Fix auth error handling. #3155
 * Documentation fixes
 * Infrastructure/build fixes
 * Win32/OS X: Apply patch from OpenSSL to handle oudated intermediates gracefully #3087

version 1.8.0 (release 2015-03-17)
 * Mac OS: HIDPI support
 * Support Sharing from desktop: Added a share dialog that can be
* opened by context menu in the file managers (Win, Mac, Nautilus)
* Supports public links with password enforcement
 * Enhanced usage of parallel HTTP requests for ownCloud 8 servers
 * Renamed github repository from mirall to client.
 * Mac OS: Use native notification support
 * Selective Sync: allow to enforce selective sync in brandings.
 * Added ability to build on Windows utilizing MingGW
 * SQLite database fixes if running on FAT filesystems
 * Improved detection of changing files to upload from local
 * Preparations for the multi-account feature
 * Fixed experience for Window manager without system tray
 * Build with Qt 5.4
 * Dropped libneon dependency if Qt 5.4 is available
 * Keep files open very short, that avoid lock problems on Windows
* especially with office software but also others.
 * Merged some NetBSD patches
 * Selective sync support for owncloudcmd
 * Reorganize the source repository
 * Prepared direct download
 * Added Crashreporter feature to be switched on on demand
 * A huge amount of bug fixes in all areas of the client.
 * almost 700 commits since 1.7.1


1.7 Series ChangeLog
====================

version 1.7.1 (release 2014-12-18)
 * Documentation fixes and updates
 * Nautilus Python plugin fixed for Python 3
 * GUI wording fixes plus improved log messages
 * Fix hidning of the database files in the sync directories
 * Compare http download size with the header value to avoid broken
* downloads, bug #2528
 * Avoid initial ETag fetch job at startup, which is not needed.
 * Add chunk size http header to PUT requests
 * Fixed deteteCookie method of our CookieJar, fix for Shibboleth
 * Added fallback for distros where XDG_RUNTIME_DIR is undefined
 * Fix the setup wizard, bug #1989, #2264
 * Fix scheduling of ETag check jobs, bug #2553
 * Fix to avoid syncing more than one folder at a time, bug #2407
 * Use fife minutes timeout for all network jobs
 * Cleanup for Folderwizard wording
 * Improve journal check: Remove corrupted journal files, bug #2547
 * Fix item count in progress dialog for deletes, bug #1132
 * Display correct file count on deletion (#1132)
 * Fix reinitializing the folder using the wizard in certain cases (#2606)
 * Mac OS: Fixed branding of the pkg file
 * Mac OS: Fix display of overlay icons in certain situations (#1132)
 * Mac OS: Use a bundled version of OpenSSL (#764, #2600, #2510)
 * Win32: improved filesystem watcher
 * Win32: Improve threading with shell integration
 * Win32: Upgraded to OpenSSL 1.0.1j
 * Win32: Improve reliability of Installer, fix removal of Shell Extensions

version 1.7.0 (release 2014-11-07)
 * oC7 Sharing: Handle new sharing options of ownCloud 7 correctly.
 * Added Selective sync: Ability to unselect server folders which are
* excluded from syncing, plus GUI and setup GUI
 * Added overlay icons for Windows Explorer, Mac OS Finder and GNOME Nautilus.
* Information is provided by the client via a local socket / named pipe API
* which provides information about the sync status of files.
 * Improved local change detection: consider file size, detect files
* with ongoing changes and do not upload immediately
 * Improved HTTP request timeout handler: all successful requests reset
* the timeout counter
 * Improvements for syncing command line tool: netrc support, improved
* SSL support, non interactive mode
 * Permission system: ownCloud 7 delivers file and folder permissions,
* added ability to deal with it for shared folders and more.
 * Ignore handling: Do not recurse into ignored or excluded directories
 * Major sync journal database improvements for more stability and performance
 * New library interface to sqlite3
 * Improve "resync handling" if errors occur
 * Blacklist improvements
 * Improved logging: more useful meta info, removed noise
 * Updated to latest Qt5 versions on Windows and OS X
 * Fixed data loss when renaming a download temporary fails and there was
* a conflict at the same time.
 * Fixed missing warnings about reusing a sync folder when the back button
* was used in the advanced folder setup wizard.
 * The 'Retry Sync' button now also restarts all downloads.
 * Clean up temporary downloads and some extra database files when wiping a
* folder.
 * OS X: Sparkle update to provide pkg format properly
 * OS X: Change distribution format from dmg to pkg with new installer.
 * Windows: Fix handling of filenames with trailing dot or space
 * Windows: Don't use the wrong way to get file mtimes in the legacy propagator.



1.6 Series ChangeLog
====================

version 1.6.4 (release 2014-10-22)
 * Fix startup logic, fixes bug #1989
 * Fix raise dialog on X11
 * Win32: fix overflow when computing the size of file > 4GiB
 * Use a fixed function to get files modification time, the
* original one was broken for certain timezone issues, see
* core bug #9781 for details
 * Added some missing copyright headers
 * Avoid data corruption due to wrong error handling, bug #2280
 * Do improved request timeout handling to reduce the number of
* timed out jobs, bug #2155
* version 1.6.3 (release 2014-09-03)
 * Fixed updater on OS X
 * Fixed memory leak in SSL button that could lead to quick memory draining
 * Fixed upload problem with files >4 GB
 * MacOSX, Linux: Bring Settings window to front properly
 * Branded clients: If no configuration is detected, try to import the data
*  from a previously configured community edition.

version 1.6.2 (release 2014-07-28 )
 * Limit the HTTP buffer size when downloading to limit memory consumption.
 * Another small mem leak fixed in HTTP Credentials.
 * Fix local file name clash detection for MacOSX.
 * Limit maximum wait time to ten seconds in network limiting.
 * Fix data corruption while trying to resume and the server does
* not support it.
 * HTTP Credentials: Read password from legacy place if not found.
 * Shibboleth: Fix the waiting curser that would not disapear (#1915)
 * Limit memory usage to avoid mem wasting and crashes
 * Propagator: Fix crash when logging out during upload (#1957)
 * Propagator_qnam: Fix signal slot connection (#1963)
 * Use more elaborated way to detect that the server was reconfigured (#1948)
 * Setup Wizard: Reconfigure Server also if local path was changed (#1948)

version 1.6.1 (release 2014-06-26 )
 * Fix 'precondition failed' bug with broken upload
 * Fix openSSL problems for windows deployment
 * Fix syncing a folder with '#' in the name
 * Fix #1845: do not update parent directory etag before sub
* directories are removed
 * Fix reappearing directories if dirs are removed during its
* upload
 * Fix app version in settings dialog, General tab
 * Fix crash in FolderWizard when going offline
 * Shibboleth fixes
 * More specific error messages (file remove during upload, open
* local sync file)
 * Use QSet rather than QHash in SyncEngine (save memory)
 * Fix some memory leaks
 * Fix some thread race problems, ie. wait for neon thread to finish
* before the propagator is shut down
 * Fix a lot of issues and warnings found by Coverity
 * Fix Mac some settings dialog problems


version 1.6.0 (release 2014-05-30 )
 * Minor GUI improvements
 * Qt5 compile issues fixed
 * Ignore sync log file in filewatcher
 * Install libocsync to private library dir and use rpath to localize
 * Fix reconnect after server disconnect
 * Fix "unknown action" display in Activity window
 * Fix memory leaks
 * Respect XDG_CONFIG_HOME environment var
 * Handle empty fileids in the journal correctly
 * Add abilility to compile libowncloudsync without GUI dependendy
 * Fix SSL error with previously-expired CAs on Windows
 * Fix incorrect folder pause state after start
 * Fix a couple of actual potential crashes
 * Improve Cookie support (e.g. for cookie-based load-balancers)
 * Introduce a general timeout of 300s for network operations
 * Improve error handling, blacklisting
 * Job-based change propagation, enables faster parallel up/downloads
* (right now only if no bandwidth limit is set and no proxy is used)
 * Significantly reduced CPU load when checking for local and remote changes
 * Speed up file stat code on Windows
 * Enforce Qt5 for Windows and Mac OS X builds
 * Improved owncloudcmd: SSL support, documentation
 * Added advanced logging of operations (file .???.log in sync
* directory)
 * Avoid creating a temporary copy of the sync database (.ctmp)
 * Enable support for TLS 1.2 negotiation on platforms that use
* Qt 5.2 or later
 * Forward server exception messages to client error messages
 * Mac OS X: Support Notification Center in OS X 10.8+
 * Mac OS X: Use native settings dialog
 * Mac OS X: Fix UI inconsistencies on Mavericks
 * Shibboleth: Warn if authenticating with a different user
 * Remove vio abstraction in csync
 * Avoid data loss when a client file system is not case sensitive


1.5 Series ChangeLog
====================

version 1.5.3 (release 2014-03-10 )
  * Fix usage of proxies after first sync run (#1502, #1524, #1459, #1521)
  * Do not wipe the credentials from config for reconnect (#1499, #1503)
  * Do not erase the full account config if an old version of the client stored
*  the password (related to above)
  * Fix layout of the network tab (fixes #1491)
  * Handle authentication requests by a Shibboleth IdP
  * Shibboleth: If no connection is available, don't open the login window
  * [Packaging] Debian/Ubuntu: ship sync-exclude.lst
  * [Packaging] Fix issues with access to gnome keychain in Fedora and RHEL6
  * [Packaging] Ensure all sub packages get updated
  * [Packaging] Fix incorrect path in desktop file (RHEL6/CentOS6)

version 1.5.2 (release 2014-02-26 )
  * Fix behavior when attempting to rename Shared folder
  * Fix potential endless sync loops on Mac OS (#1463)
  * Fix potential crash when pausing during update phase (#1442)
  * Fix handing of shared directories
  * Fix online state handling (#1441, #1459)
  * Fix potential crash in c_iconv on Mac OS
  * Fix certificate chain display in SSLButton
  * Fix sporadicly appearing multiple auth prompts on sign-in
  * Show correct state icon in Account Settings right away
  * Re-fetch content that gets deleted from read only shared directories
  * Do not store the password in the config file, erase existing ones (#1469)
  * Shibboleth: Close browser window after login
  * Shibboleth: Proper invalidation if timeout during sync
  * Shibboleth: Do not pop up IdP login immediately when modifying account
  * Shibboleth: Avoid auth on restart by storing cookies in the wallet
  * Fix license headers

version 1.5.1 (release 2014-02-13 )
  * Added an auto updater that updates the client if a
*  more recent version was found automatically (Windows, Mac OS X)
  * Added a button to the account dialog that gives information
*  about the encryption layer used for communication, plus a
*  certificate information widget
  * Preserve the permission settings of local files rather than
*  setting them to a default (Bug #820)
  * Handle windows lnk files correctly (Bug #1307)
  * Detect removes and renames in read only shares and
*  restore the gone away files. (Bug #1386)
  * Fixes sign in/sign out and password dialog. (Bug #1353)
  * Fixed error messages (Bug #1394)
  * Lots of fixes for building with Qt5
  * Changes to network limits are now also applied during a
*  sync run
  * Fixed mem leak after via valgrind on Mac
  * Imported the ocsync library into miralls repository.
*  Adopted all build systems and packaging to that.
  * Introduce a new linux packaging scheme following the
*  debian upstream scheme
  * Use a refactored Linux file system watcher based on
*  inotify, incl. unit tests
  * Wizard: Gracefully fall back to HTTP if HTTPS connection
*  fails, issuing a warning
  * Fixed translation misses in the propagator
  * Fixes in proxy configuration
  * Fixes in sync journal handling
  * Fix the upload progress if the local source is still
*  changing when the upload begins.
  * Add proxy support to owncloud commandline client
  * NSIS fixes
  * A lot of other fixes and minor improvements
  * Improve Qt5 compatability

version 1.5.0 (release 2013-12-12 ), csync 0.91.4 required
  * New owncloud propagator that skips the vio abstraction layer
  * Add owncloudcmd to replace the ocsync command line tool
  * Localize Windows installer
  * Allow to sign in and out
  * Ask for password if missing
  * Introduce activity view
  * Introduce black list for files which could not be synced
  * Enabling accessbility by shipping accessibility enables on OS X (#736)
  * Toggle Settings window when clicking on systray icon on Win and KDE (#896)
  * FolderWizard: Sanitize error detection (#1201)
  * Set proper enable state of blacklist button after the dialog was opened
  * Set proper tooltips in blacklist
  * Translatable error messages for file errors
  * Add man page for owncloudcmd (#1234)
  * Don't close setup wizard when the initial sync run is started
  * Close the sync journal if a folder gets removed (#1252)
  * Activity: Avoid horizontal scrollbar (#1213)
  * Fix crash (#1229)
  * Resize wizard appropriately (#1130)
  * Fix account identity test (#1231)
  * Maintain the file type correctly
  * Display rename-target in sync protocol action column
  * Let recursive removal also remove the top dir
  * If item is a directory, remove its contents from the database as well (#1257)
  * Install headers for owncloudsync library
  * Fix opening the explorer with a selected file in Windows (#1249)
  * Add build number into versioning scheme
  * Windows: Fix rename of temporary files
  * Windows: Fix move file operation


1.4 Series ChangeLog
====================

version 1.4.2 (release 2013-10-18 ), csync 0.90.4 required
  * Do not show the warning icon in the tray (#944)
  * Fix manual proxy support when switching (#1016)
  * Add folder column to detailed sync protocol (#1037)
  * Fix possible endless loop in inotify (#1041)
  * Do not elide the progress text (#1049)
  * Fix high CPU load (#1073)
  * Reconnect if network is unavailable after startup (#1080)
  * Ensure paused folder stays paused when syncing with more than one folder (#1083)
  * Don't show desktop notification when the user doesn't want to (#1093)
  * System tray: Avoid quick flickering up of the ok-icon for the sync prepare state
  * Progress: Do not show progress if nothing is transmitted
  * Progress: Show number of deletes.

version 1.4.1 (release 2013-09-24 ), csync 0.90.1 required
  * Translation and documentation fixes.
  * Fixed error display in settings/status dialog, displays multi
*  line error messages now correctly.
  * Wait up to 30 secs before complaining about missing systray
*  Fixes bug #949
  * Fixed utf8 issues with basic auth authentication, fixes bug #941
  * Fixed remote folder selector, avoid recursive syncing, fixes bug #962
  * Handle and display network problems at startup correctly.
  * Enable and disable the folder watcher during syncs correctly.
  * Fix setting of thread priority.
  * Fixed file size display.
  * Fixed various folder wizard issues, bug #992
  * Made "Sync started" message optional, fixes bug #934
  * Fixed shutdown, avoid crashed config on win32, fixes bug #945
  * Pop up config wizard if no server url is configured, fixes bug #1018
  * Settings: calculate sidebar width dynamically, fixes bug #1020
  * Fixed a crash if sync folders were removed, fixes bug #713
  * Do proper resync after network disconnect, fixes bug #1007
  * Various minor code fixes

version 1.4.0 (release 2013-09-04 ), csync 0.90.0 required
  * New Scheduler: Only sync when there are actual changes in the server
  * Add a Settings Dialog, move Proxy Settings there
  * Transform folder Status Dialog into Account Settings, provide feedback via context menu
  * Add Bandwidth Control
  * Add a visual storage/quota indicator (context menu and account settings)
  * Add progress indication (context menu and account settings)
  * Introduce a sync history, persisting results across syncs
  * Move ability to switch to mono icons from a switch to a Settings option
  * Add "Launch on System Startup" GUI option
  * Add "Show Desktop Nofications"GUI option (enabled by default)
*  top optionally disable sync notifications
  * Add Help item, pointing to online reference
  * Implement graphical selection of remote folders in FolderWizard
  * Allow custom ignore patterns
  * Add an editor for ingore patterns
  * ALlow to flag certain ignore patterns as discardable
  * Ensure to ship with all valid translations
  * Progress Dialog now preserves the last syncned items across sync runs
  * Split Setup Wizard into multiple pages again
  * Implement "--logfile -" to log to stdout
  * Add preliminary support for Shibboleth authentication
  * Linux: Provide more icon sizes
  * Linux: Do not trigger notifier on ignored files
  * Windows: Reduce priority of CSync thread
  * Documentation: Prem. updates to reflect UI changes
  * Significant code refactorings
  * Require Qt 4.7
  * Known issue: Under certain conditions, a file will only get uploaded after up to five minutes


1.3 Series ChangeLog
====================

version 1.3.0 (release 2013-06-25 ), csync 0.80.0 required
  * Default proxy port to 8080
  * Don't lose proxy settings when changing passwords
  * Support SOCKS5 proxy (useful in combination with ssh* *D)
  * Propagate proxy changes to csync at runtime
  * Improve proxy wizard
  * Display proxy errors
  * Solved problems with lock files
  * Warn if for some reason all files are scheduled for removal on either side
  * Avoid infinite loop if authentication fails in certain cases
  * Fix reading the password from the config in certain cases
  * Do not crash when configured sync target disappears
  * Make --help work on windows
  * Make sync feedback less ambiguous.
  * Fix icon tray tooltip sometimes showing repeated content
  * More use of native directory separators on Windows
  * Remove journal when reusing a directory that used to have a journal before
  * Visual clean up of status dialog items
  * Wizard: When changing the URL or user name, allow the user to push his data
*  to the new location or wipe the folder and start from scratch
  * Wizard: Make setting a custom folder as a sync target work again
  * Fix application icon
  * User-Agent now contains "Mozilla/5.0" and the Platform name (for firewall/proxy compat)
  * Server side directory moves will be detected
  * New setup wizard, defaulting to root syncing (only for new setups)
  * Improved thread stop/termination


1.2 Series ChangeLog
====================

version 1.2.5 (release 2013-04-23 ), csync 0.70.7 required
  * [Fixes] NSIS installer fixes
  * [Fixes] Fix crash race by making certificateChain() thread safe
  * [Fixes] Build with older CMake versions (CentOS/RHEL 6)
  * [Fixes] Wording in GUI
  * [Fixes] Silently ignore "installed = true" status.php
  * Set log verbosity before calling csync_init.
  * GUI feedback for the statistics copy action
  * Safer approach for detecting duplicate sync runs

version 1.2.4 (release 2013-04-11 ), csync 0.70.6 required
  * [Fixes] Clarify string in folder wizard
  * [Fixes] Fixed some valgrind warnings
  * [Fixes] Ensure that only one sync thread can ever run
  * [Fixes] Fix default config storage path
  * [Fixes] Skip folders with no absolute path
  * [Fixes] Allow setting the configuration directory on command line

version 1.2.3 (release 2013-04-02 ), csync 0.70.5 required
  * [Fixes] Unbreak self-signed certificate handling

version 1.2.2 (release 2013-04-02 ), csync 0.70.5 required
  * [Fixes] Do not crash when local file tree contains symlinks
  * [Fixes] Correctly handle locked files on Windows
  * [Fixes] Display errors in all members of the SSL chain
  * [Fixes] Enable Accessibility features on Windows
  * [Fixes] Make setupFavLink work properly on Mac OS
  * [Fixes] Ignore temporary files created by MS Office
  * [Gui] Support Nautilus in setupFavLink

version 1.2.1 (release 2013-02-26 ), csync 0.70.4 required
  * [Fixes] Leave configured folders on configuration changes.
  * [Fixes] Do not allow to finish the setup dialog if connection can't be established.
  * [Fixes] Better handling of credentials in setup dialog.
  * [Fixes] Do not leak fd's to /dev/null when using gnutls
  * [Fixes] Stop sync scheduling when configuration wizard starts.
  * [Fixes] Clear pending network requests when stepping back in config wizard.
  * [Fixes] User password dialog asynchronous issues.
  * [Fixes] Make folderman starting and stoping the scheduling.
  * [Fixes] Various minor fixes and cleanups.
  * [Fixes] Crash on pausing sync
  * [Fixes] Stale lock file after pausing sync
  * [App] Load translations from app dir or bundle as well.
  * [Platform] Build fixes and simplifications, ie. build only one lib.
  * [Platform] Added some getter/setters for configuration values.
  * [Platform] Added man pages.
  * [Platform] Simplified/fixed credential store usage and custom configs.
  * [Platform] Added soname version to libowncloudsync.
  * [Platform] Pull in Qt translations
  * [Gui]  Make sync result popups less annoyingq
  * [Gui] Fix for result popup

version 1.2.0 (release 2013-01-24 ), csync 0.70.2 required
  * [GUI] New status dialog to show a detailed list of synced files.
  * [GUI] New tray notifications about synced files.
  * [GUI] New platform specific icon set.
  * [App] Using cross platform QtKeychain library to store credentials crypted.
  * [App] Use cross platform notification for changes in the local file system rather than regular poll.
  * [Fixes] Improved SSL Certificate handling and SSL fixes troughout syncing.
  * [Fixes] Fixed proxy authentication.
  * [Fixes] Allow brackets in folder name alias.
  * [Fixes] Lots of other minor fixes.
  * [Platform] cmake fixes.
  * [Platform] Improved, more detailed error reporting.


1.1 Series ChangeLog
====================

version 1.1.4 (release 2012-12-19 ), csync 0.60.4 required
  * No changes to mirall, only csync fixes.

version 1.1.3 (release 2012-11-30 ), csync 0.60.3 required
  * No changes to mirall, only csync fixes.

version 1.1.2 (release 2012-11-26 ), csync 0.60.2 required
  * [Fixes] Allow to properly cancel the password dialog.
  * [Fixes] Share folder name correctly percent encoded with old Qt
* * * * 4.6 builds ie. Debian.
  * [Fixes] If local sync dir is not existing, create it.
  * [Fixes] lots of other minor fixes.
  * [GUI] Display error messages in status dialog.
  * [GUI] GUI fixes for the connection wizard.
  * [GUI] Show username for connection in statusdialog.
  * [GUI] Show intro wizard on new connection setup.
  * [APP] Use CredentialStore to better support various credential
* * *  backends.
  * [APP] Handle missing local folder more robust: Create it if
* * *  missing instead of ignoring.
  * [APP] Simplify treewalk code.
  * [Platform] Fix Mac building

version 1.1.1 (release 2012-10-18), csync 0.60.1 required
  * [GUI]* Allow changing folder name in single folder mode
  * [GUI]* Windows: Add license to installer
  * [GUI]* owncloud --logwindow will bring up the log window
* * * * in an already running instance
  * [Fixes] Make sure SSL errors are always handled
  * [Fixes] Allow special characters in folder alias
  * [Fixes] Proper workaround for Menu bug in Ubuntu
  * [Fixes] csync: Fix improper memory cleanup which could
* * * * cause memory leaks and crashes
  * [Fixes] csync: Fix memory leak
  * [Fixes] csync: Allow single quote (') in file names
  * [Fixes] csync: Remove stray temporary files

  * [GUI]* Reworked tray context menu.
  * [GUI]* Users can now sync the server root folder.
  * [Fixes] Proxy support: now supports Proxy Auto-Configuration (PAC)
* * * * on Windows, reliability fixes across all OSes.
  * [Fixes] Url entry field in setup assistant handles http/https correctly.
  * [Fixes] Button enable state in status dialog.
  * [Fixes] Crash fixed on ending the client, tray icon related.
  * [Fixes] Crash through wrong delete operator.
  * [MacOS] behave correctly on retina displays.
  * [MacOS] fix focus policy.
  * [MacOS] Packaging improvements.
  * [MacOS] Packaging improvements.
  * [Platform] Windows: Setup closes client prior to uninstall.
  * [Platform] Windows: ownCloud gets added to autorun by default.
  * [Platform] insert correct version info from cmake.
  * [Platform] csync conf file and database were moved to the users app data
* * * * * directory, away from the .csync dir.
  ** * * Renamed exclude.lst to sync-exclude.lst and moved it to
* * * * /etc/appName()/ for more clean packaging. From the user path,
* * * * still exclude.lst is read if sync-exclude.lst is not existing.
  ** * * Placed custom.ini with customization options to /etc/appName()


1.0 Series ChangeLog
====================

version 1.0.5 (release 2012-08-14), csync 0.50.8 required
  * [Fixes] Fixed setup dialog: Really use https if checkbox is activated.

version 1.0.4 (release 2012-08-10), csync 0.50.8 required
  * [APP] ownCloud is now a single instance app, can not start twice any more.
  * [APP] Proxy support
  * [APP] Handle HTTP redirection correctly, note new url.
  * [APP] More relaxed handling of read only directories in the sync paths.
  * [APP] Started to split off a library with sync functionality, eg for KDE
  * [APP] Make ownCloud Info class a singleton, more robust.
  * [GUI] New, simplified connection wizard.
  * [GUI] Added ability for customized theming.
  * [GUI] Improved icon size handling.
  * [GUI] Removed Log Window Button, log available through command line.
  * [GUI] Proxy configuration dialog added.
  * [GUI] Added Translations to languages Slovenian, Polish, Catalan,
* * *  Portuguese (Brazil), German, Greek, Spanish, Czech, Italian, Slovak,
* * *  French, Russian, Japanese, Swedish, Portuguese (Portugal)
* * *  all with translation rate >90%.
  * [Fixes] Loading of self signed certs into Networkmanager (#oc-843)
  * [Fixes] Win32: Handle SSL dll loading correctly.
  * [Fixes] Many other small fixes and improvements.

version 1.0.3 (release 2012-06-19), csync 0.50.7 required
  * [GUI] Added a log window which catches the logging if required and
* * *  allows to save for information.
  * [CMI] Added options --help, --logfile and --logflush
  * [APP] Allow to specify sync frequency in the config file.
  * [Fixes] Do not use csync database files from a sync before.
  * [Fixes] In Connection wizard, write the final config onyl if
* * * * the user really accepted. Also remove the former database.
  * [Fixes] More user expected behaviour deletion of sync folder local
* * * * and remote.
  * [Fixes] Allow special characters in the sync directory names
  * [Fixes] Win32: Fixed directory removal with special character dirs.
  * [Fixes] MacOS: Do not flood the system log any more
  * [Fixes] MacOS: Put app translations to correct places
  * [Fixes] Win32: Fix loading of csync state db.
  * [Fixes] Improved some english grammar.
  * [Platform] Added krazy2 static code checks.

version 1.0.2 (release 2012-05-18), csync 0.50.6 required
  * [GUI] New icon set for ownCloud client
  * [GUI] No splashscreen any more (oC Bug #498)
  * [GUI] Russian translation added
  * [GUI] Added 'open ownCloud' to traymenu
  * [GUI] "Pause" and "Resume" instead of Enable/Disable
  * [Fixes] Long running syncs can be interrupted now.
  * [Fixes] Dialogs comes to front on click
  * [Fixes] Open local sync folder from tray and status for win32
  * [Fixes] Load exclude.lst correctly on MacOSX
*  + csync fixes.

version 1.0.1 (release 2012-04-18), csync 0.50.5 required
  * [Security] Support SSL Connections
  * [Security] SSL Warning dialog
  * [Security] Do not store password in clear text anymore
  * [Security] Restrict credentials to the configured host
  * [Security] Added ability to forbid local password storage.
  * [Fixes] Various fixes of the startup behaviour.
  * [Fixes] Various fixes in sync status display
  * [GUI] Various error messages for user display improved.
  * [GUI] fixed terms and Translations
  * [GUI] fixed translation loading
  * [Intern] Migrate old credentials to new format
  * [Intern] Some code refactorings, got rid of rotten QWebDav lib
  * [Intern] lots of cmake cleanups
  * [Intern] Backport to Qt Version 4.6 for compat. with older distros.
  * [Platform] MacOSX porting efforts
  * [Platform] MacOSX Bundle creation added
  * [Platform] Enabled ranslations on Windows.

