Changelog for ownCloud Desktop Client [unreleased] (UNRELEASED)
=======================================
The following sections list the changes in ownCloud Desktop Client unreleased relevant to
ownCloud admins and users.

[unreleased]: https://github.com/owncloud/client/compare/v3.0.0...master

Summary
-------

* Bugfix - Fix the display of the version string for released builds: [#10329](https://github.com/owncloud/client/pull/10329)
* Bugfix - Only clear cookies if enabled in theme, clear cookies before OAuth2: [#10338](https://github.com/owncloud/client/pull/10338)
* Bugfix - Display `Add a Space` instead of `Add Folder Sync Connection` with oCIS: [#10340](https://github.com/owncloud/client/pull/10340)
* Bugfix - Mac: Don't inherit the environment of the installer after an update: [#10346](https://github.com/owncloud/client/issues/10346)
* Bugfix - Correctly detect timeouts during token refrshs: [#10373](https://github.com/owncloud/client/pull/10373)
* Bugfix - Open in web if debug logs are turned off: [#10387](https://github.com/owncloud/client/pull/10387)
* Bugfix - We fixed building the client on non linux unix systems: [#10398](https://github.com/owncloud/client/issues/10398)
* Enhancement - Add a `Reconnect` option to the account menu, when dissconnected: [#10294](https://github.com/owncloud/client/issues/10294)
* Enhancement - (Re)introduce "sync hidden files" parameter in owncloudcmd: [#10390](https://github.com/owncloud/client/issues/10390)

Details
-------

* Bugfix - Fix the display of the version string for released builds: [#10329](https://github.com/owncloud/client/pull/10329)

   We removed a trailing `-` in the version string of released clients.

   https://github.com/owncloud/client/pull/10329

* Bugfix - Only clear cookies if enabled in theme, clear cookies before OAuth2: [#10338](https://github.com/owncloud/client/pull/10338)

   We fixed a bug that enabled the explicit cookie clearing required for F5 BIG-IP setups
   unconditionally. We fixed a bug where the cookie clearing was not performed during OAuth2.

   https://github.com/owncloud/client/pull/10338

* Bugfix - Display `Add a Space` instead of `Add Folder Sync Connection` with oCIS: [#10340](https://github.com/owncloud/client/pull/10340)

   We fixed a bug where the wrong text was displayed on the "add" button.

   https://github.com/owncloud/client/pull/10340

* Bugfix - Mac: Don't inherit the environment of the installer after an update: [#10346](https://github.com/owncloud/client/issues/10346)

   https://github.com/owncloud/client/issues/10346

* Bugfix - Correctly detect timeouts during token refrshs: [#10373](https://github.com/owncloud/client/pull/10373)

   https://github.com/owncloud/client/pull/10373

* Bugfix - Open in web if debug logs are turned off: [#10387](https://github.com/owncloud/client/pull/10387)

   Due to a bug opening the browser only worked if logging was enabled.

   https://github.com/owncloud/client/pull/10387

* Bugfix - We fixed building the client on non linux unix systems: [#10398](https://github.com/owncloud/client/issues/10398)

   https://github.com/owncloud/client/issues/10398

* Enhancement - Add a `Reconnect` option to the account menu, when dissconnected: [#10294](https://github.com/owncloud/client/issues/10294)

   We added a `Reconnect` button to the account menu, this allows to trigger a manual reconnect
   try. Note: The client would try to reconnect by itself at some point.

   https://github.com/owncloud/client/issues/10294

* Enhancement - (Re)introduce "sync hidden files" parameter in owncloudcmd: [#10390](https://github.com/owncloud/client/issues/10390)

   There used to be an option to enable the synchronization of hidden files using the -h parameter
   which collided with the --help option and subsequently was removed. A new
   --sync-hidden-files parameter was introduced to fill in the missing feature.

   https://github.com/owncloud/client/issues/10390

Changelog for ownCloud Desktop Client [3.0.0] (2022-11-30)
=======================================
The following sections list the changes in ownCloud Desktop Client 3.0.0 relevant to
ownCloud admins and users.

[3.0.0]: https://github.com/owncloud/client/compare/v2.11.1...v3.0.0

Summary
-------

* Bugfix - Don't unset implicit log flush: [#9515](https://github.com/owncloud/client/pull/9515)
* Bugfix - We fixed a crash: [#10017](https://github.com/owncloud/client/pull/10017)
* Bugfix - Sync status changes are now directly displayed: [#10101](https://github.com/owncloud/client/issues/10101)
* Bugfix - Windows VFS fixed some failing downloads: [#49](https://github.com/owncloud/client-desktop-vfs-win/pull/49)
* Bugfix - Don't trigger ignore list when files are locked on the server: [#5382](https://github.com/owncloud/enterprise/issues/5382)
* Bugfix - Properly resume upload with a partial local discovery: [#5382](https://github.com/owncloud/enterprise/issues/5382)
* Bugfix - Add request time and other missing data to .owncloudsync.log: [#7348](https://github.com/owncloud/client/issues/7348)
* Bugfix - Don't display a conext menu on the root folder: [#8595](https://github.com/owncloud/client/issues/8595)
* Bugfix - Fix copy url location for private links: [#9048](https://github.com/owncloud/client/issues/9048)
* Bugfix - Fix status of files uploaded with TUS: [#9472](https://github.com/owncloud/client/pull/9472)
* Bugfix - The condition for the read only files menu was inverted: [#9574](https://github.com/owncloud/client/issues/9574)
* Bugfix - Deadlock in folder context menu in a folder selection dialog: [#9681](https://github.com/owncloud/client/issues/9681)
* Bugfix - Fix never ending sync: [#9725](https://github.com/owncloud/client/issues/9725)
* Bugfix - Fix adding bookmarks on Gtk+ 3 based desktops: [#9752](https://github.com/owncloud/client/pull/9752)
* Bugfix - Stop the activity spinner when the request failed: [#9798](https://github.com/owncloud/client/issues/9798)
* Bugfix - Changes during upload of a file could still trigger the ignore list: [#9924](https://github.com/owncloud/client/issues/9924)
* Change - Windows: Update the folder icon on every start: [#10184](https://github.com/owncloud/client/issues/10184)
* Change - Don't guess remote folder in owncloudcmd: [#10193](https://github.com/owncloud/client/issues/10193)
* Change - When connected to oCIS, open the browser instead of the sharing dialog: [#10206](https://github.com/owncloud/client/issues/10206)
* Change - Owncloudcmd OCIS support: [#10239](https://github.com/owncloud/client/pull/10239)
* Change - Make sharedialog preview be more resilient: [#8938](https://github.com/owncloud/client/issues/8938)
* Change - We no longer persist cookies: [#9495](https://github.com/owncloud/client/issues/9495)
* Change - We removed support for ownCloud servers < 10.0: [#9578](https://github.com/owncloud/client/issues/9578)
* Change - Drop socket upload job: [#9585](https://github.com/owncloud/client/issues/9585)
* Change - Remove support for Windows 7 sidebar links: [#9618](https://github.com/owncloud/client/pull/9618)
* Change - Rewrote TLS error handling: [#9655](https://github.com/owncloud/client/issues/9655)
* Change - We removed the TLS certificate button from the account page: [#9675](https://github.com/owncloud/client/pull/9675)
* Change - Add "open in web editor" feature: [#9724](https://github.com/owncloud/client/issues/9724)
* Change - Don't display error state when server is unreachable: [#9790](https://github.com/owncloud/client/issues/9790)
* Enhancement - Windows VFS download speed improvement: [#10031](https://github.com/owncloud/client/issues/10031)
* Enhancement - Add a prefer: minimal header to PROPFINDs: [#10104](https://github.com/owncloud/client/pull/10104)
* Enhancement - Allow creation of sync roots with long paths: [#10135](https://github.com/owncloud/client/pull/10135/)
* Enhancement - Windows add longPath awareness: [#10136](https://github.com/owncloud/client/pull/10136)
* Enhancement - Estimate duration of network requests in httplogger: [#10142](https://github.com/owncloud/client/pull/10142)
* Enhancement - Tweak logging format: [#10310](https://github.com/owncloud/client/pull/10310)
* Enhancement - Display `Show ownCloud` instead of `Settings` in systray: [#8234](https://github.com/owncloud/client/issues/8234)
* Enhancement - Built-in AppImage self-updater: [#8923](https://github.com/owncloud/client/issues/8923)
* Enhancement - Don't query private links if disabled on the server: [#8998](https://github.com/owncloud/client/issues/8998)
* Enhancement - Add CMakeOption WITH_AUTO_UPDATER: [#9082](https://github.com/owncloud/client/issues/9082)
* Enhancement - Rewrite wizard from scratch: [#9249](https://github.com/owncloud/client/issues/9249)
* Enhancement - Remove use of legacy DAV endpoint: [#9538](https://github.com/owncloud/client/pull/9538)
* Enhancement - Support for OCIS Spaces: [#9154](https://github.com/owncloud/client/pull/9154)
* Enhancement - Set Windows VFS placeholders readonly if needed: [#9598](https://github.com/owncloud/client/issues/9598)
* Enhancement - Create continuous log files: [#9731](https://github.com/owncloud/client/issues/9731)
* Enhancement - Display a correct error when the wrong user was authenticated: [#9772](https://github.com/owncloud/client/issues/9772)
* Enhancement - We improved the performance for local filesystem actions: [#9910](https://github.com/owncloud/client/pull/9910)
* Enhancement - We improved the performance of db access: [#9918](https://github.com/owncloud/client/pull/9918)
* Enhancement - Reduce CPU load during discovery: [#9919](https://github.com/owncloud/client/pull/9919)
* Enhancement - Remove app name from connection error message: [#9923](https://github.com/owncloud/client/issues/9923)
* Enhancement - Allow HTTP/1.1 pipelining: [#9930](https://github.com/owncloud/client/pull/9930/)
* Enhancement - Improve look and feel of many dialogs on macOS: [#9995](https://github.com/owncloud/client/issues/9995)

Details
-------

* Bugfix - Don't unset implicit log flush: [#9515](https://github.com/owncloud/client/pull/9515)

   Since https://github.com/owncloud/client/pull/9515 we flush the the log if the output is
   stdout. We fixed a bug which disabled it again.

   https://github.com/owncloud/client/pull/9515

* Bugfix - We fixed a crash: [#10017](https://github.com/owncloud/client/pull/10017)

   We fixed a crash that could occur after a folder reported a setup error

   https://github.com/owncloud/client/pull/10017

* Bugfix - Sync status changes are now directly displayed: [#10101](https://github.com/owncloud/client/issues/10101)

   https://github.com/owncloud/client/issues/10101

* Bugfix - Windows VFS fixed some failing downloads: [#49](https://github.com/owncloud/client-desktop-vfs-win/pull/49)

   We fixed an api issue where some downloads in the Explorer caused infite download restarts.

   https://github.com/owncloud/client-desktop-vfs-win/pull/49

* Bugfix - Don't trigger ignore list when files are locked on the server: [#5382](https://github.com/owncloud/enterprise/issues/5382)

   https://github.com/owncloud/enterprise/issues/5382

* Bugfix - Properly resume upload with a partial local discovery: [#5382](https://github.com/owncloud/enterprise/issues/5382)

   https://github.com/owncloud/enterprise/issues/5382
   https://github.com/owncloud/client/pull/10200

* Bugfix - Add request time and other missing data to .owncloudsync.log: [#7348](https://github.com/owncloud/client/issues/7348)

   Some parameters were missing for some operations. This fix makes the log more complete and more
   useful as a result.

   https://github.com/owncloud/client/issues/7348

* Bugfix - Don't display a conext menu on the root folder: [#8595](https://github.com/owncloud/client/issues/8595)

   https://github.com/owncloud/client/issues/8595

* Bugfix - Fix copy url location for private links: [#9048](https://github.com/owncloud/client/issues/9048)

   We fixed a bug where a placholder was copied to the clipboard instead of the url.

   https://github.com/owncloud/client/issues/9048

* Bugfix - Fix status of files uploaded with TUS: [#9472](https://github.com/owncloud/client/pull/9472)

   Setting the upload status of files uploaded with TUS failed as we were still using the file.

   https://github.com/owncloud/client/pull/9472

* Bugfix - The condition for the read only files menu was inverted: [#9574](https://github.com/owncloud/client/issues/9574)

   We fixed a bug where we displayed solutions to fix issues with read only fils for readable files
   and vise versa.

   https://github.com/owncloud/client/issues/9574

* Bugfix - Deadlock in folder context menu in a folder selection dialog: [#9681](https://github.com/owncloud/client/issues/9681)

   We fixed a deadlock when a user requested a context menu in a folder slection dialog on Windows.

   https://github.com/owncloud/client/issues/9681

* Bugfix - Fix never ending sync: [#9725](https://github.com/owncloud/client/issues/9725)

   Under certain conditions an upload could enter a state were it would never finish. The client
   would wait for that upload so only a restart of the client or a manual abort of the sync could
   resolve the issue.

   https://github.com/owncloud/client/issues/9725

* Bugfix - Fix adding bookmarks on Gtk+ 3 based desktops: [#9752](https://github.com/owncloud/client/pull/9752)

   We used to add those bookmarks in a Gtk+ 2 compatible way only. Now, bookmarks are added to the
   file belonging to Gtk+ 3, dropping support for end-of-life Gtk+ 2. The bookmarks are now shown
   again for all Gtk+ 3 compatible file browsers, including Thunar, Nautilus, Nemo, Caja, etc.

   https://github.com/owncloud/client/pull/9752

* Bugfix - Stop the activity spinner when the request failed: [#9798](https://github.com/owncloud/client/issues/9798)

   If the server did not provide the activity endpoint we always displayed a progress spinenr.

   https://github.com/owncloud/client/issues/9798

* Bugfix - Changes during upload of a file could still trigger the ignore list: [#9924](https://github.com/owncloud/client/issues/9924)

   We fixed another issue where changes during an upload could cause the file to be ignored for an
   increasing amount of time.

   https://github.com/owncloud/client/issues/9924

* Change - Windows: Update the folder icon on every start: [#10184](https://github.com/owncloud/client/issues/10184)

   The ownCloud installation path might have changed, causing the desktop.ini to point at the
   wrong path. We now update the icon location on every application start.

   https://github.com/owncloud/client/issues/10184

* Change - Don't guess remote folder in owncloudcmd: [#10193](https://github.com/owncloud/client/issues/10193)

   The commandline client was modified to explicitly accept remote folder, the remote folder
   must no longer be encoded in the server url.

   https://github.com/owncloud/client/issues/10193

* Change - When connected to oCIS, open the browser instead of the sharing dialog: [#10206](https://github.com/owncloud/client/issues/10206)

   When connected to oCIS, we now open the browser and navigate to the file the user wanted to share
   instead of opening the legacy sharing dialog.

   https://github.com/owncloud/client/issues/10206

* Change - Owncloudcmd OCIS support: [#10239](https://github.com/owncloud/client/pull/10239)

   When using ocis and spaces with the cmd client the additional parameter `--server` is
   required. `--server` spcifies the url to the server, while the positional parameter
   'server_url' specifies the webdav url.

   https://github.com/owncloud/client/pull/10239

* Change - Make sharedialog preview be more resilient: [#8938](https://github.com/owncloud/client/issues/8938)

   We no longer enforce png thumbnails. We no longer replace the file icon if the thumbnail is
   invalid.

   https://github.com/owncloud/client/issues/8938
   https://github.com/owncloud/client/pull/8939

* Change - We no longer persist cookies: [#9495](https://github.com/owncloud/client/issues/9495)

   We no longer persist cookies over multiple client sessions.

   https://github.com/owncloud/client/issues/9495

* Change - We removed support for ownCloud servers < 10.0: [#9578](https://github.com/owncloud/client/issues/9578)

   https://github.com/owncloud/client/issues/9578

* Change - Drop socket upload job: [#9585](https://github.com/owncloud/client/issues/9585)

   https://github.com/owncloud/client/issues/9585

* Change - Remove support for Windows 7 sidebar links: [#9618](https://github.com/owncloud/client/pull/9618)

   We removed the support for Windows < 10 sidebar links.

   https://github.com/owncloud/client/pull/9618

* Change - Rewrote TLS error handling: [#9655](https://github.com/owncloud/client/issues/9655)

   We rewrote the way we handle TLS errors.

   https://github.com/owncloud/client/issues/9655
   https://github.com/owncloud/client/pull/9643
   https://github.com/owncloud/client/pull/9667

* Change - We removed the TLS certificate button from the account page: [#9675](https://github.com/owncloud/client/pull/9675)

   https://github.com/owncloud/client/pull/9675

* Change - Add "open in web editor" feature: [#9724](https://github.com/owncloud/client/issues/9724)

   We now provide the option to open files in an online office suite from the local file browser
   context menu, provided the server offers integration with one of the supported services.

   https://github.com/owncloud/client/issues/9724

* Change - Don't display error state when server is unreachable: [#9790](https://github.com/owncloud/client/issues/9790)

   We no longer display a network error if the server is currently unavailable.

   https://github.com/owncloud/client/issues/9790

* Enhancement - Windows VFS download speed improvement: [#10031](https://github.com/owncloud/client/issues/10031)

   We improved the performance of downloads performed on virtual files in the Windows Explorer.

   https://github.com/owncloud/client/issues/10031

* Enhancement - Add a prefer: minimal header to PROPFINDs: [#10104](https://github.com/owncloud/client/pull/10104)

   This will not return missing attribs in the reply in a 404 not found status propset. That reduces
   the amount of transfered data significantely.

   https://github.com/owncloud/client/pull/10104

* Enhancement - Allow creation of sync roots with long paths: [#10135](https://github.com/owncloud/client/pull/10135/)

   Until now, we were only able to create a .sync_journal.db in a path with less than 260
   characters.

   https://github.com/owncloud/client/pull/10135/

* Enhancement - Windows add longPath awareness: [#10136](https://github.com/owncloud/client/pull/10136)

   Requires Windows 10 newer than 1607 and the registry key to be enabled see:
   https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation?tabs=registry#enable-long-paths-in-windows-10-version-1607-and-later

   https://github.com/owncloud/client/pull/10136

* Enhancement - Estimate duration of network requests in httplogger: [#10142](https://github.com/owncloud/client/pull/10142)

   We now include an estimated duration in the httploger.

   https://github.com/owncloud/client/pull/10142

* Enhancement - Tweak logging format: [#10310](https://github.com/owncloud/client/pull/10310)

   The logging format is now better parseable for 3rdparty apps that ease debugging.

   https://github.com/owncloud/client/pull/10310

* Enhancement - Display `Show ownCloud` instead of `Settings` in systray: [#8234](https://github.com/owncloud/client/issues/8234)

   We changed the menu entry to align its name with is function.

   https://github.com/owncloud/client/issues/8234

* Enhancement - Built-in AppImage self-updater: [#8923](https://github.com/owncloud/client/issues/8923)

   In release 2.10, we introduced a preview on our future AppImage packaging for Linux
   distributions. Now, these AppImages can self-update using a built-in libappimageupdate
   based updater and ownCloud's update infrastructure.

   https://github.com/owncloud/client/issues/8923
   https://github.com/owncloud/client/pull/9376

* Enhancement - Don't query private links if disabled on the server: [#8998](https://github.com/owncloud/client/issues/8998)

   https://github.com/owncloud/client/issues/8998
   https://github.com/owncloud/client/pull/9840
   https://github.com/owncloud/client/pull/9964

* Enhancement - Add CMakeOption WITH_AUTO_UPDATER: [#9082](https://github.com/owncloud/client/issues/9082)

   WITH_AUTO_UPDATER allows to build the client without the auto updater.

   https://github.com/owncloud/client/issues/9082

* Enhancement - Rewrite wizard from scratch: [#9249](https://github.com/owncloud/client/issues/9249)

   We completely rewrote the wizard from scratch. The new wizard provides greater flexibility
   and makes adding new features easier in the future. It has also been redesigned to improve the
   user experience.

   https://github.com/owncloud/client/issues/9249
   https://github.com/owncloud/client/pull/9482
   https://github.com/owncloud/client/pull/9563
   https://github.com/owncloud/client/pull/9566
   https://github.com/owncloud/client/pull/9577
   https://github.com/owncloud/client/pull/9596
   https://github.com/owncloud/client/pull/9606
   https://github.com/owncloud/client/pull/9621
   https://github.com/owncloud/client/pull/9629
   https://github.com/owncloud/client/pull/9636
   https://github.com/owncloud/client/pull/9637
   https://github.com/owncloud/client/pull/9642
   https://github.com/owncloud/client/pull/9643
   https://github.com/owncloud/client/pull/9697
   https://github.com/owncloud/client/pull/9720
   https://github.com/owncloud/client/pull/9746

* Enhancement - Remove use of legacy DAV endpoint: [#9538](https://github.com/owncloud/client/pull/9538)

   We no longer guess the DAV endpoint depending on the chunking-ng feature.

   https://github.com/owncloud/client/pull/9538

* Enhancement - Support for OCIS Spaces: [#9154](https://github.com/owncloud/client/pull/9154)

   We added support to sync OCIS Spaces.

   https://github.com/owncloud/client/pull/9154
   https://github.com/owncloud/client/pull/9575/

* Enhancement - Set Windows VFS placeholders readonly if needed: [#9598](https://github.com/owncloud/client/issues/9598)

   We now properly set the read only flag on Windows virtual files.

   https://github.com/owncloud/client/issues/9598
   https://github.com/owncloud/client-desktop-vfs-win/issues/24

* Enhancement - Create continuous log files: [#9731](https://github.com/owncloud/client/issues/9731)

   Previously, when logging was enabled, we started a new log file for every sync. This worked
   quite well if you sync a single account and a single folder. With spaces however we have a
   multitude of sync folders, which resulted in hundreds of tiny log files.

   Now, as soon as a log file's size exceeds 100 MiB, a new log file is started, and the old one is moved
   and compressed. The option to delete log files older than 4h was replaced by an option to keep a
   number of log files.

   https://github.com/owncloud/client/issues/9731

* Enhancement - Display a correct error when the wrong user was authenticated: [#9772](https://github.com/owncloud/client/issues/9772)

   When the wrong user was authenticated using oauth we used to display a misleading message. We
   now also style the html response the client provides to the file browser.

   https://github.com/owncloud/client/issues/9772
   https://github.com/owncloud/client/pull/9813

* Enhancement - We improved the performance for local filesystem actions: [#9910](https://github.com/owncloud/client/pull/9910)

   https://github.com/owncloud/client/pull/9910

* Enhancement - We improved the performance of db access: [#9918](https://github.com/owncloud/client/pull/9918)

   We removed a check for the existence of the db that was executed before every access to the db.

   The check was introduced in #6049 to prevent crashes if the db does not exist or is removed during
   runtime. We nowadays gracefully handle missing dbs on startup, removing the db at runtime is
   too much of a corner case to sacrifice that much performance however.

   https://github.com/owncloud/client/pull/9918

* Enhancement - Reduce CPU load during discovery: [#9919](https://github.com/owncloud/client/pull/9919)

   https://github.com/owncloud/client/pull/9919

* Enhancement - Remove app name from connection error message: [#9923](https://github.com/owncloud/client/issues/9923)

   We removed the app name from some connection messages. `No connection to ownCloud at
   http://..` was misleading as the server could have any other branding.

   https://github.com/owncloud/client/issues/9923

* Enhancement - Allow HTTP/1.1 pipelining: [#9930](https://github.com/owncloud/client/pull/9930/)

   Under certain conditions, this change can result in a better network utilization.

   https://github.com/owncloud/client/pull/9930/

* Enhancement - Improve look and feel of many dialogs on macOS: [#9995](https://github.com/owncloud/client/issues/9995)

   https://github.com/owncloud/client/issues/9995

Changelog for ownCloud Desktop Client [2.11.1] (2022-08-31)
=======================================
The following sections list the changes in ownCloud Desktop Client 2.11.1 relevant to
ownCloud admins and users.

[2.11.1]: https://github.com/owncloud/client/compare/v2.11.0...v2.11.1

Summary
-------

* Bugfix - Fix configuration of selective sync from account settings: [#10058](https://github.com/owncloud/client/pull/10058)

Details
-------

* Bugfix - Fix configuration of selective sync from account settings: [#10058](https://github.com/owncloud/client/pull/10058)

   We fixed a bug that prevented the directory tree in the account settings window from being
   expanded beyond the root directory level. The problem was introduced in 8d0dd36d2.

   https://github.com/owncloud/client/pull/10058
   https://github.com/owncloud/client/pull/10065

Changelog for ownCloud Desktop Client [2.11.0] (2022-08-18)
=======================================
The following sections list the changes in ownCloud Desktop Client 2.11.0 relevant to
ownCloud admins and users.

[2.11.0]: https://github.com/owncloud/client/compare/v2.10.1...v2.11.0

Summary
-------

* Bugfix - Compare usernames case insensitive: [#5174](https://github.com/owncloud/enterprise/issues/5174)
* Bugfix - Use UTF-8 for .owncloudsync.log: [#9571](https://github.com/owncloud/client/pull/9571)
* Bugfix - Crash when interacting with a folder in an error state: [#9600](https://github.com/owncloud/client/issues/9600)
* Bugfix - Database was recreated after its removal: [#9791](https://github.com/owncloud/client/issues/9791)
* Bugfix - We fixed a potential crash: [#9864](https://github.com/owncloud/client/issues/9864)
* Bugfix - Windows VFS: Files in an existing folder are dehydrated: [#9966](https://github.com/owncloud/client/pull/9966)
* Bugfix - Run next scheduled sync after a folder was removed: [#9969](https://github.com/owncloud/client/issues/9969)
* Bugfix - Windows VFS: Keep file attributes and pin state: [#34](https://github.com/owncloud/client-desktop-vfs-win/pull/34)
* Enhancement - Throttle the UI updates during sync: [#9832](https://github.com/owncloud/client/issues/9832)
* Enhancement - Run vfs downloads with a high priority: [#9836](https://github.com/owncloud/client/pull/9836)
* Enhancement - Don't abort sync if a user requests a file: [#9956](https://github.com/owncloud/client/pull/9956)

Details
-------

* Bugfix - Compare usernames case insensitive: [#5174](https://github.com/owncloud/enterprise/issues/5174)

   We fixed a bug where the user name was compared with the name provided by the server in a case
   sensitive way.

   https://github.com/owncloud/enterprise/issues/5174

* Bugfix - Use UTF-8 for .owncloudsync.log: [#9571](https://github.com/owncloud/client/pull/9571)

   We fixed a bug where unicode file names were not correctly displayed in .owncloudsync.log.

   https://github.com/owncloud/client/pull/9571

* Bugfix - Crash when interacting with a folder in an error state: [#9600](https://github.com/owncloud/client/issues/9600)

   We fixed a crash wher using the context menu on a folder that encountered an error and was not
   using virutal files.

   https://github.com/owncloud/client/issues/9600

* Bugfix - Database was recreated after its removal: [#9791](https://github.com/owncloud/client/issues/9791)

   We fixed a bug whre the database was recreated during the removal of a sync folder connection.

   https://github.com/owncloud/client/issues/9791

* Bugfix - We fixed a potential crash: [#9864](https://github.com/owncloud/client/issues/9864)

   https://github.com/owncloud/client/issues/9864

* Bugfix - Windows VFS: Files in an existing folder are dehydrated: [#9966](https://github.com/owncloud/client/pull/9966)

   We fixed a bug, when a user selects an existing folder as sync root we previously dehydrated all
   existing files.

   https://github.com/owncloud/client/pull/9966

* Bugfix - Run next scheduled sync after a folder was removed: [#9969](https://github.com/owncloud/client/issues/9969)

   We fixed a bug where we did not start another sync when a folder that was currently syncing was
   removed.

   https://github.com/owncloud/client/issues/9969

* Bugfix - Windows VFS: Keep file attributes and pin state: [#34](https://github.com/owncloud/client-desktop-vfs-win/pull/34)

   When a user selected "Always keep on this device" on a cloud only file, we lost that information.
   "Always keep on this device" only worked on already present files.

   https://github.com/owncloud/client-desktop-vfs-win/pull/34

* Enhancement - Throttle the UI updates during sync: [#9832](https://github.com/owncloud/client/issues/9832)

   We reduced the number of UI updates during the sync, especially with Windows vfs files this
   should improve the performance by a lot.

   https://github.com/owncloud/client/issues/9832
   https://github.com/owncloud/client/pull/9863

* Enhancement - Run vfs downloads with a high priority: [#9836](https://github.com/owncloud/client/pull/9836)

   This should reduce the probability for timeouts when downloading vfs files in the Windows
   explorer.

   https://github.com/owncloud/client/issues/9832
   https://github.com/owncloud/client/pull/9836

* Enhancement - Don't abort sync if a user requests a file: [#9956](https://github.com/owncloud/client/pull/9956)

   Previously we aborted any running sync if a user requested a file that was not yet available
   locally. This was done to ensure the user does not need to wait for the current sync to finish.
   However in todays code both actions the download and the sync can run in parallel.

   https://github.com/owncloud/client/issues/9832
   https://github.com/owncloud/client/pull/9956

Changelog for ownCloud Desktop Client [2.10.1] (2022-04-05)
=======================================
The following sections list the changes in ownCloud Desktop Client 2.10.1 relevant to
ownCloud admins and users.

[2.10.1]: https://github.com/owncloud/client/compare/v2.10.0...v2.10.1

Summary
-------

* Bugfix - Do not strip trailing whitespace from a file or folder name: [#9030](https://github.com/owncloud/client/issues/9030)
* Bugfix - Remove outdated translations: [#9105](https://github.com/owncloud/client/issues/9105)
* Bugfix - Progress reporting for TUS uploads: [#9121](https://github.com/owncloud/client/issues/9121)
* Bugfix - Fix crash if a database error occurs: [#9147](https://github.com/owncloud/client/issues/9147)
* Bugfix - --version showed incorrect information about VFS support: [#9155](https://github.com/owncloud/client/issues/9155)
* Bugfix - Client warns about non exisitng files: [#9236](https://github.com/owncloud/client/issues/9236)
* Bugfix - Mention source file in activity tab when renaming: [#9238](https://github.com/owncloud/client/issues/9238)
* Bugfix - Fix crash on remove account: [#9367](https://github.com/owncloud/client/issues/9367)
* Bugfix - Raise ssl issue dialoig above the wizard: [#9375](https://github.com/owncloud/client/pull/9375)
* Bugfix - Fallback to ownCloud sidebar icons on Mac if none provided in branding: [#9381](https://github.com/owncloud/client/pull/9381)
* Bugfix - Immediately retry upload if file changed during sync: [#9382](https://github.com/owncloud/client/issues/9382)
* Bugfix - Don't reset change time on upload: [#9383](https://github.com/owncloud/client/issues/9383)
* Bugfix - Fix toggling launch-on-login for macOS: [#9387](https://github.com/owncloud/client/issues/9387)
* Bugfix - Fix translated icon names in desktop file with ownBrander themes: [#9390](https://github.com/owncloud/client/pull/9390)
* Bugfix - Fix possible crash: [#9417](https://github.com/owncloud/client/issues/9417)
* Bugfix - Add open local/remote folder options: [#9405](https://github.com/owncloud/client/issues/9405)
* Bugfix - Fix Account Filter for Server Activity tab: [#9481](https://github.com/owncloud/client/pull/9481)
* Bugfix - Ensure proper setup of network jobs on retries: [#9437](https://github.com/owncloud/client/pull/9437)
* Bugfix - If reuqired clear cookies in more scenarios: [#9489](https://github.com/owncloud/client/pull/9489)
* Bugfix - Improve filter pop-up menu and button: [#9425](https://github.com/owncloud/client/issues/9425)
* Bugfix - Possible crash when removing non exisitng folder: [#9533](https://github.com/owncloud/client/issues/9533)
* Bugfix - Can't stop basic auth login: [#9545](https://github.com/owncloud/client/issues/9545)
* Bugfix - Fix potential download failure for renamed file with Windows VFS: [#18](https://github.com/owncloud/client-desktop-vfs-win/pull/18)
* Bugfix - Possible crash when downloading a virtual file on Windows: [#21](https://github.com/owncloud/client-desktop-vfs-win/pull/21)
* Bugfix - Don't publish upload if we can't finish the transaction in the client: [#5052](https://github.com/owncloud/enterprise/issues/5052)
* Enhancement - Retry token refresh multiple times before logout: [#9245](https://github.com/owncloud/client/issues/9245)
* Enhancement - Don't log error when checking removed file for changes: [#9304](https://github.com/owncloud/client/issues/9304)
* Enhancement - Leave password field in share dialog enabled on errors: [#9336](https://github.com/owncloud/client/issues/9336)
* Enhancement - Provide informal German translations: [#9460](https://github.com/owncloud/client/issues/9460)
* Enhancement - Always flush log when logging to stdout: [#9515](https://github.com/owncloud/client/pull/9515)
* Enhancement - Added branding parameter to disallow duplicated folder sync pairs: [#9523](https://github.com/owncloud/client/issues/9523)
* Enhancement - Retry update after 10 minutes: [#9522](https://github.com/owncloud/client/issues/9522)

Details
-------

* Bugfix - Do not strip trailing whitespace from a file or folder name: [#9030](https://github.com/owncloud/client/issues/9030)

   https://github.com/owncloud/client/issues/9030
   https://github.com/owncloud/client/pull/9452

* Bugfix - Remove outdated translations: [#9105](https://github.com/owncloud/client/issues/9105)

   Due to a bug we were not removing the translations that fell below a required quality margin, we
   only stopped updating them. Resulting in even worse translations in some cases.

   https://github.com/owncloud/client/issues/9105

* Bugfix - Progress reporting for TUS uploads: [#9121](https://github.com/owncloud/client/issues/9121)

   We fixed a bug with missing progress reporting in TUS uploads

   https://github.com/owncloud/client/issues/9121

* Bugfix - Fix crash if a database error occurs: [#9147](https://github.com/owncloud/client/issues/9147)

   We no longer crash if a database error occurs on startup, instead the folder will enter an error
   sate similar to the case that the folder does not exist.

   https://github.com/owncloud/client/issues/9147

* Bugfix - --version showed incorrect information about VFS support: [#9155](https://github.com/owncloud/client/issues/9155)

   --version used to always show "Off", even when a VFS plugin was available. This has been fixed
   now.

   https://github.com/owncloud/client/issues/9155
   https://github.com/owncloud/client/pull/9457

* Bugfix - Client warns about non exisitng files: [#9236](https://github.com/owncloud/client/issues/9236)

   We fixed a bug where the client warns about ignored files that where added to the DB in previous
   versions of the client and do no longer exist.

   https://github.com/owncloud/client/issues/9236

* Bugfix - Mention source file in activity tab when renaming: [#9238](https://github.com/owncloud/client/issues/9238)

   https://github.com/owncloud/client/issues/9238
   https://github.com/owncloud/client/pull/9453

* Bugfix - Fix crash on remove account: [#9367](https://github.com/owncloud/client/issues/9367)

   We fixed a potential reference to a deleted item, when an account was removed.

   https://github.com/owncloud/client/issues/9367

* Bugfix - Raise ssl issue dialoig above the wizard: [#9375](https://github.com/owncloud/client/pull/9375)

   Under certain conditions it was possible that the ssl dialog was hidden behind the wizard.

   https://github.com/owncloud/client/pull/9375

* Bugfix - Fallback to ownCloud sidebar icons on Mac if none provided in branding: [#9381](https://github.com/owncloud/client/pull/9381)

   If a customer does not provide sidebar icons we use the ownCloud sidebar icons.

   https://github.com/owncloud/client/pull/9381

* Bugfix - Immediately retry upload if file changed during sync: [#9382](https://github.com/owncloud/client/issues/9382)

   If a file changed during discovery and the actual upload for multiple retries in a row, changes
   of it were ignored for a period of time.

   https://github.com/owncloud/client/issues/9382

* Bugfix - Don't reset change time on upload: [#9383](https://github.com/owncloud/client/issues/9383)

   We fixed a bug where we reset the change time of Windows placeholder files to the value in the
   database during uploads. This cold cause other applications to detect non existing changes in
   that file.

   https://github.com/owncloud/client/issues/9383
   https://github.com/owncloud/client-desktop-vfs-win/pull/16

* Bugfix - Fix toggling launch-on-login for macOS: [#9387](https://github.com/owncloud/client/issues/9387)

   This would fail when upgrading the application, and the upgraded version has one or more
   letters in the name changed from/to upper-case.

   https://github.com/owncloud/client/issues/9387
   https://github.com/owncloud/client/pull/9433

* Bugfix - Fix translated icon names in desktop file with ownBrander themes: [#9390](https://github.com/owncloud/client/pull/9390)

   Fixes broken translated icon reference in desktop entries for some branded build themes.

   https://github.com/owncloud/client/pull/9390

* Bugfix - Fix possible crash: [#9417](https://github.com/owncloud/client/issues/9417)

   We change the initialisation of a Windows icon to prevent a possible crash.

   https://github.com/owncloud/client/issues/9417

* Bugfix - Add open local/remote folder options: [#9405](https://github.com/owncloud/client/issues/9405)

   Add the "open local/remote folder" context menu items for non-sync-root items back into the
   accounts tab in the settings dialog.

   https://github.com/owncloud/client/issues/9405
   https://github.com/owncloud/client/pull/9420

* Bugfix - Fix Account Filter for Server Activity tab: [#9481](https://github.com/owncloud/client/pull/9481)

   https://github.com/owncloud/client/pull/9481

* Bugfix - Ensure proper setup of network jobs on retries: [#9437](https://github.com/owncloud/client/pull/9437)

   On retries network jobs where not properly setup which could lead to undefined behaviour.

   https://github.com/owncloud/client/pull/9437

* Bugfix - If reuqired clear cookies in more scenarios: [#9489](https://github.com/owncloud/client/pull/9489)

   BigIp F5 requires special cookie handling on our side. We only explicitly cleared the cookies
   when we hit an unexpected redirect, now we will clear them also when refreshing our OAuth token.

   https://github.com/owncloud/client/pull/9489

* Bugfix - Improve filter pop-up menu and button: [#9425](https://github.com/owncloud/client/issues/9425)

   - replaced "No filter" option text with "All", to avoid the "No filter is not enabled" situation
   - replace the "Filter" label on the button with "1 Filter"/"2 Filters" when a filter is active,
   so a user can immediately see that without having to open the filter pop-up

   https://github.com/owncloud/client/issues/9425
   https://github.com/owncloud/client/pull/9513

* Bugfix - Possible crash when removing non exisitng folder: [#9533](https://github.com/owncloud/client/issues/9533)

   https://github.com/owncloud/client/issues/9533

* Bugfix - Can't stop basic auth login: [#9545](https://github.com/owncloud/client/issues/9545)

   We fixed a bug where the user was asked for their credentials again and again with no chance to
   abort.

   https://github.com/owncloud/client/issues/9545

* Bugfix - Fix potential download failure for renamed file with Windows VFS: [#18](https://github.com/owncloud/client-desktop-vfs-win/pull/18)

   When a dehydrated file is renamed and immediately opened, the subsequent download might try to
   create a file with the original (un-renamed) name.

   https://github.com/owncloud/client-desktop-vfs-win/pull/18

* Bugfix - Possible crash when downloading a virtual file on Windows: [#21](https://github.com/owncloud/client-desktop-vfs-win/pull/21)

   We fixed a bug that might have caused crashes when working with virtual files on Windows.

   https://github.com/owncloud/client-desktop-vfs-win/pull/21

* Bugfix - Don't publish upload if we can't finish the transaction in the client: [#5052](https://github.com/owncloud/enterprise/issues/5052)

   When a file gets locked during an upload we aborted after the upload finished on the server.
   Resulting in a divergence of the local and remote state which could lead to conflicts.

   https://github.com/owncloud/enterprise/issues/5052

* Enhancement - Retry token refresh multiple times before logout: [#9245](https://github.com/owncloud/client/issues/9245)

   https://github.com/owncloud/client/issues/9245
   https://github.com/owncloud/client/pull/9380

* Enhancement - Don't log error when checking removed file for changes: [#9304](https://github.com/owncloud/client/issues/9304)

   We removed some misleading error messages from the log.

   https://github.com/owncloud/client/issues/9304

* Enhancement - Leave password field in share dialog enabled on errors: [#9336](https://github.com/owncloud/client/issues/9336)

   The password line edit used to be disabled because the related checkbox was unchecked upon
   errors such as failing to satisfy the requirements imposed by the "password policy" server
   app.

   Now, the checkbox will not be unchecked, leaving the line edit enabled and keeping the focus on
   it. This allows users to enter a new password and try again without having to enable the checkbox
   and clicking into the line edit again.

   https://github.com/owncloud/client/issues/9336
   https://github.com/owncloud/client/pull/9508

* Enhancement - Provide informal German translations: [#9460](https://github.com/owncloud/client/issues/9460)

   The community was maintaining an informal German translation for years but we where only able
   to provide a single version of German in the client. We now ship both versions, the informal can
   be selected in the combobox in the advanced settings. To be able to distinguish between formal
   and informal locales, we also include the locale identifier in the dropdown (e.g., "Deutsch
   (de-informal)").

   https://github.com/owncloud/client/issues/9460
   https://github.com/owncloud/client/pull/9502

* Enhancement - Always flush log when logging to stdout: [#9515](https://github.com/owncloud/client/pull/9515)

   We improved the behaviour of logging to a terminal.

   https://github.com/owncloud/client/pull/9515

* Enhancement - Added branding parameter to disallow duplicated folder sync pairs: [#9523](https://github.com/owncloud/client/issues/9523)

   We added a branding parameter to disallow the addition of duplicated folder sync pairs in the
   add folder wizard.

   https://github.com/owncloud/client/issues/9523

* Enhancement - Retry update after 10 minutes: [#9522](https://github.com/owncloud/client/issues/9522)

   When an update (check) fails, it is currently retried only when the regular timeout (10 hours by
   default) is triggered. With this change, we retry the update (check) after 10 minutes already.

   https://github.com/owncloud/client/issues/9522
   https://github.com/owncloud/client/pull/9525

Changelog for ownCloud Desktop Client [2.10.0] (2022-01-17)
=======================================
The following sections list the changes in ownCloud Desktop Client 2.10.0 relevant to
ownCloud admins and users.

[2.10.0]: https://github.com/owncloud/client/compare/v2.9.2...v2.10.0

Summary
-------

* Bugfix - Issues with long file path: [#4896](https://github.com/owncloud/enterprise/issues/4896)
* Bugfix - Do not ask for credentails at start-up when the user logged out: [#8924](https://github.com/owncloud/client/issues/8924)
* Bugfix - A folder moved on the server was displayed as outdated: [#9071](https://github.com/owncloud/client/issues/9071)
* Bugfix - Immediately propagate changes to the ignore list: [#8975](https://github.com/owncloud/client/issues/8975)
* Bugfix - Fix icon name in desktop file with ownBrander themes: [#8992](https://github.com/owncloud/client/issues/8992)
* Bugfix - Crash when handling locked files: [#9170](https://github.com/owncloud/client/issues/9170)
* Bugfix - Don't abort upload if chunk is locked: [#9194](https://github.com/owncloud/client/issues/9194)
* Bugfix - Always restart OAuth2 on error: [#9196](https://github.com/owncloud/client/issues/9196)
* Bugfix - Display correct error message for files containign `\:?*"<>|`: [#9223](https://github.com/owncloud/client/pull/9223/)
* Bugfix - Do not sync when unsyncedfolders file cannot be read: [#9165](https://github.com/owncloud/client/issues/9165)
* Bugfix - Fix failing dehydration causing files to be moved to trash: [#9257](https://github.com/owncloud/client/pull/9257)
* Bugfix - Do not show Activity tab if server app is disabled or uninstalled: [#9260](https://github.com/owncloud/client/issues/9260)
* Bugfix - Handle file locks for delete jobs: [#9293](https://github.com/owncloud/client/issues/9293)
* Bugfix - Run a full local discovery after we where paused or on a forced sync: [#9341](https://github.com/owncloud/client/issues/9341)
* Bugfix - Infinite sync loop if folder is locked: [#9342](https://github.com/owncloud/client/issues/9342)
* Bugfix - We fixed a possible crash: [#13](https://github.com/owncloud/client-desktop-vfs-win/pull/13)
* Enhancement - Reintroduce issue filtering: [#9000](https://github.com/owncloud/client/issues/9000)
* Enhancement - Allow to remove broken sync folders: [#9099](https://github.com/owncloud/client/pull/9099)
* Enhancement - Also ignore local reapeating errors for a period of time: [#9208](https://github.com/owncloud/client/issues/9208)
* Enhancement - Remove the availability menu from the ui: [#9291](https://github.com/owncloud/client/pull/9291)
* Enhancement - Add the syncroot to the search indexed with Windows VFS: [#12](https://github.com/owncloud/client-desktop-vfs-win/pull/12)

Details
-------

* Bugfix - Issues with long file path: [#4896](https://github.com/owncloud/enterprise/issues/4896)

   We fixed another issue with long Windows paths.

   https://github.com/owncloud/enterprise/issues/4896

* Bugfix - Do not ask for credentails at start-up when the user logged out: [#8924](https://github.com/owncloud/client/issues/8924)

   When a user would logout, and quit the client, then on the next start the client would
   immediately ask for credentials. This has been fixed by storing the fact that the user logged
   out before in the account settings.

   https://github.com/owncloud/client/issues/8924

* Bugfix - A folder moved on the server was displayed as outdated: [#9071](https://github.com/owncloud/client/issues/9071)

   We fixed a bug where a folder moved on the server was displayed as outdated.

   https://github.com/owncloud/client/issues/9071

* Bugfix - Immediately propagate changes to the ignore list: [#8975](https://github.com/owncloud/client/issues/8975)

   Previously, when changing the ignore list, those changes would not be propagated to existing
   sync folders. Only after restarting the client, would these changes be applied.

   https://github.com/owncloud/client/issues/8975
   https://github.com/owncloud/client/pull/9149

* Bugfix - Fix icon name in desktop file with ownBrander themes: [#8992](https://github.com/owncloud/client/issues/8992)

   Fixes broken icon reference in desktop entries for some branded build themes.

   https://github.com/owncloud/client/issues/8992
   https://github.com/owncloud/client/pull/9150

* Bugfix - Crash when handling locked files: [#9170](https://github.com/owncloud/client/issues/9170)

   We fixed a crash that could occur when trying to add a locked folder to the databse.

   https://github.com/owncloud/client/issues/9170

* Bugfix - Don't abort upload if chunk is locked: [#9194](https://github.com/owncloud/client/issues/9194)

   Since 2.9 we know that we need exclusive file access to a file to properly handle it with Windows
   virtual files. Therefore we checked for the locked state before we start the upload. Due to a bug
   we checked that for each file chunk, now we only check when the upload starts and when it finished
   completely.

   https://github.com/owncloud/client/issues/9194
   https://github.com/owncloud/client/pull/9264
   https://github.com/owncloud/client/pull/9296

* Bugfix - Always restart OAuth2 on error: [#9196](https://github.com/owncloud/client/issues/9196)

   We now always restart the OAuth2 process once we got a result. This will ensure that a second try
   after an error occurred can succeed.

   https://github.com/owncloud/client/issues/9196

* Bugfix - Display correct error message for files containign `\:?*"<>|`: [#9223](https://github.com/owncloud/client/pull/9223/)

   While the error message was supposed to be: `File names containing the character '%1' are not
   supported on this file system.`

   We displayed: `The file name is a reserved name on this file system.`

   https://github.com/owncloud/client/pull/9223/

* Bugfix - Do not sync when unsyncedfolders file cannot be read: [#9165](https://github.com/owncloud/client/issues/9165)

   Owncloudcmd now checks if the file specified by --unsyncedfolders exists and can be read,
   before starting the sync. If it does not exist, show an error message and quit immediately.

   https://github.com/owncloud/client/issues/9165
   https://github.com/owncloud/client/pull/9241

* Bugfix - Fix failing dehydration causing files to be moved to trash: [#9257](https://github.com/owncloud/client/pull/9257)

   If files where dehydrated by the user the action could fail under certain conditions which
   caused a deletion of the file.

   https://github.com/owncloud/client/pull/9257
   https://github.com/owncloud/client-desktop-vfs-win/pull/9

* Bugfix - Do not show Activity tab if server app is disabled or uninstalled: [#9260](https://github.com/owncloud/client/issues/9260)

   The Activity app API nowadays returns error responses in case the app is disabled or
   uninstalled. This new behavior is now supported in the client.

   https://github.com/owncloud/client/issues/9260
   https://github.com/owncloud/client/pull/9266

* Bugfix - Handle file locks for delete jobs: [#9293](https://github.com/owncloud/client/issues/9293)

   We no longer report an error when the client tries to delete a locked file but wait for the lock to
   be removed.

   This only works when a file is deleted not on folders.

   https://github.com/owncloud/client/issues/9293
   https://github.com/owncloud/client/pull/9295

* Bugfix - Run a full local discovery after we where paused or on a forced sync: [#9341](https://github.com/owncloud/client/issues/9341)

   Previously we did a incremental search wich might have skipped some local changes.

   https://github.com/owncloud/client/issues/9341

* Bugfix - Infinite sync loop if folder is locked: [#9342](https://github.com/owncloud/client/issues/9342)

   We fixed a bug that caused an infinite sync loop if an error occured.

   https://github.com/owncloud/client/issues/9342
   https://github.com/owncloud/client-desktop-vfs-win/pull/14

* Bugfix - We fixed a possible crash: [#13](https://github.com/owncloud/client-desktop-vfs-win/pull/13)

   We fixed a possible crash that could happen during the initialisation of the vfs plugin.

   https://github.com/owncloud/client-desktop-vfs-win/pull/13

* Enhancement - Reintroduce issue filtering: [#9000](https://github.com/owncloud/client/issues/9000)

   We reintroduced a filtering option to the issue table. With the addition of a Filter button we
   also made the existing filter by account feature more accessible.

   https://github.com/owncloud/client/issues/9000
   https://github.com/owncloud/client/pull/9023

* Enhancement - Allow to remove broken sync folders: [#9099](https://github.com/owncloud/client/pull/9099)

   In case a folder is no longer available it was not possible to remove the folder. We now made the
   remove action available in that case.

   https://github.com/owncloud/client/pull/9099

* Enhancement - Also ignore local reapeating errors for a period of time: [#9208](https://github.com/owncloud/client/issues/9208)

   If an error occurs on the server (a url is not reachable) we try a couple of times, then we ignore
   that file for a period of time. We now do the same with erros that occure locally.

   https://github.com/owncloud/client/issues/9208
   https://github.com/owncloud/client/issues/9133

* Enhancement - Remove the availability menu from the ui: [#9291](https://github.com/owncloud/client/pull/9291)

   The availability options should be handled on a folder base and in the file browser.

   https://github.com/owncloud/client/pull/9291

* Enhancement - Add the syncroot to the search indexed with Windows VFS: [#12](https://github.com/owncloud/client-desktop-vfs-win/pull/12)

   Microsoft recommends adding the syncroot to search indexer to improve the performance with
   the file status icons.

   https://github.com/owncloud/client-desktop-vfs-win/pull/12

Changelog for ownCloud Desktop Client [2.9.2] (2021-11-24)
=======================================
The following sections list the changes in ownCloud Desktop Client 2.9.2 relevant to
ownCloud admins and users.

[2.9.2]: https://github.com/owncloud/client/compare/v2.9.1...v2.9.2

Summary
-------

* Bugfix - Properly handle Windows log off: [#8979](https://github.com/owncloud/client/issues/8979)
* Bugfix - Disable vfs option is ignored: [#9159](https://github.com/owncloud/client/issues/9159)
* Bugfix - The client no longer idles after a minor url change: [#9202](https://github.com/owncloud/client/pull/9202)
* Bugfix - Config migration on systems with a 2.4 and a 2.8 config: [#9224](https://github.com/owncloud/client/issues/9224)
* Enhancement - Add branding option to disable icons in the file explorer: [#9167](https://github.com/owncloud/client/issues/9167)
* Enhancement - Branding option to disable warning for multiple sync_journal.db's: [#9216](https://github.com/owncloud/client/pull/9216)

Details
-------

* Bugfix - Properly handle Windows log off: [#8979](https://github.com/owncloud/client/issues/8979)

   We now ensure that we receive the window messages dispatched by the system.

   https://github.com/owncloud/client/issues/8979
   https://github.com/owncloud/client/pull/9142
   https://github.com/owncloud/client/pull/9220
   https://github.com/owncloud/client/pull/9227

* Bugfix - Disable vfs option is ignored: [#9159](https://github.com/owncloud/client/issues/9159)

   We fixed a branding issue where vfs was used even when the parameter was set to disabled.

   https://github.com/owncloud/client/issues/9159
   https://github.com/owncloud/enterprise/issues/4820

* Bugfix - The client no longer idles after a minor url change: [#9202](https://github.com/owncloud/client/pull/9202)

   When the client detects a change of the url we ask the user to accept the change or if it was only
   representational change (demo.com vs demo.com/) we directly accept the change. Due to a bug
   the we aborted the sync only after we updated the url. This caused the client to idle for one
   minute.

   https://github.com/owncloud/client/pull/9202

* Bugfix - Config migration on systems with a 2.4 and a 2.8 config: [#9224](https://github.com/owncloud/client/issues/9224)

   We fixed a bug where the client migrated the old settings from 2.4 to 2.9 instead of the 2.8
   settings. Only branded clients where affected by the issue.

   https://github.com/owncloud/client/issues/9224
   https://github.com/owncloud/client/pull/9226

* Enhancement - Add branding option to disable icons in the file explorer: [#9167](https://github.com/owncloud/client/issues/9167)

   We implemented a branding parameter to disable the display of icons in the file explorer
   context menu, this only affects Windows and Linux.

   https://github.com/owncloud/client/issues/9167

* Enhancement - Branding option to disable warning for multiple sync_journal.db's: [#9216](https://github.com/owncloud/client/pull/9216)

   We added a branding option that disables the `Multiple accounts are sharing the folder`
   warning. In previous client versions a bug caused the creation of new sync journals, causing
   false positives in the detection. While this can be handled by the individual user, companies
   with multiple hundreds of users may opt to disable the warning.

   https://github.com/owncloud/client/pull/9216

Changelog for ownCloud Desktop Client [2.9.1] (2021-10-13)
=======================================
The following sections list the changes in ownCloud Desktop Client 2.9.1 relevant to
ownCloud admins and users.

[2.9.1]: https://github.com/owncloud/client/compare/v2.9.0...v2.9.1

Summary
-------

* Bugfix - Mac multiple dialogs block all input: [#8421](https://github.com/owncloud/client/issues/8421)
* Bugfix - Enforce rtl layout with rtl languages: [#8806](https://github.com/owncloud/client/issues/8806)
* Bugfix - Broken url in branded builds: [#8920](https://github.com/owncloud/client/issues/8920)
* Bugfix - Handle use of directory of another Windows VFS sync client: [#8994](https://github.com/owncloud/client/issues/8994)
* Bugfix - Building libcloudprovider support fails: [#8996](https://github.com/owncloud/client/issues/8996)
* Bugfix - Tables now display local time: [#9006](https://github.com/owncloud/client/issues/9006)
* Bugfix - We fixed a crash when using the retry action on an issue: [#9013](https://github.com/owncloud/client/issues/9013)
* Bugfix - Fix crash when closing the client: [#9014](https://github.com/owncloud/client/issues/9014)
* Bugfix - Crash on missing or unreadable sync root: [#9016](https://github.com/owncloud/client/issues/9016)
* Bugfix - A upgrade to 2.9 causes the usage of a new journal file: [#9019](https://github.com/owncloud/client/issues/9019)
* Bugfix - Properly deployment of Qt translations Mac and Window: [#9022](https://github.com/owncloud/client/issues/9022)
* Bugfix - The file status of suffix placeholders was displayed incorrectly: [#9026](https://github.com/owncloud/client/issues/9026)
* Bugfix - When a folder is removed we leave a database behind: [#9057](https://github.com/owncloud/client/issues/9057)
* Bugfix - Dehydrating placeholders failed if the file is read only: [#9093](https://github.com/owncloud/client/issues/9093)
* Bugfix - Downgrades could trigger deletion of virtual files: [#9114](https://github.com/owncloud/client/issues/9114)
* Enhancement - Display the error type in the issue protocol to allow sorting: [#9010](https://github.com/owncloud/client/pull/9010)

Details
-------

* Bugfix - Mac multiple dialogs block all input: [#8421](https://github.com/owncloud/client/issues/8421)

   We back ported a fix to a Qt bug which causes multiple dialogs on Mac to block all input.

   https://github.com/owncloud/client/issues/8421
   https://bugreports.qt.io/browse/QTBUG-91059
   https://invent.kde.org/packaging/craft-blueprints-kde/-/commit/feca22a30f8d3a2122fd9b2097351fcb2da28543

* Bugfix - Enforce rtl layout with rtl languages: [#8806](https://github.com/owncloud/client/issues/8806)

   We fixed a bug where setting the language to a right to left language on mac did not change the
   layout of the application.

   https://github.com/owncloud/client/issues/8806
   https://github.com/owncloud/client/pull/8981

* Bugfix - Broken url in branded builds: [#8920](https://github.com/owncloud/client/issues/8920)

   We fixed a string issue with branded builds resulting in invalid urls.

   https://github.com/owncloud/client/issues/8920

* Bugfix - Handle use of directory of another Windows VFS sync client: [#8994](https://github.com/owncloud/client/issues/8994)

   We now better handle setup issues during the initialisation of virtual files support.
   Especially the case that a user tries to use a directory managed by a competitor which until now
   caused a crash.

   https://github.com/owncloud/client/issues/8994

* Bugfix - Building libcloudprovider support fails: [#8996](https://github.com/owncloud/client/issues/8996)

   We fixed the libcloudprovider integration.

   https://github.com/owncloud/client/issues/8996

* Bugfix - Tables now display local time: [#9006](https://github.com/owncloud/client/issues/9006)

   We fixed a bug where the sync tables where displaying utc time for some items.

   https://github.com/owncloud/client/issues/9006

* Bugfix - We fixed a crash when using the retry action on an issue: [#9013](https://github.com/owncloud/client/issues/9013)

   Using the context menu action on a sync issue could cause a crash.

   https://github.com/owncloud/client/issues/9013
   https://github.com/owncloud/client/pull/9012

* Bugfix - Fix crash when closing the client: [#9014](https://github.com/owncloud/client/issues/9014)

   We fixed a crash where we crash when we closed the client during a sync.

   https://github.com/owncloud/client/issues/9014

* Bugfix - Crash on missing or unreadable sync root: [#9016](https://github.com/owncloud/client/issues/9016)

   We fixed an issue where the client crashed after a user deleted the sync root or lost access to the
   directory.

   https://github.com/owncloud/client/issues/9016
   https://github.com/owncloud/client/pull/9017
   https://github.com/owncloud/client/pull/9065

* Bugfix - A upgrade to 2.9 causes the usage of a new journal file: [#9019](https://github.com/owncloud/client/issues/9019)

   We fixed a bug where the name of the sync journal was not properly saved to the settings. This
   caused a bug when migration to 2.9, so a new a new sync journal was created.

   This not only caused the loss of some selective sync settings, but also caused the display of the
   following warning message:

   ``` Multiple accounts are sharing the folder. This configuration is know to lead to dataloss
   and is no longer supported. Please consider removing this folder from the account and adding it
   again. ```

   We also removed the account info infix from the sync db used with the cmd client.

   https://github.com/owncloud/client/issues/9019
   https://github.com/owncloud/client/pull/9028
   https://github.com/owncloud/client/pull/9046
   https://github.com/owncloud/client/pull/9054

* Bugfix - Properly deployment of Qt translations Mac and Window: [#9022](https://github.com/owncloud/client/issues/9022)

   We fixed a deployment bug which prevented the translation of some components to be loaded.

   https://github.com/owncloud/client/issues/9022
   https://invent.kde.org/packaging/craft/-/commit/77c114917826480f294d0432f147c9e9f7d19e21

* Bugfix - The file status of suffix placeholders was displayed incorrectly: [#9026](https://github.com/owncloud/client/issues/9026)

   We incorrectly reported that suffix files where ignored.

   https://github.com/owncloud/client/issues/9026

* Bugfix - When a folder is removed we leave a database behind: [#9057](https://github.com/owncloud/client/issues/9057)

   We fixed a bug where we left an empty `sync_journal.db` behind, when we removed a
   folder/account. As we use the presence of `sync_journal.db` to determine whether the folder
   is used by a sync client this prevented using an old folder in a new setup.

   https://github.com/owncloud/client/issues/9057

* Bugfix - Dehydrating placeholders failed if the file is read only: [#9093](https://github.com/owncloud/client/issues/9093)

   We fixed a bug where dehydrating a read only file failed without any apparent reason.

   https://github.com/owncloud/client/issues/9093
   https://gitea.owncloud.services/client/client-plugin-vfs-win/pulls/33

* Bugfix - Downgrades could trigger deletion of virtual files: [#9114](https://github.com/owncloud/client/issues/9114)

   We now prevent the downgrade of Windows VFS folders.

   https://github.com/owncloud/client/issues/9114

* Enhancement - Display the error type in the issue protocol to allow sorting: [#9010](https://github.com/owncloud/client/pull/9010)

   We now display the error type in the not synced protocol and allow to sort by the error type.

   https://github.com/owncloud/client/issues/9000
   https://github.com/owncloud/client/pull/9010

Changelog for ownCloud Desktop Client [2.9.0] (2021-09-08)
=======================================
The following sections list the changes in ownCloud Desktop Client 2.9.0 relevant to
ownCloud admins and users.

[2.9.0]: https://github.com/owncloud/client/compare/v2.8.2...v2.9.0

Summary
-------

* Bugfix - Settings migration from v2.4: [#4597](https://github.com/owncloud/enterprise/issues/4597)
* Bugfix - VFS support for folders in the drive root: [#4639](https://github.com/owncloud/enterprise/issues/4639)
* Bugfix - Keep share link names in sync with server: [#7549](https://github.com/owncloud/client/issues/7549)
* Bugfix - SQLite wal file grows to several gigabyte: [#7646](https://github.com/owncloud/client/issues/7646)
* Bugfix - Fix missing permission for newly created folder: [#8076](https://github.com/owncloud/client/pull/8076)
* Bugfix - Don't recommend non default sync option: [#8317](https://github.com/owncloud/client/issues/8317)
* Bugfix - Limit min window size to 2/3 available: [#8366](https://github.com/owncloud/client/issues/8366)
* Bugfix - Set up default locale correctly: [#8367](https://github.com/owncloud/client/issues/8367)
* Bugfix - Only show the settings if they are explicitly requested on start: [#8590](https://github.com/owncloud/client/issues/8590)
* Bugfix - Ignore consecutive errors for a pereiode of time: [#8672](https://github.com/owncloud/client/issues/8672)
* Bugfix - Properly print the sync events in .owncloudsync.log: [#8729](https://github.com/owncloud/client/issues/8729)
* Bugfix - Correctly handle file path comparison on Windows: [#8732](https://github.com/owncloud/client/issues/8732)
* Bugfix - Locked files are not correctly synced: [#8761](https://github.com/owncloud/client/issues/8761)
* Bugfix - Prompt the user of the cmd client what to do if all files where removed: [#8804](https://github.com/owncloud/client/issues/8804)
* Bugfix - Relaunching the client on macOS will show the settings dialog: [#8377](https://github.com/owncloud/client/issues/8377)
* Bugfix - Fix config migrations from versions 2.8 to 2.9: [#8824](https://github.com/owncloud/client/issues/8824)
* Bugfix - Always exclude .owncloud files: [#8836](https://github.com/owncloud/client/pull/8836)
* Bugfix - Don't crash if a certain move is undone: [#8837](https://github.com/owncloud/client/issues/8837)
* Bugfix - Prevent sync root sharing: [#8849](https://github.com/owncloud/client/issues/8849)
* Bugfix - Removed support for client side certificates: [#8864](https://github.com/owncloud/client/pull/8864)
* Bugfix - The `Re-open Browser` now always does what it says: [#8866](https://github.com/owncloud/client/pull/8866)
* Bugfix - Start oauth/password prompt if password is wrong during start up: [#8901](https://github.com/owncloud/client/issues/8901)
* Bugfix - Handle timeouts occurring during oauth: [#8940](https://github.com/owncloud/client/pull/8940)
* Change - Don't ask the user to switch to http: [#8231](https://github.com/owncloud/client/issues/8231)
* Change - Remove support for http redirects: [#8293](https://github.com/owncloud/client/pull/8293)
* Change - We no longer try to locate an ownCloud install at /owncloud: [#8273](https://github.com/owncloud/client/issues/8273)
* Change - Ignore the desktop.ini file in every directory, not only in top dir: [#8298](https://github.com/owncloud/client/issues/8298)
* Change - Add support for dynamic client registration with OIDC: [#8350](https://github.com/owncloud/client/pull/8350/)
* Change - Include full os version in the about dialog: [#8374](https://github.com/owncloud/client/pull/8374)
* Change - We removed the support for async jobs using OC-JobStatus-Location: [#8398](https://github.com/owncloud/client/pull/8398)
* Change - Add a branding option to skip the advanced setup page: [#8665](https://github.com/owncloud/client/issues/8665)
* Enhancement - Prefer 127.0.0.1 as oauth redirect url: [#4542](https://github.com/owncloud/enterprise/issues/4542)
* Enhancement - Display an icon in the Windows explorer context menu: [#4627](https://github.com/owncloud/client/issues/4627)
* Enhancement - Show last sync date in tray menu: [#5644](https://github.com/owncloud/client/issues/5644)
* Enhancement - Display the information state in case we encountered ignored errors: [#8858](https://github.com/owncloud/client/pull/8858)
* Enhancement - Make crash report IDs easy to copy: [#25](https://github.com/dschmidt/libcrashreporter-qt/pull/25)
* Enhancement - We reworked the tables: [#8158](https://github.com/owncloud/client/issues/8158)
* Enhancement - Provide a socket api call to get the client icon and: [#8464](https://github.com/owncloud/client/issues/8464)
* Enhancement - Add language picker to general settings: [#8466](https://github.com/owncloud/client/issues/8466)
* Enhancement - Attach the last 20 log lines to a crash report: [#8467](https://github.com/owncloud/client/issues/8467)
* Enhancement - Mention the local file name when a file name clash occurs: [#8609](https://github.com/owncloud/client/issues/8609)
* Enhancement - Consider a remote poll interval coming with the server capabilities: [#5947](https://github.com/owncloud/client/issues/5947)
* Enhancement - Improved handling of errors during local file updates: [#8787](https://github.com/owncloud/client/pull/8787)
* Enhancement - Retry sync on `502 Bad Gateway`: [#8811](https://github.com/owncloud/client/issues/8811)

Details
-------

* Bugfix - Settings migration from v2.4: [#4597](https://github.com/owncloud/enterprise/issues/4597)

   We fixed the migration of settings of version 2.4 to the current location.

   https://github.com/owncloud/enterprise/issues/4597

* Bugfix - VFS support for folders in the drive root: [#4639](https://github.com/owncloud/enterprise/issues/4639)

   We fixed a bug where it was not possible to use a folder in C:\ as sync folder.

   https://github.com/owncloud/enterprise/issues/4639

* Bugfix - Keep share link names in sync with server: [#7549](https://github.com/owncloud/client/issues/7549)

   When the name of a share link is changed, e.g., to an empty string, the server may not apply this,
   but assign a fallback value (e.g., the link ID). The client therefore needs to re-check the name
   after sending a change request.

   https://github.com/owncloud/client/issues/7549
   https://github.com/owncloud/client/pull/8546

* Bugfix - SQLite wal file grows to several gigabyte: [#7646](https://github.com/owncloud/client/issues/7646)

   We fixed a bug where the SQLite wal file growed until the client was quit.

   https://github.com/owncloud/client/issues/7646

* Bugfix - Fix missing permission for newly created folder: [#8076](https://github.com/owncloud/client/pull/8076)

   We fixed a bug where a newly created folder had no permissions set.

   https://github.com/owncloud/client/pull/8076

* Bugfix - Don't recommend non default sync option: [#8317](https://github.com/owncloud/client/issues/8317)

   We fixed a bug where sync all was still recommended on Windows

   https://github.com/owncloud/client/issues/8317

* Bugfix - Limit min window size to 2/3 available: [#8366](https://github.com/owncloud/client/issues/8366)

   When scaling was used the window could become bigger than the screen. The size is now limited to
   2/3 of the screen.

   https://github.com/owncloud/client/issues/8366

* Bugfix - Set up default locale correctly: [#8367](https://github.com/owncloud/client/issues/8367)

   Fixes the formatting in locale-dependent widgets, e.g., date pickers, like the one in the
   "share link" window.

   https://github.com/owncloud/client/issues/8367
   https://github.com/owncloud/client/pull/8541
   https://github.com/owncloud/client/pull/8617

* Bugfix - Only show the settings if they are explicitly requested on start: [#8590](https://github.com/owncloud/client/issues/8590)

   We now only display the settings when the user requested it on start and not every time the
   application is started a second time.

   https://github.com/owncloud/client/issues/8590

* Bugfix - Ignore consecutive errors for a pereiode of time: [#8672](https://github.com/owncloud/client/issues/8672)

   We fixed a bug where certain errors caused a sync run every 30 seconds

   https://github.com/owncloud/client/issues/8672

* Bugfix - Properly print the sync events in .owncloudsync.log: [#8729](https://github.com/owncloud/client/issues/8729)

   We fixed a bug in the .owncloudsync.log logger which caused enum values to be printed as a number
   rather than a string.

   https://github.com/owncloud/client/issues/8729

* Bugfix - Correctly handle file path comparison on Windows: [#8732](https://github.com/owncloud/client/issues/8732)

   We fixed a bug in which a change in the casing og the sync root made the client ignore changes in it.

   https://github.com/owncloud/client/issues/8732

* Bugfix - Locked files are not correctly synced: [#8761](https://github.com/owncloud/client/issues/8761)

   We fixed an issue where files locked by office etc, where not correctly synced, when Windows
   Virtual files are enabled.

   https://github.com/owncloud/client/issues/8761
   https://github.com/owncloud/client/issues/8765
   https://github.com/owncloud/client/issues/8766
   https://github.com/owncloud/client/pull/8763
   https://github.com/owncloud/client/pull/8768

* Bugfix - Prompt the user of the cmd client what to do if all files where removed: [#8804](https://github.com/owncloud/client/issues/8804)

   We now prompt the user, previously the cmd client got stuck.

   https://github.com/owncloud/client/issues/8804

* Bugfix - Relaunching the client on macOS will show the settings dialog: [#8377](https://github.com/owncloud/client/issues/8377)

   Relaunching the ownCloud client when it is already running, would seemingly do nothing at all.
   To make this more consistent with other macOS applications, relaunching will now open the
   settings dialog.

   https://github.com/owncloud/client/issues/8377
   https://github.com/owncloud/client/pull/8812

* Bugfix - Fix config migrations from versions 2.8 to 2.9: [#8824](https://github.com/owncloud/client/issues/8824)

   Due to a value change of an internal Qt configuration variable, the configuration data could
   not be migrated on many systems. We fixed this by implementing an additional migration path.

   Furthermore, we removed the dependency on said value within the GUI, and use the values
   explicitly from the theme to display the correct values on UI elements such as buttons.

   https://github.com/owncloud/client/issues/8824
   https://github.com/owncloud/client/pull/8860

* Bugfix - Always exclude .owncloud files: [#8836](https://github.com/owncloud/client/pull/8836)

   Our Linux virtual files implementation is using the file name extension .owncloud those files
   where only ignored if the Linux VFS was enabled. Under some circumstances it could lead to
   undefined client states. We now always ignore those files as system reserved.

   https://github.com/owncloud/client/pull/8836

* Bugfix - Don't crash if a certain move is undone: [#8837](https://github.com/owncloud/client/issues/8837)

   https://github.com/owncloud/client/issues/8837
   https://github.com/owncloud/client/pull/8863
   https://github.com/owncloud/client/pull/8958

* Bugfix - Prevent sync root sharing: [#8849](https://github.com/owncloud/client/issues/8849)

   Due to legacy reasons it is possible to let two sync connections use the same directory. In
   combination with virtual files this was leading to dataloss however.

   https://github.com/owncloud/client/issues/8849
   https://github.com/owncloud/client/issues/8512

* Bugfix - Removed support for client side certificates: [#8864](https://github.com/owncloud/client/pull/8864)

   Client side certificates where never officially supported and where untested in many
   scenarios.

   https://github.com/owncloud/client/pull/8864

* Bugfix - The `Re-open Browser` now always does what it says: [#8866](https://github.com/owncloud/client/pull/8866)

   Under certain conditions the previous authentication run might have failed and the button
   became unresponsive, we now start a new authentication in that case.

   https://github.com/owncloud/client/pull/8866

* Bugfix - Start oauth/password prompt if password is wrong during start up: [#8901](https://github.com/owncloud/client/issues/8901)

   If the oauth token was invalid during start up we didn't start the oauth process and the user
   needed to manually log out in order to log in again.

   https://github.com/owncloud/client/issues/8901

* Bugfix - Handle timeouts occurring during oauth: [#8940](https://github.com/owncloud/client/pull/8940)

   We now handle timeouts occurring during oauth.

   https://github.com/owncloud/client/pull/8940

* Change - Don't ask the user to switch to http: [#8231](https://github.com/owncloud/client/issues/8231)

   We no longer recommend to use a http connection if a https url was not found.

   https://github.com/owncloud/client/issues/8231

* Change - Remove support for http redirects: [#8293](https://github.com/owncloud/client/pull/8293)

   We no longer follow redirects, when a redirect is detected we will start a connection
   validation process that does follow redirects. This change improves the support of APM
   solutions which apply special redirects to provide cookie sessions to the client.

   https://github.com/owncloud/client/pull/8293
   https://github.com/owncloud/client/pull/8253

* Change - We no longer try to locate an ownCloud install at /owncloud: [#8273](https://github.com/owncloud/client/issues/8273)

   We no longer try to locate an ownCloud install in /owncloud if we failed to connect to a server.

   https://github.com/owncloud/client/issues/8273

* Change - Ignore the desktop.ini file in every directory, not only in top dir: [#8298](https://github.com/owncloud/client/issues/8298)

   The windows explorer files called desktop.ini were ignored only in the top sync dir so far. They
   are now ignored in the sync in all directory levels of the file tree.

   https://github.com/owncloud/client/issues/8298
   https://github.com/owncloud/client/pull/8299

* Change - Add support for dynamic client registration with OIDC: [#8350](https://github.com/owncloud/client/pull/8350/)

   We implemented support for dynamic client registration with an OpenID Connect provider.

   https://github.com/owncloud/client/pull/8350/

* Change - Include full os version in the about dialog: [#8374](https://github.com/owncloud/client/pull/8374)

   We now include the os version in the about dialog, this might help us to faster pin down os related
   issues.

   https://github.com/owncloud/client/pull/8374

* Change - We removed the support for async jobs using OC-JobStatus-Location: [#8398](https://github.com/owncloud/client/pull/8398)

   We removed the support of async polling jobs after discovering potential issues.

   https://github.com/owncloud/client/pull/8398

* Change - Add a branding option to skip the advanced setup page: [#8665](https://github.com/owncloud/client/issues/8665)

   If the option is enabled we will create a sync with the default values.

   https://github.com/owncloud/client/issues/8665

* Enhancement - Prefer 127.0.0.1 as oauth redirect url: [#4542](https://github.com/owncloud/enterprise/issues/4542)

   When using OpenID Connect we now always use http://127.0.0.1 as redirect url instead of
   http://localhost, following the recommendations in RFC 8252
   (https://tools.ietf.org/html/rfc8252). For OAuth2 we added a branding parameter which
   allows to specify http://127.0.0.1 instead of http://localhost.

   https://github.com/owncloud/enterprise/issues/4542

* Enhancement - Display an icon in the Windows explorer context menu: [#4627](https://github.com/owncloud/client/issues/4627)

   We now display the ownCloud icon in the context menu.

   https://github.com/owncloud/client/issues/4627

* Enhancement - Show last sync date in tray menu: [#5644](https://github.com/owncloud/client/issues/5644)

   Users can see what "Up to date" refers to explicitly now.

   https://github.com/owncloud/client/issues/5644
   https://github.com/owncloud/client/pull/8547

* Enhancement - Display the information state in case we encountered ignored errors: [#8858](https://github.com/owncloud/client/pull/8858)

   If syncing a file fails multiple times we mark it as ignored to skip it for a certain amount of
   time. If we have ignored files we are not in sync, we now don't display the green icon.

   Additionally this change aligns the icon displayed in the system tray with the icon displayed
   in the app.

   Https://github.com/owncloud/client/issues/7715
   https://github.com/owncloud/client/issues/7365
   https://github.com/owncloud/client/issues/7200
   https://github.com/owncloud/client/issues/5860

   https://github.com/owncloud/client/pull/8858

* Enhancement - Make crash report IDs easy to copy: [#25](https://github.com/dschmidt/libcrashreporter-qt/pull/25)

   Users can now click on crash report IDs to copy them to their personal clipboard. This way, they
   can easily reference them in bug reports.

   https://github.com/owncloud/client/issues/8130
   https://github.com/dschmidt/libcrashreporter-qt/pull/25
   https://github.com/owncloud/client/pull/8540

* Enhancement - We reworked the tables: [#8158](https://github.com/owncloud/client/issues/8158)

   We reworked all the tables in the application to unify their behaviour and improve their
   performance.

   https://github.com/owncloud/client/issues/8158
   https://github.com/owncloud/client/issues/4336
   https://github.com/owncloud/client/issues/8528
   https://github.com/owncloud/client/pull/8584
   https://github.com/owncloud/client/pull/8585
   https://github.com/owncloud/client/pull/8627
   https://github.com/owncloud/client/pull/8629

* Enhancement - Provide a socket api call to get the client icon and: [#8464](https://github.com/owncloud/client/issues/8464)

   Add the icon to the dolphin right click menu

   We added support to get the ownCloud client icon of the current theme to the socket api. We show
   the client icon in the dolphin file browser context menu.

   https://github.com/owncloud/client/issues/8464

* Enhancement - Add language picker to general settings: [#8466](https://github.com/owncloud/client/issues/8466)

   Users can override the automatically chosen language by selecting a custom language in a
   dropdown in the general settings. Furthermore, a --language CLI parameter was added that
   serves the same purpose.

   https://github.com/owncloud/client/issues/8466
   https://github.com/owncloud/client/pull/8493

* Enhancement - Attach the last 20 log lines to a crash report: [#8467](https://github.com/owncloud/client/issues/8467)

   We now save the last 20 lines of log to a tempoary file. This file is then part of a crash report.

   https://github.com/owncloud/client/issues/8467
   https://github.com/owncloud/client/pull/8469

* Enhancement - Mention the local file name when a file name clash occurs: [#8609](https://github.com/owncloud/client/issues/8609)

   We now display the name of the file we detected the clash with.

   https://github.com/owncloud/client/issues/8609
   https://github.com/owncloud/client/pull/8630

* Enhancement - Consider a remote poll interval coming with the server capabilities: [#5947](https://github.com/owncloud/client/issues/5947)

   This way, admins can configure the remote sync poll interval of clients through the
   capabilities settings of the server. Note that the setting in the server capabilities needs to
   be done in milliseconds. Default is 30 seconds.

   https://github.com/owncloud/client/issues/5947
   https://github.com/owncloud/client/issues/8780
   https://github.com/owncloud/client/pull/8777

* Enhancement - Improved handling of errors during local file updates: [#8787](https://github.com/owncloud/client/pull/8787)

   If a local metadata update fails we now provide the proper error in the ui. In case that the error
   was caused by a locked file we now retry the operation.

   https://github.com/owncloud/client/pull/8787

* Enhancement - Retry sync on `502 Bad Gateway`: [#8811](https://github.com/owncloud/client/issues/8811)

   We now treat a `502 Bad Gateway` as an less severer error and directly initialise a retry.

   https://github.com/owncloud/client/issues/8811

Changelog for ownCloud Desktop Client [2.8.2] (2021-05-28)
=======================================
The following sections list the changes in ownCloud Desktop Client 2.8.2 relevant to
ownCloud admins and users.

[2.8.2]: https://github.com/owncloud/client/compare/v2.8.1...v2.8.2

Summary
-------

* Bugfix - Correctly detect network drives: [#8272](https://github.com/owncloud/client/issues/8272)
* Bugfix - We fixed a potential crash in the socket api: [#8664](https://github.com/owncloud/client/pull/8664)

Details
-------

* Bugfix - Correctly detect network drives: [#8272](https://github.com/owncloud/client/issues/8272)

   We fixed a bug which allowed to use Virtual files on Windows network drives, which is not
   supported by Windows.

   https://github.com/owncloud/client/issues/8272

* Bugfix - We fixed a potential crash in the socket api: [#8664](https://github.com/owncloud/client/pull/8664)

   We fixed a crash in the Mac implementation of the socket api.

   https://github.com/owncloud/client/pull/8664

Changelog for ownCloud Desktop Client [2.8.1] (2021-05-21)
=======================================
The following sections list the changes in ownCloud Desktop Client 2.8.1 relevant to
ownCloud admins and users.

[2.8.1]: https://github.com/owncloud/client/compare/v2.8.0...v2.8.1

Summary
-------

* Bugfix - We fixed an issue with the assignment of tags: [#8633](https://github.com/owncloud/client/pull/8633/)
* Enhancement - Prevent user from setting up a VFS sync to the root of a drive: [#8615](https://github.com/owncloud/client/pull/8615)

Details
-------

* Bugfix - We fixed an issue with the assignment of tags: [#8633](https://github.com/owncloud/client/pull/8633/)

   We fixed the file id used for the assignment of the tag.

   https://github.com/owncloud/client/pull/8633/

* Enhancement - Prevent user from setting up a VFS sync to the root of a drive: [#8615](https://github.com/owncloud/client/pull/8615)

   We now display a warning when a user tries to sync to a drive like D:\ instead of a folder and
   prevent this. Previous versions of the client used to crash.

   https://github.com/owncloud/client/pull/8615

Changelog for ownCloud Desktop Client [2.8.0] (2021-05-06)
=======================================
The following sections list the changes in ownCloud Desktop Client 2.8.0 relevant to
ownCloud admins and users.

[2.8.0]: https://github.com/owncloud/client/compare/v2.7.6...v2.8.0

Summary
-------

* Bugfix - Fix issues with long path: [#4405](https://github.com/owncloud/enterprise/issues/4405)
* Bugfix - Moves in folder sync connections where executed as a delete and upload: [#7838](https://github.com/owncloud/client/issues/7838)
* Bugfix - Fix brandings with space in the name: [#8482](https://github.com/owncloud/client/pull/8482)
* Bugfix - We fixed an issue with the promptDeleteAllFiles setting: [#8484](https://github.com/owncloud/client/issues/8484)
* Enhancement - Added branding option to enforce VFS: [#4144](https://github.com/owncloud/enterprise/issues/4144)
* Enhancement - Add initial support for backups triggered by an external tool: [#8437](https://github.com/owncloud/client/pull/8437)
* Enhancement - Add an icon to the Windows system tray notification: [#8539](https://github.com/owncloud/client/pull/8539)
* Enhancement - Encode the log file as UTF-8: [#8550](https://github.com/owncloud/client/pull/8550)

Details
-------

* Bugfix - Fix issues with long path: [#4405](https://github.com/owncloud/enterprise/issues/4405)

   We fixed an issue introduced in dd641fae997d71c8396b77def2fa25ad96fdf47f with some
   functions and files paths > 260 characters.

   https://github.com/owncloud/enterprise/issues/4405

* Bugfix - Moves in folder sync connections where executed as a delete and upload: [#7838](https://github.com/owncloud/client/issues/7838)

   We fixed a bug where moves in folder sync connections where executed as a delete and upload.

   https://github.com/owncloud/client/issues/7838
   https://github.com/owncloud/enterprise/issues/4428
   https://github.com/owncloud/client/pull/8453
   https://github.com/owncloud/client/pull/8456
   https://github.com/owncloud/client/pull/8459

* Bugfix - Fix brandings with space in the name: [#8482](https://github.com/owncloud/client/pull/8482)

   We fix a build system issue with brandings containing spaces.

   https://github.com/owncloud/client/pull/8482

* Bugfix - We fixed an issue with the promptDeleteAllFiles setting: [#8484](https://github.com/owncloud/client/issues/8484)

   When promptDeleteAllFiles=false is set the client will now correctly delete all files.

   https://github.com/owncloud/client/issues/8484

* Enhancement - Added branding option to enforce VFS: [#4144](https://github.com/owncloud/enterprise/issues/4144)

   We added a branding option that enforces the use of Virtual Files on Windows.

   https://github.com/owncloud/enterprise/issues/4144
   https://github.com/owncloud/client/pull/8179/

* Enhancement - Add initial support for backups triggered by an external tool: [#8437](https://github.com/owncloud/client/pull/8437)

   We added a socket api function which allows creation of backups.

   https://github.com/owncloud/client/pull/8437
   https://github.com/owncloud/client/pull/8535
   https://github.com/owncloud/client/pull/8536
   https://github.com/owncloud/client/pull/8539

* Enhancement - Add an icon to the Windows system tray notification: [#8539](https://github.com/owncloud/client/pull/8539)

   We now display a branded icon in the system tray notification.

   https://github.com/owncloud/client/pull/8539

* Enhancement - Encode the log file as UTF-8: [#8550](https://github.com/owncloud/client/pull/8550)

   We fixed an issue where the log file might not have been encoded as UTF-8 and thus scrambled file
   names.

   https://github.com/owncloud/client/pull/8550

Changelog for ownCloud Desktop Client [2.7.6] (2021-02-04)
=======================================
The following sections list the changes in ownCloud Desktop Client 2.7.6 relevant to
ownCloud admins and users.

[2.7.6]: https://github.com/owncloud/client/compare/v2.7.5...v2.7.6

Summary
-------

* Bugfix - Fix missing sidebar icons on Mac with branded builds: [#4387](https://github.com/owncloud/enterprise/issues/4387)
* Bugfix - Case sensitive comparison of checksum algorithm: [#8371](https://github.com/owncloud/client/pull/8371)

Details
-------

* Bugfix - Fix missing sidebar icons on Mac with branded builds: [#4387](https://github.com/owncloud/enterprise/issues/4387)

   We fixed an issue where branded client where lacking the sidebar icons.

   https://github.com/owncloud/enterprise/issues/4387

* Bugfix - Case sensitive comparison of checksum algorithm: [#8371](https://github.com/owncloud/client/pull/8371)

   We fixed a bug where the checksum detection was case sensitive and used a different casing than
   the server.

   https://github.com/owncloud/client/pull/8371
   https://github.com/owncloud/client/pull/8376

Changelog for ownCloud Desktop Client [2.7.5] (2021-01-28)
=======================================
The following sections list the changes in ownCloud Desktop Client 2.7.5 relevant to
ownCloud admins and users.

[2.7.5]: https://github.com/owncloud/client/compare/v2.7.4...v2.7.5

Summary
-------

* Bugfix - Support dark theme with branded client and vanilla icons: [#4363](https://github.com/owncloud/enterprise/issues/4363)
* Bugfix - Mac tray icon is scaled wrong: [#8149](https://github.com/owncloud/client/issues/8149)
* Bugfix - Fix potential crashes with the remove all dialog: [#8314](https://github.com/owncloud/client/pull/8314)
* Bugfix - Detect file name clash with VirtualFiles enabled: [#8323](https://github.com/owncloud/client/issues/8323)
* Bugfix - Remove stray placeholders: [#8326](https://github.com/owncloud/client/issues/8326)
* Bugfix - Fix wrong option provided to OIDC: [#8390](https://github.com/owncloud/client/issues/8390)
* Change - Third party upgrades in distributed binaries: [#8349](https://github.com/owncloud/client/issues/8349)

Details
-------

* Bugfix - Support dark theme with branded client and vanilla icons: [#4363](https://github.com/owncloud/enterprise/issues/4363)

   We fixed a bug where the dark vanilla icons where used with a branded client.

   https://github.com/owncloud/enterprise/issues/4363

* Bugfix - Mac tray icon is scaled wrong: [#8149](https://github.com/owncloud/client/issues/8149)

   We backported a change to Qt 5.12.10 which fixed the scaling of the system tray icon on Big Sur.

   https://github.com/owncloud/client/issues/8149

* Bugfix - Fix potential crashes with the remove all dialog: [#8314](https://github.com/owncloud/client/pull/8314)

   We fixed a bug a dialog window belonging to a removed account could still be visible. User action
   on that dialog would then cause a crash.

   https://github.com/owncloud/client/pull/8314

* Bugfix - Detect file name clash with VirtualFiles enabled: [#8323](https://github.com/owncloud/client/issues/8323)

   We fixed an issue where the file name clash detection was not run with VirtualFiles enabled.

   https://github.com/owncloud/client/issues/8323

* Bugfix - Remove stray placeholders: [#8326](https://github.com/owncloud/client/issues/8326)

   We fixed a bug where Windows Virtual Files where not handled as such and thus not removed.

   https://github.com/owncloud/client/issues/8326

* Bugfix - Fix wrong option provided to OIDC: [#8390](https://github.com/owncloud/client/issues/8390)

   We fixed a bug where we passed a wrong value to the OIDC display parameter

   https://github.com/owncloud/client/issues/8390

* Change - Third party upgrades in distributed binaries: [#8349](https://github.com/owncloud/client/issues/8349)

   We updated Qt from 5.12.9 to 5.12.10. We updated OpenSSL from 1.1.1g to 1.1.1i. Linux
   dependencies: QtKeychain was updated from 0.10.0 to 0.12.0

   https://github.com/owncloud/client/issues/8349

Changelog for ownCloud Desktop Client [2.7.4] (2020-12-21)
=======================================
The following sections list the changes in ownCloud Desktop Client 2.7.4 relevant to
ownCloud admins and users.

[2.7.4]: https://github.com/owncloud/client/compare/v2.7.3...v2.7.4

Summary
-------

* Bugfix - Fix crash when a download is cancelled: [#4329](https://github.com/owncloud/enterprise/issues/4329)
* Bugfix - Update Windows launch on start entry: [#7672](https://github.com/owncloud/client/issues/7672)
* Bugfix - Log the final http request: [#8289](https://github.com/owncloud/client/pull/8289)
* Bugfix - Properly display parent Window when displaying a dialog: [#8313](https://github.com/owncloud/client/issues/8313)

Details
-------

* Bugfix - Fix crash when a download is cancelled: [#4329](https://github.com/owncloud/enterprise/issues/4329)

   We fixed a crash, on Windows, when a user cancelled a download of a VirtualFile using the Windows
   explorer.

   https://github.com/owncloud/enterprise/issues/4329

* Bugfix - Update Windows launch on start entry: [#7672](https://github.com/owncloud/client/issues/7672)

   We fixed a bug where launch on start did not work after a re install to a new location.

   https://github.com/owncloud/client/issues/7672

* Bugfix - Log the final http request: [#8289](https://github.com/owncloud/client/pull/8289)

   We fixed a bug where the http log did not include all headers of a request.

   https://github.com/owncloud/client/pull/8289

* Bugfix - Properly display parent Window when displaying a dialog: [#8313](https://github.com/owncloud/client/issues/8313)

   We fixed a bug where a dialog was shown before the main window was show.

   https://github.com/owncloud/client/issues/8313

Changelog for ownCloud Desktop Client [2.7.3] (2020-12-11)
=======================================
The following sections list the changes in ownCloud Desktop Client 2.7.3 relevant to
ownCloud admins and users.

[2.7.3]: https://github.com/owncloud/client/compare/v2.7.2...v2.7.3

Summary
-------

* Bugfix - Fix handling of errors with the Windows Cloud Filter API: [#8294](https://github.com/owncloud/client/issues/8294)

Details
-------

* Bugfix - Fix handling of errors with the Windows Cloud Filter API: [#8294](https://github.com/owncloud/client/issues/8294)

   We fixed a bug where errors during the creation of placeholder files where not correctly
   handled. The missing files where than falsely detected as deleted and thus removed from the
   server.

   https://github.com/owncloud/client/issues/8294

Changelog for ownCloud Desktop Client [2.7.2] (2020-12-02)
=======================================
The following sections list the changes in ownCloud Desktop Client 2.7.2 relevant to
ownCloud admins and users.

[2.7.2]: https://github.com/owncloud/client/compare/v2.7.1...v2.7.2

Summary
-------

* Bugfix - Correctly detect Windows 10 taskbar theme: [#8244](https://github.com/owncloud/client/issues/8244)
* Bugfix - Sync small plaintext files with Windows VFS: [#8248](https://github.com/owncloud/client/issues/8248)
* Bugfix - Update "Sync hidden files" button: [#8258](https://github.com/owncloud/client/issues/8258)
* Bugfix - Pause sync when displaying remove all dialog: [#8263](https://github.com/owncloud/client/issues/8263)

Details
-------

* Bugfix - Correctly detect Windows 10 taskbar theme: [#8244](https://github.com/owncloud/client/issues/8244)

   We fixed the detection of a dark system try theme on Windows.

   https://github.com/owncloud/client/issues/8244

* Bugfix - Sync small plaintext files with Windows VFS: [#8248](https://github.com/owncloud/client/issues/8248)

   We fixed a bug where small plaintext files where not synced due to a broken interity check.

   https://github.com/owncloud/client/issues/8248

* Bugfix - Update "Sync hidden files" button: [#8258](https://github.com/owncloud/client/issues/8258)

   We fixed a bug that prevented the "Sync hidden files" from displaying the correct value.

   https://github.com/owncloud/client/issues/8258

* Bugfix - Pause sync when displaying remove all dialog: [#8263](https://github.com/owncloud/client/issues/8263)

   We now pause the syn process when the all files where removed dialog is displayed. This prevents
   multiple dialogs from being displayed.

   https://github.com/owncloud/client/issues/8263

Changelog for ownCloud Desktop Client [2.7.1] (2020-11-18)
=======================================
The following sections list the changes in ownCloud Desktop Client 2.7.1 relevant to
ownCloud admins and users.

[2.7.1]: https://github.com/owncloud/client/compare/v2.7.0...v2.7.1

Summary
-------

* Bugfix - Fix client forgetting VirtualFiles mode: [#8229](https://github.com/owncloud/client/pull/8229)
* Bugfix - Don't follow redirects on .well-known/openid-configuration: [#8232](https://github.com/owncloud/client/pull/8232)

Details
-------

* Bugfix - Fix client forgetting VirtualFiles mode: [#8229](https://github.com/owncloud/client/pull/8229)

   We fixed a migration issue where 2.5 based settings where the client was forgetting the
   VirtualFiles settings.

   https://github.com/owncloud/client/pull/8229

* Bugfix - Don't follow redirects on .well-known/openid-configuration: [#8232](https://github.com/owncloud/client/pull/8232)

   We fixed a bug where the client followed redirects for .well-known/openid-configuration.

   https://github.com/owncloud/openidconnect/issues/20
   https://github.com/owncloud/client/pull/8232

Changelog for ownCloud Desktop Client [2.7.0] (2020-11-13)
=======================================
The following sections list the changes in ownCloud Desktop Client 2.7.0 relevant to
ownCloud admins and users.

[2.7.0]: https://github.com/owncloud/client/compare/v2.6.3...v2.7.0

Summary
-------

* Bugfix - Legacy config migration reverted files to online only: [#7779](https://github.com/owncloud/client/issues/7779)
* Bugfix - Some check boxes in the sharing dialog are disabled: [#7877](https://github.com/owncloud/client/issues/7877)
* Bugfix - Selective sync dialog is displayed when virtual files are enabled: [#7976](https://github.com/owncloud/client/issues/7976)
* Bugfix - Fix support of adler32 checksums with Windows virtual files support: [#7999](https://github.com/owncloud/client/issues/7999)
* Bugfix - Use the correct style for the folder wizard: [#8027](https://github.com/owncloud/client/pull/8027)
* Bugfix - Use the same HDPI for the crash reporter as for the client: [#8042](https://github.com/owncloud/client/issues/8042)
* Bugfix - The back button on the advanced wizard page no longer gets enabled: [#8051](https://github.com/owncloud/client/issues/8051)
* Bugfix - Ensure permissions for newly added files are available: [#8066](https://github.com/owncloud/client/issues/8066)
* Bugfix - Remove notifications when the account they belong to was removed: [#8085](https://github.com/owncloud/client/issues/8085)
* Bugfix - Check whether the local folder supports the virtual file system: [#8131](https://github.com/owncloud/client/issues/8131)
* Bugfix - "All Files removed" dialog no longer blocks the application: [#8170](https://github.com/owncloud/client/issues/8170)
* Bugfix - We fixed the permissions check for local folders on NTFS: [#8187](https://github.com/owncloud/client/issues/8187)
* Change - Don't hardcode a plugin location: [#3839](https://github.com/owncloud/enterprise/issues/3839)
* Change - Detect the Windows 10 theme for the system tray: [#7356](https://github.com/owncloud/client/issues/7356)
* Change - When manually adding a folder sync connection, don't display 404 errors: [#7724](https://github.com/owncloud/client/issues/7724)
* Change - Enabling native VFS on Windows create two shortcuts in the Explorer: [#7748](https://github.com/owncloud/client/issues/7748)
* Change - Redesign the About dialog: [#7749](https://github.com/owncloud/client/issues/7749)
* Change - The password dialog is hidden behind a window: [#7833](https://github.com/owncloud/client/issues/7833)
* Change - The client uploads chunks even though the server repports lack of support: [#7862](https://github.com/owncloud/client/issues/7862)
* Change - Mac does not display a dock entry: [#7868](https://github.com/owncloud/client/issues/7868)
* Change - Option to log HTTP requests and responses: [#7873](https://github.com/owncloud/client/issues/7873)
* Change - Add button for `Log Settings` to advanced settings: [#7881](https://github.com/owncloud/client/issues/7881)
* Change - The layout of serveral ui elements is broken: [#7920](https://github.com/owncloud/client/issues/7920)
* Change - The client hides the window during the authentication process: [#7922](https://github.com/owncloud/client/pull/7922)
* Change - The settings ui shows a prompt for a few seconds: [#7925](https://github.com/owncloud/client/pull/7925)
* Change - Reorgenize Settings: [#7962](https://github.com/owncloud/client/pull/7962)
* Change - Use the checksum type specified by the server by default: [#7989](https://github.com/owncloud/client/pull/7989)
* Change - Move "Choose what to sync" to sync everything settings: [#8018](https://github.com/owncloud/client/pull/8018)
* Change - Enable Windows Virtual files by default: [#8019](https://github.com/owncloud/client/pull/8019)
* Change - Move sync hidden files to advanced settings: [#8020](https://github.com/owncloud/client/issues/8020)
* Change - Replace the old icons with a unified look: [#8038](https://github.com/owncloud/client/pull/8038)
* Change - Sharing dialog is now always on top of the settings dialog: [#8050](https://github.com/owncloud/client/pull/8050)
* Change - Remove the branding option `wizardSelectiveSyncDefaultNothing`: [#8064](https://github.com/owncloud/client/pull/8064)
* Change - Account display name `servername (username)` to `username@servername`: [#8104](https://github.com/owncloud/client/issues/8104)
* Change - Remove update channels from the ownCloud client: [#8127](https://github.com/owncloud/client/issues/8127)
* Change - Display the users avatar in the activity list: [#8169](https://github.com/owncloud/client/issues/8169)
* Change - Support for OpenID Connect: [#7509](https://github.com/owncloud/client/pull/7509)
* Change - Add support for the TUS resumeable upload protocol: [#19](https://github.com/owncloud/product/issues/19)

Details
-------

* Bugfix - Legacy config migration reverted files to online only: [#7779](https://github.com/owncloud/client/issues/7779)

   We fixed a legacy config migration which reverted all files to online only on every start.

   https://github.com/owncloud/client/issues/7779

* Bugfix - Some check boxes in the sharing dialog are disabled: [#7877](https://github.com/owncloud/client/issues/7877)

   We fixed a bug where the servers default sharing permissions where used as limiting factor
   instead of a defualt selection.

   https://github.com/owncloud/client/issues/7877

* Bugfix - Selective sync dialog is displayed when virtual files are enabled: [#7976](https://github.com/owncloud/client/issues/7976)

   We hide that dialog now so that it is no longer possible to remove files from synchronisation
   when virtual files are enabled.

   https://github.com/owncloud/client/issues/7976

* Bugfix - Fix support of adler32 checksums with Windows virtual files support: [#7999](https://github.com/owncloud/client/issues/7999)

   The validation device reported a size of 0 and thus the computations of the checksums was
   aborted.

   https://github.com/owncloud/client/issues/7999
   https://github.com/owncloud/client/pull/8015

* Bugfix - Use the correct style for the folder wizard: [#8027](https://github.com/owncloud/client/pull/8027)

   We now use the same style for the wizard on all platforms

   https://github.com/owncloud/client/pull/8027

* Bugfix - Use the same HDPI for the crash reporter as for the client: [#8042](https://github.com/owncloud/client/issues/8042)

   We fixed the behaviour of the crash reporter on HDPI screens.

   https://github.com/owncloud/client/issues/8042

* Bugfix - The back button on the advanced wizard page no longer gets enabled: [#8051](https://github.com/owncloud/client/issues/8051)

   We fixed a bug where the back button in the advanced wizard page get re enabled.

   https://github.com/owncloud/client/issues/8051

* Bugfix - Ensure permissions for newly added files are available: [#8066](https://github.com/owncloud/client/issues/8066)

   We fixed a bug where newly added files had no server permissions set. Under certain conditions
   that was leading to an undefined behaviour.

   https://github.com/owncloud/client/issues/8066
   https://github.com/owncloud/client/issues/7967

* Bugfix - Remove notifications when the account they belong to was removed: [#8085](https://github.com/owncloud/client/issues/8085)

   We fixed a bug where notifications where still displayed after an account was removed.

   https://github.com/owncloud/client/issues/8085

* Bugfix - Check whether the local folder supports the virtual file system: [#8131](https://github.com/owncloud/client/issues/8131)

   The Windows virtual file system requires NTFS, we now ensure that the folder is using NTFS
   before we continue.

   https://github.com/owncloud/client/issues/8131

* Bugfix - "All Files removed" dialog no longer blocks the application: [#8170](https://github.com/owncloud/client/issues/8170)

   We fixed a bug where a dialog locked the whole application

   https://github.com/owncloud/client/issues/8170

* Bugfix - We fixed the permissions check for local folders on NTFS: [#8187](https://github.com/owncloud/client/issues/8187)

   We fixed a bug where the check whether the local folder is writeable returned a wrong result.
   This could cause a crash with the virtual file system plugin.

   https://github.com/owncloud/client/issues/8187

* Change - Don't hardcode a plugin location: [#3839](https://github.com/owncloud/enterprise/issues/3839)

   We no longer hardcode a plugin location only available on the build system. If a setup uses a non
   default plugin location, please consider setting the environment variable QT_PLUGIN_PATH.

   https://github.com/owncloud/enterprise/issues/3839

* Change - Detect the Windows 10 theme for the system tray: [#7356](https://github.com/owncloud/client/issues/7356)

   We now display the system tray icon according to the current theme

   https://github.com/owncloud/client/issues/7356

* Change - When manually adding a folder sync connection, don't display 404 errors: [#7724](https://github.com/owncloud/client/issues/7724)

   We no longer display 404 errors when exploring the folders. A user might not have access to the
   full file tree on the server.

   https://github.com/owncloud/client/issues/7724

* Change - Enabling native VFS on Windows create two shortcuts in the Explorer: [#7748](https://github.com/owncloud/client/issues/7748)

   We now remove legacy shortcuts when we enable VFS

   https://github.com/owncloud/client/issues/7748

* Change - Redesign the About dialog: [#7749](https://github.com/owncloud/client/issues/7749)

   We redesigned the way the About information is displayed and unified it with the "--version"
   switch.

   https://github.com/owncloud/client/issues/7749
   https://github.com/owncloud/enterprise/issues/3787
   https://github.com/owncloud/client/issues/7704

* Change - The password dialog is hidden behind a window: [#7833](https://github.com/owncloud/client/issues/7833)

   We changed the password dialog to stay on top of the ownCloud window.

   https://github.com/owncloud/client/issues/7833

* Change - The client uploads chunks even though the server repports lack of support: [#7862](https://github.com/owncloud/client/issues/7862)

   We now correctly handle the bigfilechunking capability

   https://github.com/owncloud/client/issues/7862

* Change - Mac does not display a dock entry: [#7868](https://github.com/owncloud/client/issues/7868)

   We changed the behaviour of the client to display a dock entry when we have a window open.

   https://github.com/owncloud/client/issues/7868

* Change - Option to log HTTP requests and responses: [#7873](https://github.com/owncloud/client/issues/7873)

   We now allow to log http requests and responses

   https://github.com/owncloud/client/issues/7873

* Change - Add button for `Log Settings` to advanced settings: [#7881](https://github.com/owncloud/client/issues/7881)

   We added an easy way, besides pressing F12, to access the log settings.

   https://github.com/owncloud/client/issues/7881

* Change - The layout of serveral ui elements is broken: [#7920](https://github.com/owncloud/client/issues/7920)

   We replace an old layout mechanism with a more advanced one.

   https://github.com/owncloud/client/issues/7920
   https://github.com/owncloud/client/issues/7941

* Change - The client hides the window during the authentication process: [#7922](https://github.com/owncloud/client/pull/7922)

   We changed the confusing behavioir and now minimize ownCloud instead. This ensures that the
   window stays accessible.

   https://github.com/owncloud/client/pull/7922

* Change - The settings ui shows a prompt for a few seconds: [#7925](https://github.com/owncloud/client/pull/7925)

   We now hide that prompt by default and only show it if needed.

   https://github.com/owncloud/client/pull/7925

* Change - Reorgenize Settings: [#7962](https://github.com/owncloud/client/pull/7962)

   We rename "General" to "Settings" and move the "Network" into "Settings"

   https://github.com/owncloud/client/pull/7962

* Change - Use the checksum type specified by the server by default: [#7989](https://github.com/owncloud/client/pull/7989)

   The default type for computation of the checksum was sha1 independent of the type specified by
   the server. Under certain conditions that caused multiple computations of the checksum.

   https://github.com/owncloud/client/pull/7989

* Change - Move "Choose what to sync" to sync everything settings: [#8018](https://github.com/owncloud/client/pull/8018)

   While selective sync is a feature only available when everything is synced, the the option had
   its own radio button. We now moved the button to the other sync everything related settings.

   https://github.com/owncloud/client/pull/8018

* Change - Enable Windows Virtual files by default: [#8019](https://github.com/owncloud/client/pull/8019)

   We now enable the Windows Virtual file support by default.

   https://github.com/owncloud/client/issues/8139
   https://github.com/owncloud/client/pull/8019

* Change - Move sync hidden files to advanced settings: [#8020](https://github.com/owncloud/client/issues/8020)

   We moved the option to sync hidden files from the "Edit ignored Files" dialog into the advanced
   settings.

   https://github.com/owncloud/client/issues/8020

* Change - Replace the old icons with a unified look: [#8038](https://github.com/owncloud/client/pull/8038)

   We replaced the different styles and colors of the icons with a new unified look.

   https://github.com/owncloud/client/pull/8038

* Change - Sharing dialog is now always on top of the settings dialog: [#8050](https://github.com/owncloud/client/pull/8050)

   The sharing dialog is now a sub dialog, so it will be easier to continue sharing a folder.

   https://github.com/owncloud/client/pull/8050

* Change - Remove the branding option `wizardSelectiveSyncDefaultNothing`: [#8064](https://github.com/owncloud/client/pull/8064)

   The branding option was removed as believe that it did not provide a good user experience. We
   recommend `newBigFolderSizeLimit` together with `wizardHideFolderSizeLimitCheckbox`
   as a replacement.

   https://github.com/owncloud/client/pull/8064

* Change - Account display name `servername (username)` to `username@servername`: [#8104](https://github.com/owncloud/client/issues/8104)

   We changed the way the accounts are displayed to improve the syntactical value.

   https://github.com/owncloud/client/issues/8104

* Change - Remove update channels from the ownCloud client: [#8127](https://github.com/owncloud/client/issues/8127)

   To ensure a maximum of stability user should not replace their productive client with a preview
   build. For that exact reason we offer tespilotcloud clients, they can be installed in parallel
   and updated via the beta channel.

   If a user is more adventures than the average a manual install of a preview is always possible.

   https://github.com/owncloud/client/issues/8127

* Change - Display the users avatar in the activity list: [#8169](https://github.com/owncloud/client/issues/8169)

   We now display the users avatar if available in the activity log table.

   https://github.com/owncloud/client/issues/8169

* Change - Support for OpenID Connect: [#7509](https://github.com/owncloud/client/pull/7509)

   The ownCloud desktop client now supports OpenID Connect. OpenID Connect 1.0 is a simple
   identity layer on top of the OAuth 2.0 protocol. It allows clients to verify the identity of the
   end-user based on the authentication performed by an authorization server, as well as to
   obtain basic profile information about the end-user in an interoperable and REST-like
   manner.

   OpenID Connect allows clients of all types, including web-based, mobile, and JavaScript
   clients, to request and receive information about authenticated sessions and end-users. The
   specification suite is extensible, allowing participants to use optional features such as
   encryption of identity data, discovery of OpenID providers, and session management, when it
   makes sense for them.

   See http://openid.net/connect/faq/ for a set of answers to frequently asked questions about
   OpenID Connect.

   https://github.com/owncloud/client/pull/7509

* Change - Add support for the TUS resumeable upload protocol: [#19](https://github.com/owncloud/product/issues/19)

   With the support of the TUS protocol we are now able to easily and reliably upload files to ocis.

   https://github.com/owncloud/product/issues/19

Changelog for ownCloud Desktop Client [2.6.3] (2020-06-10)
=======================================
The following sections list the changes in ownCloud Desktop Client 2.6.3 relevant to
ownCloud admins and users.

[2.6.3]: https://github.com/owncloud/client/compare/v2.6.2...v2.6.3

Summary
-------

* Bugfix - Client sometimes does not show up when started by a user: [#7018](https://github.com/owncloud/client/issues/7018)
* Bugfix - Fix several wrong colored icons in dark mode: [#7043](https://github.com/owncloud/client/issues/7043)
* Bugfix - Fixed bug in public link with password required: [#7247](https://github.com/owncloud/client/issues/7247)
* Bugfix - Fixed editing public link expiration date: [#7313](https://github.com/owncloud/client/issues/7313)
* Bugfix - Expand file tree also when no folders are synced: [#7336](https://github.com/owncloud/client/issues/7336)
* Bugfix - Fixed bug saving the cookies: [#7700](https://github.com/owncloud/client/issues/7700)
* Bugfix - Fixed crash in the setup wizard: [#7709](https://github.com/owncloud/client/issues/7709)
* Bugfix - Fixed bug in the Virtual File warning dialog: [#7710](https://github.com/owncloud/client/issues/7710)
* Bugfix - Fixed a potential crash while discovering moves: [#7722](https://github.com/owncloud/client/issues/7722)
* Bugfix - Add Folder Sync Connection sometimes does not warn: [#7741](https://github.com/owncloud/client/issues/7741)
* Bugfix - Resize the buttons in the settings view dynamically: [#7744](https://github.com/owncloud/client/issues/7744)
* Bugfix - Fix status icon after move errors: [#7759](https://github.com/owncloud/client/issues/7759)
* Bugfix - Fixed a potential crash on cancelation of discovery jobs: [#7760](https://github.com/owncloud/client/pull/7760)
* Bugfix - Fix a potential crash on Windows VFS: [#7761](https://github.com/owncloud/client/issues/7761)
* Bugfix - Fixed OAuth2 login of user with `+`: [#7762](https://github.com/owncloud/client/issues/7762)
* Bugfix - On Windows the share dialog does not open as the top most window: [#7774](https://github.com/owncloud/client/issues/7774)
* Bugfix - Client sometimes crashes when a placeholder file was moved: [#7799](https://github.com/owncloud/client/issues/7799)
* Bugfix - Don't override cookies with old values: [#7831](https://github.com/owncloud/client/pull/7831)
* Bugfix - Closing prompt has the same effect as accepting: [#7874](https://github.com/owncloud/client/issues/7874)
* Bugfix - Crash on migration of old settings: [#7878](https://github.com/owncloud/client/issues/7878)
* Change - Restart the client after an update: [#3922](https://github.com/owncloud/enterprise/issues/3922)
* Change - Wizard is hidden behind the browser: [#7856](https://github.com/owncloud/client/issues/7856)

Details
-------

* Bugfix - Client sometimes does not show up when started by a user: [#7018](https://github.com/owncloud/client/issues/7018)

   We fixed a bug where a client sometimes does not show up when a user request a start.

   https://github.com/owncloud/client/issues/7018

* Bugfix - Fix several wrong colored icons in dark mode: [#7043](https://github.com/owncloud/client/issues/7043)

   We fixed multiple issues where monochrome icons where not converted to match the current
   theme.

   https://github.com/owncloud/client/issues/7043

* Bugfix - Fixed bug in public link with password required: [#7247](https://github.com/owncloud/client/issues/7247)

   In the sharing dialog, "password required" capabilities lead to incorrect behaviour

   https://github.com/owncloud/client/issues/7247

* Bugfix - Fixed editing public link expiration date: [#7313](https://github.com/owncloud/client/issues/7313)

   In the sharing dialog, allow editing public link expiration date when it is enforced

   https://github.com/owncloud/client/issues/7313

* Bugfix - Expand file tree also when no folders are synced: [#7336](https://github.com/owncloud/client/issues/7336)

   We have fixed the behaviour of the folder Widget, when a account was added and none of its folders
   was selected to be synced, the tree was not expanded.

   https://github.com/owncloud/client/issues/7336

* Bugfix - Fixed bug saving the cookies: [#7700](https://github.com/owncloud/client/issues/7700)

   Saving the cookies on Windows might fail if the containing folder does not exist

   https://github.com/owncloud/client/issues/7700

* Bugfix - Fixed crash in the setup wizard: [#7709](https://github.com/owncloud/client/issues/7709)

   Fixe crash if setup wizard is closed while the virtual file system dialog is open

   https://github.com/owncloud/client/issues/7709
   https://github.com/owncloud/client/issues/7711

* Bugfix - Fixed bug in the Virtual File warning dialog: [#7710](https://github.com/owncloud/client/issues/7710)

   Don't show virtual file system warning again when the radio button is triggered a second time.
   Declining the dialog had no effect as the radio button was already checked

   https://github.com/owncloud/client/issues/7710
   https://github.com/owncloud/client/issues/7712

* Bugfix - Fixed a potential crash while discovering moves: [#7722](https://github.com/owncloud/client/issues/7722)

   https://github.com/owncloud/client/issues/7722

* Bugfix - Add Folder Sync Connection sometimes does not warn: [#7741](https://github.com/owncloud/client/issues/7741)

   Warn when adding a folder sync connection of a remote folder for which a subdir is already
   synchronized

   https://github.com/owncloud/client/issues/7741

* Bugfix - Resize the buttons in the settings view dynamically: [#7744](https://github.com/owncloud/client/issues/7744)

   Since Qt 5.12 the button text gets elided automatically if the button text would exceed the
   button width.

   https://github.com/owncloud/client/issues/7744

* Bugfix - Fix status icon after move errors: [#7759](https://github.com/owncloud/client/issues/7759)

   The file status icon was not showing an error when a move operation failed

   https://github.com/owncloud/client/issues/7759

* Bugfix - Fixed a potential crash on cancelation of discovery jobs: [#7760](https://github.com/owncloud/client/pull/7760)

   https://github.com/owncloud/client/pull/7760

* Bugfix - Fix a potential crash on Windows VFS: [#7761](https://github.com/owncloud/client/issues/7761)

   We've fixed a potential crash where marking a file as "Always keep on this device" caused the
   client to crash.

   https://github.com/owncloud/client/issues/7761

* Bugfix - Fixed OAuth2 login of user with `+`: [#7762](https://github.com/owncloud/client/issues/7762)

   Make sure that the `+` in the user name is properly encoded in the URL opened by the browser when
   doing an OAuth2 authentication.

   https://github.com/owncloud/client/issues/7762

* Bugfix - On Windows the share dialog does not open as the top most window: [#7774](https://github.com/owncloud/client/issues/7774)

   We now ensure that the our dialogs are correctly raised.

   https://github.com/owncloud/client/issues/7774

* Bugfix - Client sometimes crashes when a placeholder file was moved: [#7799](https://github.com/owncloud/client/issues/7799)

   We fixed an issue where moving a placeholder file would lead to a crash.

   https://github.com/owncloud/client/issues/7799

* Bugfix - Don't override cookies with old values: [#7831](https://github.com/owncloud/client/pull/7831)

   We fixed a bug where a client somteimes overrode the content of the cookie jar with outdated or
   corrupted values

   https://github.com/owncloud/client/pull/7831

* Bugfix - Closing prompt has the same effect as accepting: [#7874](https://github.com/owncloud/client/issues/7874)

   We fixed the handling of the user's input.

   https://github.com/owncloud/client/issues/7874

* Bugfix - Crash on migration of old settings: [#7878](https://github.com/owncloud/client/issues/7878)

   We fixed a crash when user settings are migrated to a new client version.

   https://github.com/owncloud/client/issues/7878

* Change - Restart the client after an update: [#3922](https://github.com/owncloud/enterprise/issues/3922)

   We now start the client after an update, if the client was running before the update.

   https://github.com/owncloud/enterprise/issues/3922

* Change - Wizard is hidden behind the browser: [#7856](https://github.com/owncloud/client/issues/7856)

   We now raise the wizard after a successful authentication

   https://github.com/owncloud/client/issues/7856

Changelog for ownCloud Desktop Client [2.6.2] (2020-02-21)
=======================================
The following sections list the changes in ownCloud Desktop Client 2.6.2 relevant to
ownCloud admins and users.

[2.6.2]: https://github.com/owncloud/client/compare/v2.6.1...v2.6.2

Summary
-------

* Change - Add branding option to disable experimental features: [#7755](https://github.com/owncloud/client/issues/7755)

Details
-------

* Change - Add branding option to disable experimental features: [#7755](https://github.com/owncloud/client/issues/7755)

   https://github.com/owncloud/client/issues/7755

ChangeLog
=========
version 2.6.1 (2020-01-17)
(last updated on de0d330c002436454f3fe4929bd707e5f0425949)

Changes:
* GUI: Change the display name to "server (user name)", show the full text in the tooltip (#6728)
* GUI: Add quit button to the settings dialog (#7547)
* GUI: Show a warning that proxy settings do not apply to localhost (#7169)
* CLI: Make it possible to show settings/quit by command line invocation (#7018, #7547)
* Linux:  Add action to Desktop file to show settings, quit the client (#7018, #7547)

Bugfixes:
* Sync: Correctly sync files on Windows after they got unlocked (owncloud/enterprise#3609)
* Log: Message priority wasn't handled correctly so the console log was flooded(#7453)
* Vfs: Do not overwrite existing files by placeholder (#7557,  #7556)
* Discovery:  Allow more HTTP error code to be treated as ignored dir (#7586)
* GUI: Limit the clickable region of the 'add folder' button (#7326)
* GUI: Don't show the "All files deleted" popup when unselecting everything with selective sync (#7337)
* GUI: Don't put a too big icon in about dialog (#7574)
* Shell Integration: Don't assume read-only folder when permissions are not known (#7330)
* Sync: Temporary disable http2 support by default again (#7610)
* Windows Installer: Remember install location on auto update (#7580)

version 2.6.0 (2019-11-11)
(last update on 5131f50ff048d5213aa69edfbae349de2822498a)

Major changes and additions:
* Rewrote discovery code for performance improvements and better maintainability.
* Tech Preview: Add native virtual files mode for Windows 10.
  (https://github.com/owncloud/client/wiki/Virtual-Files-on-Windows-10)
* Tech Preview: Improvements and fixes for all virtual files mode
  (https://github.com/owncloud/client/wiki/Virtual-Files)
* Add basic support for libcloudproviders for gtk/gnome integration (#7209)
* Remove support for Shibboleth auth, please use OAuth2 server app (#6451)

Some small changes and bug fixes:
* Sync: Better detection of complex renames
* Sync: Add workarounds so HTTP2 may be enabled with Qt >=5.12.4 (#7092, #7174)
* Sync: When propagating new remote directories, set local mtime to server mtime (#7119)
* Sync: Add support for asynchronous upload operations (core#31851)
* Sync: Handle blacklisted_files server capability (#434)
* Sync: Fix downloading of files when the database is used for local discovery
* Sync: Fix sync progress when virtual files are created (#6933)
* Sync: Fix issue with a folder being renamed into another renamed folder (#6694)
* Sync: Reduce client-triggered touch ignore duration from 15s to 3s
* Sync: Fix file attribute propagation when propagating conflicts
* Sync: Fix local discovery when removing a selective sync exclusion
* Sync: Fix detection of case-only renames on Windows
* Sync: Fix race conditions in the linux folder watcher (#7068)
* Sync: Fix issue with special characters in the filename and chunked uploads (#7176)
* Sync: Fix renaming a single file causing the "all files deleted" popup (#7204)
* Sync: Reduce memory use during uploads by not reading whole chunks to memory (#7226)
* Sync: Don't abort on 404, 500, 503 errors (#5088, #5859, #7199)
* Sync: Fix parsing of etags, improving move detection (#7271, #7345)
* Sync: If a move is forbidden, restore the source (#7410)
* Sync: When moving is allowed but deleting is not, do not restore moved items (#7293)
* Sync: Fix delete-before-rename bug (#7441)
* Sync: Delay job execution a bit (#7439)
* Sync: Make sure we schedule only one job (#7439)
* Sync: PropagateDownload: Don't try to open readonly temporaries (#7419)
* Sync: Don't fatal on "Storage temporarily unavailable" (#5088)
* Experimental: Add capability to sync file deltas. (#179) (https://github.com/owncloud/client/wiki/DeltaSync)
* Vfs: The online-only/available-locally flag applies to new remote files now.
* Vfs: Introduce actions and warning text for switching vfs on and off.
* Vfs: Cannot be used with selective sync at the same time.
* Vfs: Can now be fully enabled or disabled.
* Vfs: Suffix mode ignores remote files with the suffix (#6953)
* Vfs: Fix behavior when file is renamed and suffix is added/removed at the same time (#7001)
* Vfs: Improve notifications for file creation actions (#7101)
* Vfs: Improve user-visible aspects of pinning and availability (#7111)
* Vfs: Add note about which plugin is in use to about dialog (#7137)
* Vfs: Fix reliability of "Download file" context menu action (#7124)
* Vfs: Fix crash when dehydrating a complete folder
* Vfs: Make "Free space" context menu action only enabled when it has an effect (#7143)
* Vfs: Ensure the database temporaries are marked as excluded (#7141)
* Vfs: Don't dehydrate existing data when switching on (#7302)
* Vfs: Fix move detection when virtual files are involved (#7001)
* Vfs: Lots of tests and corrections for suffix edge cases (#7350, #7261)
* GUI: Adjust "new public link share" ui so options can be set before share creation
* GUI: Added action to open folder in browser to selective sync context menu (#6471)
* GUI: Add server version info to SSL info button (#6628)
* GUI: Allow log window of running client to be opened via command line
* GUI: Introduce conflict resolution actions to right-click menu of conflicts and files in read-only directories (#6252)
* GUI: Update sooner when user resolves a conflict (#7073)
* GUI: Improve error message for missing data in server replies (#7112)
* GUI: Remove log window, instead focus on easy handling of log files (#6475)
* GUI: Fix notification buttons sometimes getting their own window (#7185)
* GUI: Notifications: Remove do-nothing "OK" button (#7355)
* GUI: Add "Show file versions" context menu action (#7196)
* GUI: Fix layout in "Add Certificate" dialog (#7184)
* GUI: Fix duplicated error message for fatal errors (#5088)
* GUI: Fix selective sync ui initial state after account creation (#7336)
* GUI: Improve help text in ignore editor (#7162)
* GUI: Show restoration items in protocol
* Sharing: Fix issues with enforced passwords and expiry (#7246, #7245)
* Sharing: Fix resharing an item in a share with limited permissions (#7275)
* Sharing: Use the default expiration date even when not enforced (#7256)
* Sharing: When sharing from context menu, show dialog if share creation fails (#7286)
* Sharing: Always show at least readonly permissions (#7429)
* OSX: Fix issues with Finder integration being gone after reboot
* OSX: Use the same implementation as on Linux/Windows for the settings dialog (#7371)
* Linux: Add autostart delay to avoid tray icon issues (#6518)
* Folder watcher: Test before relying on it (#7241)
* Client certs: Fix storage of large certs in older Windows versions (#6776)
* Updater: Show a nicer version string In the "available update" notification (#6602)
* Updater: Set correct state on network error (#3933)
* Updater: Provide more useful options on update failure (#7217)
* Updater: Improve logging (#3475, #7388)
* Updater: Fix Version numer not shown in the user visible string (#7288)
* DB: Database path for new folders now starts  with ".sync_", avoiding the "._" (#5904)
* File hashes: Add support for SHA256 and SHA3
* Cmd: Respect chunk sizing environment variables (#7078)
* Log: Don't write to logdir if --logfile is passed (#6909)
* Log: Make --logfile - work on Windows
* Log: Make --logdir compress logs reliably (#7353)
* Log: Print critical and fatal messages to stderr
* Doc: Migrate the documentation to Antora (#6785)
* Doc: Update Windows build instructions
* Doc: Add explanation of how to manually change server url (#6579)
* Doc: List more environment variables
* Doc: List more config file options (owncloud/docs#1365)
* Build: Fix KDEInstallDirs deprecation warnings (#6922)
* Build: Fixes for compiling on "remarkable" tablet (#6870)
* Build: Add PLUGINDIR variable for finding vfs plugins (#7027)
* Build: Drone (#7426)
* Build: Remove 'binary' submodule, remove outdated VS projects
* Translations: Change the way we pull in translations  (#7426)
* Remove the WebKit dependency (#6451)
* Several performance optimizations
* Update SQLite3 to 3.27.2 (if bundled)
* Many test improvements (like #6358)

version 2.5.4 (2019-03-19)
* Crash fix: Infinite recursion for bad paths on Windows (#7041)
* Crash fix: SocketApi mustn't send if readyRead happens after disconnected (#7044)
* Fix rare error causing spurious local deletes (#6677)
* Disable HTTP2 support due to bugs in Qt 5.12.1 (#7020, QTBUG-73947)
* Fix loading of persisted cookies when loading accounts (#7054)
* Windows: Fix breaking of unrelated explorer actions (#7004, #7023)
* Windows: Forbid syncing of files with bytes 0x00 to 0x1F in filenames (#6987)
* macOS: Opt out of dark mode until problems can be addressed (#7043)
* macOS: Fix folder dehydration requests (#6977)
* Linux: Tray: Try to establish tray after 10s if failed initially (#6518)
* Linux: FolderWatcher: Work around missing notifications (#7068)
* Shares: "copy link" action can create shares with expiry (#7061)
* Selective sync: Don't collapse folder tree when changing selection (#7055)
* Client cert dialog: Avoid incorrect behavior due to multiple signal connections

version 2.5.3 (2019-02-07)
* Connectivity: Add a noUnauthedRequests branding option
* Credentials: Warn in log if keychain-write jobs fail (#6776)
* Database: Move drop-index to after pragmas are set (#6881)
* Download: Ignore Content-length for compressed HTTP2/SPDY replies (#6885)

version 2.5.2 (2019-01-25)
* Crash fix: macOS: When opening settings dialog (#6930)
* Crash fix: macOS: While app is in background
* Crash fix: When deleting an account (#6893)
* Crash fix: During password dialog
* SyncJournalDB: Change sqlite3 locking_mode to "exclusive" (#6881)
* Wizard: Fix setting up accounts with SSL client certs (#6911)
* Sync: Fix duplicate slashes in destination of MOVE operation (#6904)
* Sync: Fix file unlocking triggering too many syncs (#6822)
* GUI: Increase default size of ignore list editor (#6641)
* GUI: Fix background color of SSL info button (#871)
* GUI: Ctrl-L and Cmd-L open the log window (F12 is sometimes taken)
* Vfs: Fix problem with dehydrating a file on OSX (#6844)
* Vfs: Do not show settings window when opening a virtual file (#6764)
* Settings: Fix lookup of system override settings (e.g. from HKEY_LOCAL_MACHINE)
* macOS: New "make macdeployqt" target instead of deploying Qt on "make install"

version 2.5.1 (2018-11-09)
* OAuth2: Refresh the token without aborting the sync (#6814)
* OAuth2: Fix migration from BasicAuth when the server uses LDAP
* Linux: FolderWatcher: fix paths after dir renames (#6808)
* Sync: Always recurse within touched directory (#6804)
* Sync: Fixed crash when aborting sync of large files with older servers
* Sync: Don't error out if X-OC-MTime header is missing (#6797)
* Sync: Fix memory leak during upload (#6699)
* Sync: Server Move: Fix too many starting slashes in the destination header (#6824)
* Sync: Windows: Don't check if a server file name can be encoded (#6810)
* Virtual Files: Renaming a virtual files also rename the file on the server (#6718)
* Virtual Files: Disable the 'choose what to sync' in the new folder wizard when virtual files are selected
* Account Settings: Add a context menu entry to enable or disable virtual files (#6725)
* Account Settings: Fix progress being written in white when there are errors
* Account Settings: Link to about dialog from old about space in General Settings
* GUI: Plug a few smaller memory leaks
* Wizard: Reset the QSslConfiguration before checking the server (#6832)
* Wizard: Manual folder configuration should not create the local folder (#6853)
* Windows Shell Integration: No limit on the amount of selected files (#6780)
* Windows Shell Integration: Make OCUtil helper lib static and link it statically against crt
* Windows: Disable autostartCheckBox if autostart is configured system wide (#6816)
* Windows: Make qFatal() trigger the crash reporter on Windows (#6823)
* macOS: Fix icon name in Info.plist
* macOS: Do not select ownCloud in Finder after installation (#6781)
* macOS: Improve macdeployqt.py
* Discovery: Include path in error message (#6826)
* Database: Allow downgrade from 2.6
* Migration from 2.4: fallback to move file by file if directory move failled (#6807)
* owncloudcmd: Read server version and dav user id from the server (#6830)

version 2.5.0 (2018-09-18)
* Local discovery: Speed up by skipping directories without changes reported by the file system watcher.
* Experimental option to create virtual files (e.g. my_document.txt.owncloud) and download contents on demand ("placeholders")
* Windows: Add sync folders to Explorer's navigation pane (#5295)
* Config: Client configuration in roaming profile on Windows, XDG conform on Linux (#684, #2245)
* Experimental option to upload conflict files (#4557)
* Conflicts: Change conflict file naming scheme
* Conflicts: Add user name to conflict file name (#6325)
* Conflicts: Better comparison when connection broke (#6626)
* Conflicts: Deal with file/folder conflicts (#6312)
* Conflicts: Change tray icon for unresolved conflicts (#6277)
* Conflicts: Add documentation link to conflicts listing (#6396)
* Conflicts: Change tags to be more user friendly (#6365)
* Share dialog: Allow opening it if the file's contents are still syncing (#4608)
* Share dialog: Don't hide account settings when opening it (#6185)
* Share dialog: Remove odd grey square on OSX (#5774)
* Share dialog: Preserve the entered password when unrelated changes are done (#6512)
* Share dialog: Fix Re-shares not not showing up (#6666)
* Sharing: Add "copy public link" to menu (#6356)
* Share link: Update permission wording (#6192)
* Private links: improve legacy fileid derivation (#6745)
* User shares: Show avatars
* OAuth2: Remove the timeout (#6612)
* Wizard: Remove the "Skip folder config" button and instead add a radio button (#3664)
* Wizard: Fix for back button in OAuth2 (#6574)
* Wizard: add a context menu to copy the OAuth2 link (enterprise
* Wizard: Put errors into a scroll area (#6546)
* Wizard: show a message when the URL is invalid
* Wizard: pre-select the right radio button (#6685)
* Selective Sync: Do not abort applying selective sync if one folder has an error (#6675)
* Protocol: Introduce context menu with "open in browser" (#6121)
* Protocol: Correct sorting by size (#6326)
* Issues tab: Invalidate issues selectively (#6226)
* Issues tab: Don't allow two issues for the same file/folder
* Issues tab: addItem performance improvement
* Activities: Remove the text that a server does not support activities when the account is removed (#6679)
* Activities: Handle the fact that the username can contain a '@' (#6728)
* Notifications: Lower hiding timeout
* Notifications: Also have clickable link (#6236)
* Shell integration: Add "Open in browser" entry in the explorer menu (#5903)
* Sync journal: Fix crash when unmounting a drive while a sync is running (#6049)
* Client certs: Improve error message (#6128)
* Settings: Hide selective sync buttons while disconnected (#5809)
* Settings: Show account page when account created
* Settings: Move "About" to a dialog (#6075)
* Settings: Force sync should wipe the blacklist (#6757)
* Excludes: Optimize further the matching of exclude files using regular expression
* Windows: Update Overlay Icon naming
* Windows: Release handle/fd when file open fails (#6699)
* Config: Look for exclude file in a relative path.
* Config: Versionize settings
* Settings: Fix rename migration issue on old macOS
* Credentials: Re-try on Linux if daemon not running  (#4274, #6522)
* Windows: Fixed MSVC build and compiler bugs
* Proxy: Hostname validation and reconnection on setting change (#6140)
* owncloudcmd: Set proxy earlier (#6281)
* Exclude regex: Restore old matching on Windows (#6245)
* Build system: Modernize the CMakeLists.txt files
* Use standard png2ico
* Sync: When detecting a local move, keep the local mtime (#6629)
* Sync: Better error handling for local directory parsing (#6610)
* Sync: Error if properties are missing (#6317)
* Sync: Recover when the PUT reply (or chunkin's MOVE) is lost (#5106)
* Sync: Do not abort a sync if the server closes the connection (#6516)
* Sync: Increase the timeout for the last MOVE/PUT for huge files (#6527)
* Sync: Fix renames making hierarchy inversion (#6694)
* Sync: RemotePermissions: Fix empty vs null (#4608)
* Sync: Fix the "direction" of the "all file delted" message when the server is reset (#6317)
* Data-Fingerprint: Fix backup detection when fingerprint is empty
* propagateuploadv1: Fixed an assert with ownCloud 5
* Download: Use the <s:message> from the reply in the error message (#6459, #6459)
* SocketAPI: dynamic action menu
* Hidden option to move remote-deleted files to trash (#6265)
* FolderStatusModel: Refresh folders on Problem sync (#6337)
* SyncJournal: Clear etag filter before sync
* SyncEngine: Use separate state for two unicode conversions
* owncloudcmd: Do not read the proxy settings from the gui's config file
* ProgressInfo: Add information for local vs remote discovery
* SyncResult: Make sure the number of conflicts is correct (#6226)
* Remove the "CSync" wording from the error messages
* Apply branding to crashreporter resources file
* SslButton: Add HTTP/2 info (#3146)
* SslButton: Improve speed (especially on macOS) (#6031)
* Folder: normalize the local path. (#4424)
* Folder: Fix checking if the folder can be used as sync folder (#6654)
* Blacklisting must prevent parent etag updates (#6411)
* FolderStatusModel: fix potential assert
* Nautilus integration: Not a ColumnProvider
* Nautilus integration: Fix python3 compatibility (#6406, #6643)
* Nautilus: Guard against None state (#6643)
* Dolphin plugin: fall back if $XDG_RUNTIME_DIR is empty
* Notify if an explicitly excluded folder is created (#6222)
* Theme: unify ownCloudTheme and Theme classes
* SyncJournalDb::setSelectiveSyncList: Always use a transaction (#6431)
* Folders: Use "Problem" icon for unresolved conflicts (#6277)
* macOS: Unload the Finder extension on exit (#5382, #3819)
* Logging: Go to new file on Problem/Abort too (#6442)
* Logging: Compress log when switching files (#6442)
* Logging: Add persistent auto-logdir option (#6442)
* Logging: .owncloudsynclog: Allow 10 MB of size (#6420)
* Logging: .owncloudsynclog: Persist X-Request-ID for correlation with server (#6420)
* UI: High-DPI layout fixes
* Network settings: Better warnings about bad configuration (#5885)
* Folder watcher: Show a notification if it becomes unreliable (#6119)
* Ignore editor: Preserve comments in the exclude list file
* Updater: Support EXE->MSI upgrade (different code than 2.4)
* Updater: Remove unused installers before copying new ones into the appdata dir (#6690)
* ConnectionValidator: change the minimum server version to 7.0
* ConnectionValidator: Warn when the server version is less than 10.0
* Valgrind: Refactorings to avoid errors
* Crash fixes (#6562 and more)
* Windows: Fix missing company name in our DLLs
* Windows: Appveyor/craft changes
* Linux: More tray workarounds (#6545)
* libocsync: Rename to ${APPLICATION_EXECUTABLE}_csync
* Don't use Qt deprecated API now that we required Qt 5.6

version 2.4.3 (2018-08-13)
* Windows: Don't ignore files with FILE_ATTRIBUTE_TEMPORARY (#6696, #6610)
* OAuth2: Fix infinite loop when the refresh token is expired
* Windows MSI: Fix crash in the auto updater
* Nautilus: Guard against None state (#6643)

version 2.4.2 (2018-07-18)
* Linux: Tray workarounds (#6545)
* Fix nautilus/nemo shell issues (#6393, #6406)
* Updater: Add update channel feature (#6259)
* Updater: Support EXE->MSI upgrade
* SyncJournal: Fixes for sync folders on removable media (#6049, #6049)
* SslButton: Add HTTP/2 info (#3146)
* Fix assert when using ownCloud server 5 (which you should not) (#6403)
* Normalize local path (#4424)
* Blacklisting must prevent parent etag updates (#6411)
* macdeployqt: Adjust minimum version based on our Qt (#5932)
* macOS: Unload the Finder extension on exit (#5382, #3819)
* Upload: Adjust timeout for final job based on file size (#6527)
* Sync: When detecting a local move, keep the local mtime (#6629)
* Credentials: Retry fetching from the keychain in case the keychain is still starting (#4274, #6522)
* OAuth2: Try to refresh the token even if the credentials weren't ready (#6522)

version 2.4.1 (2018-03-05)
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
* Sharing: Use maximum allowed permissions for new share (#6346)
* Nautilus integration: Work with python2 and python3
* Windows: Don't delete contents behind directory junctions (#6322)
* SyncJournal: Don't use LIKE with paths (#6322)
* Fix setting launch-on-startup when the first account is set up (#6347)
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
   longer provide bandwith limiting on Linux-distributions where it is
   using Qt < 5.4
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

version 1.8.4 (release 2015-07-13)
  * Release to ship a security release of openSSL. No source changes of the ownCloud Client code.

version 1.8.3 (release 2015-06-23)
  * Fix a bug in the Windows Installer that could crash explorer (#3320)
  * Reduce 'Connection closed' errors (#3318, #3313, #3298)
  * Ignores: Force a remote discovery after ignore list change (#3172)
  * Shibboleth: Avoid crash by letting the webview use its own QNAM (#3359)
  * System Ignores: Removed *.tmp from system ignore again. If a user
    wants to ignore *.tmp, it needs to be added to the user ignore list.

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
   client to server again (#3246)
 * Propagator: Add 'Content-Length: 0' header to MKCOL request (#3256)
 * Switch on checksum verification through branding or config
 * Add ability for checksum verification of up and download
 * Fix opening external links for some labels (#3135)
 * AccountState: Run only a single validator, allow error message
   overriding (#3236, #3153)
 * SyncJournalDB: Minor fixes and simplificatons
 * SyncEngine: Force re-read of folder Etags for upgrades from
   1.8.0 and 1.8.1
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
   but can not be found on the server. #2919
 * Windows: Reset QNAM to proper function after hibernation. #2899 #2895 #2973
 * Fix argument verification of --confdir #2453
 * Fix a crash when accessing a dangling UploadDevice pointer #2984
 * Add-folder wizard: Make sure there is a scrollbar if folder names
   are too long #2962
 * Add-folder Wizard: Select the newly created folder
 * Activity: Correctly restore column sizes #3005
 * SSL Button: do not crash on empty certificate chain
 * SSL Button: Make menu creation lazy #3007 #2990
 * Lookup system proxy async to avoid hangs #2993 #2802
 * ShareDialog: Some GUI refinements
 * ShareDialog: On creation of a share always retrieve the share
   This makes sure that if a default expiration date is set this is reflected
   in the dialog. #2889
 * ShareDialog: Only show share dialog if we are connected.
 * HttpCreds: Fill pw dialog with previous password. #2848 #2879
 * HttpCreds: Delete password from old location. #2186
 * Do not store Session Cookies in the client cookie storage
 * CookieJar: Don't accidentally overwrite cookies. #2808
 * ProtocolWidget: Always add seconds to the DateTime locale. #2535
 * Updater: Give context as to which app is about to be updated #3040
 * Windows: Add version information for owncloud.exe. This should help us know
   what version or build number a crash report was generated with.
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
   opened by context menu in the file managers (Win, Mac, Nautilus)
   Supports public links with password enforcement
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
   especially with office software but also others.
 * Merged some NetBSD patches
 * Selective sync support for owncloudcmd
 * Reorganize the source repository
 * Prepared direct download
 * Added Crashreporter feature to be switched on on demand
 * A huge amount of bug fixes in all areas of the client.
 * almost 700 commits since 1.7.1

version 1.7.1 (release 2014-12-18)
 * Documentation fixes and updates
 * Nautilus Python plugin fixed for Python 3
 * GUI wording fixes plus improved log messages
 * Fix hidning of the database files in the sync directories
 * Compare http download size with the header value to avoid broken
   downloads, bug #2528
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
   excluded from syncing, plus GUI and setup GUI
 * Added overlay icons for Windows Explorer, Mac OS Finder and GNOME Nautilus.
   Information is provided by the client via a local socket / named pipe API
   which provides information about the sync status of files.
 * Improved local change detection: consider file size, detect files
   with ongoing changes and do not upload immediately
 * Improved HTTP request timeout handler: all successful requests reset
   the timeout counter
 * Improvements for syncing command line tool: netrc support, improved
   SSL support, non interactive mode
 * Permission system: ownCloud 7 delivers file and folder permissions,
   added ability to deal with it for shared folders and more.
 * Ignore handling: Do not recurse into ignored or excluded directories
 * Major sync journal database improvements for more stability and performance
 * New library interface to sqlite3
 * Improve "resync handling" if errors occur
 * Blacklist improvements
 * Improved logging: more useful meta info, removed noise
 * Updated to latest Qt5 versions on Windows and OS X
 * Fixed data loss when renaming a download temporary fails and there was
   a conflict at the same time.
 * Fixed missing warnings about reusing a sync folder when the back button
   was used in the advanced folder setup wizard.
 * The 'Retry Sync' button now also restarts all downloads.
 * Clean up temporary downloads and some extra database files when wiping a
   folder.
 * OS X: Sparkle update to provide pkg format properly
 * OS X: Change distribution format from dmg to pkg with new installer.
 * Windows: Fix handling of filenames with trailing dot or space
 * Windows: Don't use the wrong way to get file mtimes in the legacy propagator.

version 1.6.4 (release 2014-10-22)
 * Fix startup logic, fixes bug #1989
 * Fix raise dialog on X11
 * Win32: fix overflow when computing the size of file > 4GiB
 * Use a fixed function to get files modification time, the
   original one was broken for certain timezone issues, see
   core bug #9781 for details
 * Added some missing copyright headers
 * Avoid data corruption due to wrong error handling, bug #2280
 * Do improved request timeout handling to reduce the number of
   timed out jobs, bug #2155
   version 1.6.3 (release 2014-09-03)
 * Fixed updater on OS X
 * Fixed memory leak in SSL button that could lead to quick memory draining
 * Fixed upload problem with files >4 GB
 * MacOSX, Linux: Bring Settings window to front properly
 * Branded clients: If no configuration is detected, try to import the data
    from a previously configured community edition.

version 1.6.2 (release 2014-07-28 )
 * Limit the HTTP buffer size when downloading to limit memory consumption.
 * Another small mem leak fixed in HTTP Credentials.
 * Fix local file name clash detection for MacOSX.
 * Limit maximum wait time to ten seconds in network limiting.
 * Fix data corruption while trying to resume and the server does
   not support it.
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
   directories are removed
 * Fix reappearing directories if dirs are removed during its
   upload
 * Fix app version in settings dialog, General tab
 * Fix crash in FolderWizard when going offline
 * Shibboleth fixes
 * More specific error messages (file remove during upload, open
   local sync file)
 * Use QSet rather than QHash in SyncEngine (save memory)
 * Fix some memory leaks
 * Fix some thread race problems, ie. wait for neon thread to finish
   before the propagator is shut down
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
   (right now only if no bandwidth limit is set and no proxy is used)
 * Significantly reduced CPU load when checking for local and remote changes
 * Speed up file stat code on Windows
 * Enforce Qt5 for Windows and Mac OS X builds
 * Improved owncloudcmd: SSL support, documentation
 * Added advanced logging of operations (file .???.log in sync
   directory)
 * Avoid creating a temporary copy of the sync database (.ctmp)
 * Enable support for TLS 1.2 negotiation on platforms that use
   Qt 5.2 or later
 * Forward server exception messages to client error messages
 * Mac OS X: Support Notification Center in OS X 10.8+
 * Mac OS X: Use native settings dialog
 * Mac OS X: Fix UI inconsistencies on Mavericks
 * Shibboleth: Warn if authenticating with a different user
 * Remove vio abstraction in csync
 * Avoid data loss when a client file system is not case sensitive

version 1.5.3 (release 2014-03-10 )
  * Fix usage of proxies after first sync run (#1502, #1524, #1459, #1521)
  * Do not wipe the credentials from config for reconnect (#1499, #1503)
  * Do not erase the full account config if an old version of the client stored
    the password (related to above)
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
    more recent version was found automatically (Windows, Mac OS X)
  * Added a button to the account dialog that gives information
    about the encryption layer used for communication, plus a
    certificate information widget
  * Preserve the permission settings of local files rather than
    setting them to a default (Bug #820)
  * Handle windows lnk files correctly (Bug #1307)
  * Detect removes and renames in read only shares and
    restore the gone away files. (Bug #1386)
  * Fixes sign in/sign out and password dialog. (Bug #1353)
  * Fixed error messages (Bug #1394)
  * Lots of fixes for building with Qt5
  * Changes to network limits are now also applied during a
    sync run
  * Fixed mem leak after via valgrind on Mac
  * Imported the ocsync library into miralls repository.
    Adopted all build systems and packaging to that.
  * Introduce a new linux packaging scheme following the
    debian upstream scheme
  * Use a refactored Linux file system watcher based on
    inotify, incl. unit tests
  * Wizard: Gracefully fall back to HTTP if HTTPS connection
    fails, issuing a warning
  * Fixed translation misses in the propagator
  * Fixes in proxy configuration
  * Fixes in sync journal handling
  * Fix the upload progress if the local source is still
    changing when the upload begins.
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
    line error messages now correctly.
  * Wait up to 30 secs before complaining about missing systray
    Fixes bug #949
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
    top optionally disable sync notifications
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

version 1.3.0 (release 2013-06-25 ), csync 0.80.0 required

  * Default proxy port to 8080
  * Don't lose proxy settings when changing passwords
  * Support SOCKS5 proxy (useful in combination with ssh   *D)
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
    to the new location or wipe the folder and start from scratch
  * Wizard: Make setting a custom folder as a sync target work again
  * Fix application icon
  * User-Agent now contains "Mozilla/5.0" and the Platform name (for firewall/proxy compat)
  * Server side directory moves will be detected
  * New setup wizard, defaulting to root syncing (only for new setups)
  * Improved thread stop/termination

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

version 1.1.4 (release 2012-12-19 ), csync 0.60.4 required
  * No changes to mirall, only csync fixes.

version 1.1.3 (release 2012-11-30 ), csync 0.60.3 required
  * No changes to mirall, only csync fixes.

version 1.1.2 (release 2012-11-26 ), csync 0.60.2 required
  * [Fixes] Allow to properly cancel the password dialog.
  * [Fixes] Share folder name correctly percent encoded with old Qt
            4.6 builds ie. Debian.
  * [Fixes] If local sync dir is not existing, create it.
  * [Fixes] lots of other minor fixes.
  * [GUI] Display error messages in status dialog.
  * [GUI] GUI fixes for the connection wizard.
  * [GUI] Show username for connection in statusdialog.
  * [GUI] Show intro wizard on new connection setup.
  * [APP] Use CredentialStore to better support various credential
          backends.
  * [APP] Handle missing local folder more robust: Create it if
          missing instead of ignoring.
  * [APP] Simplify treewalk code.
  * [Platform] Fix Mac building

version 1.1.1 (release 2012-10-18), csync 0.60.1 required
  * [GUI]   Allow changing folder name in single folder mode
  * [GUI]   Windows: Add license to installer
  * [GUI]   owncloud --logwindow will bring up the log window
            in an already running instance
  * [Fixes] Make sure SSL errors are always handled
  * [Fixes] Allow special characters in folder alias
  * [Fixes] Proper workaround for Menu bug in Ubuntu
  * [Fixes] csync: Fix improper memory cleanup which could
            cause memory leaks and crashes
  * [Fixes] csync: Fix memory leak
  * [Fixes] csync: Allow single quote (') in file names
  * [Fixes] csync: Remove stray temporary files

  * [GUI]   Reworked tray context menu.
  * [GUI]   Users can now sync the server root folder.
  * [Fixes] Proxy support: now supports Proxy Auto-Configuration (PAC)
            on Windows, reliability fixes across all OSes.
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
               directory, away from the .csync dir.
  *         Renamed exclude.lst to sync-exclude.lst and moved it to
            /etc/appName()/ for more clean packaging. From the user path,
	    still exclude.lst is read if sync-exclude.lst is not existing.
  *         Placed custom.ini with customization options to /etc/appName()

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
          Portuguese (Brazil), German, Greek, Spanish, Czech, Italian, Slovak,
	  French, Russian, Japanese, Swedish, Portuguese (Portugal)
	  all with translation rate >90%.
  * [Fixes] Loading of self signed certs into Networkmanager (#oc-843)
  * [Fixes] Win32: Handle SSL dll loading correctly.
  * [Fixes] Many other small fixes and improvements.

version 1.0.3 (release 2012-06-19), csync 0.50.7 required
  * [GUI] Added a log window which catches the logging if required and
          allows to save for information.
  * [CMI] Added options --help, --logfile and --logflush
  * [APP] Allow to specify sync frequency in the config file.
  * [Fixes] Do not use csync database files from a sync before.
  * [Fixes] In Connection wizard, write the final config onyl if
            the user really accepted. Also remove the former database.
  * [Fixes] More user expected behaviour deletion of sync folder local
            and remote.
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
    + csync fixes.

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



