# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [3.6.0] - 2022-09-06

### Added
* Enable more warnings also for gcc by @mgallien in https://github.com/nextcloud/desktop/pull/4804
* Make UserStatusSelector a dismissible page pushed onto the tray window by @claucambra in https://github.com/nextcloud/desktop/pull/4760
* Add Debian Bullseye build by @ivaradi in https://github.com/nextcloud/desktop/pull/4773
* Handle edit locally from web by @allexzander in https://github.com/nextcloud/desktop/pull/4771
* Double-clicking tray icon opens currently-selected user's local folder (if available) by @claucambra in https://github.com/nextcloud/desktop/pull/4774
* Virtual file system Windows thumbnails by @allexzander in https://github.com/nextcloud/desktop/pull/4713
* Use macOS-specific application icon by @claucambra in https://github.com/nextcloud/desktop/pull/4707
* Limit concurrent notifications by @claucambra in https://github.com/nextcloud/desktop/pull/4706
* Add a placeholder message for the recents tab of the emoji picker by @claucambra in https://github.com/nextcloud/desktop/pull/4628
* Add a custom back button to the account wizard's advanced setup page by @claucambra in https://github.com/nextcloud/desktop/pull/4686
* Add SVG icon styled for macOS Big Sur by @elsiehupp in https://github.com/nextcloud/desktop/pull/4631
* Enable Windows CI unit tests and test coverage by @allexzander in https://github.com/nextcloud/desktop/pull/4609
* Prevent call dialogs from being presented when do not disturb is set as the user status by @claucambra in https://github.com/nextcloud/desktop/pull/4611
* Generate state icons from svg by @allexzander in https://github.com/nextcloud/desktop/pull/4622
* Ignore Office temp folders on Mac ('.sb-' in folder name). by @allexzander in https://github.com/nextcloud/desktop/pull/4615
* Display chat message inside the OS notification. by @allexzander in https://github.com/nextcloud/desktop/pull/4575
* Bump up minimum macOS version by @claucambra in https://github.com/nextcloud/desktop/pull/4564
* Add new fixup workflow from nextcloud org by @mgallien in https://github.com/nextcloud/desktop/pull/4560

### Changed
* Ensure file activity dialog is centered on screen and appears at top of window stack by @claucambra in https://github.com/nextcloud/desktop/pull/4853
* Do not build qt keychain already included in the CI images by @mgallien in https://github.com/nextcloud/desktop/pull/4882
* Reads and store fileId and remote permissions during bulk upload by @mgallien in https://github.com/nextcloud/desktop/pull/4869
* Refactor user line by @claucambra in https://github.com/nextcloud/desktop/pull/4797
* Eliminate padding around the menu separator in the account menu by @claucambra in https://github.com/nextcloud/desktop/pull/4802
* Restyle unified search skeleton items animation and simplify their code by @claucambra in https://github.com/nextcloud/desktop/pull/4718
* Clean up TalkReplyTextField, remove unnecessary parent Item by @claucambra in https://github.com/nextcloud/desktop/pull/4790
* Clicking on an activity list item for a file opens the local file if available by @claucambra in https://github.com/nextcloud/desktop/pull/4748
* Switch to using the main client CI image based on ubuntu 22.04 by @mgallien in https://github.com/nextcloud/desktop/pull/4704
* Always run MSI with full UI. by @allexzander in https://github.com/nextcloud/desktop/pull/4801
* Replace unified search text field busy indicator with custom indicator by @claucambra in https://github.com/nextcloud/desktop/pull/4753
* Make apps menu scrollable when content taller than available vertical space, preventing borking of layout by @claucambra in https://github.com/nextcloud/desktop/pull/4723
* Remove Ubuntu Impish, add Kinetic by @ivaradi in https://github.com/nextcloud/desktop/pull/4758
* Stop styling QML unified search items hierarchically, use global Style constants by @claucambra in https://github.com/nextcloud/desktop/pull/4719
* print sync direction in SyncFileStatusTracker::slotAboutToPropagate by @mgallien in https://github.com/nextcloud/desktop/pull/4679
* Use preprocessor directive rather than normal 'if' for UNNotification types by @claucambra in https://github.com/nextcloud/desktop/pull/4720
* QML-ify the UserModel, use properties rather than setter methods by @claucambra in https://github.com/nextcloud/desktop/pull/4710
* Take ints by value rather than reference in UserModel methods by @claucambra in https://github.com/nextcloud/desktop/pull/4712
* Refactor tray window opening code for clarity and efficiency by @claucambra in https://github.com/nextcloud/desktop/pull/4688
* Properly adapt the UserStatusSelectorModel to QML, eliminate hacks, make code more declarative by @claucambra in https://github.com/nextcloud/desktop/pull/4650
* Clean up systray methods, make more QML-friendly by @claucambra in https://github.com/nextcloud/desktop/pull/4687
* Add 'db/local/remote' reference to log string. by @camilasan in https://github.com/nextcloud/desktop/pull/4683
* Work around issues with window positioning on Linux DEs, hardcode tray window to screen center when new account added by @claucambra in https://github.com/nextcloud/desktop/pull/4685
* Increase the call state checking interval to not overload the server by @claucambra in https://github.com/nextcloud/desktop/pull/4693
* Use an en-dash for the userstatus panel by @szaimen in https://github.com/nextcloud/desktop/pull/4671
* Windows CI. Use specific Craft revision. by @allexzander in https://github.com/nextcloud/desktop/pull/4682
* Reply button size should be same as the input field, smaller + text color by @camilasan in https://github.com/nextcloud/desktop/pull/4577
* Make user status dialog look in line with the rest of the desktop client tray and Nextcloud by @claucambra in https://github.com/nextcloud/desktop/pull/4624
* Make client language gender-neutral and more clear by @claucambra in https://github.com/nextcloud/desktop/pull/4667
* Make the share dialog resizeable by @claucambra in https://github.com/nextcloud/desktop/pull/4663
* Redesign local folder information in the account-adding wizard by @claucambra in https://github.com/nextcloud/desktop/pull/4638
* Remove tooltip because it is only repeating the label of the link. by @camilasan in https://github.com/nextcloud/desktop/pull/4657
* Fix general section by @jospoortvliet in https://github.com/nextcloud/desktop/pull/4439
* Ensure call notification stays on top of other windows by @claucambra in https://github.com/nextcloud/desktop/pull/4659
* Rephrase login dialog button text to be in line with clients on other platforms by @claucambra in https://github.com/nextcloud/desktop/pull/4637
* Add a transparent background to the send reply button. by @camilasan in https://github.com/nextcloud/desktop/pull/4578
* Reduce spacing above the buttons: spacing should be same as space between lines in the text above by @camilasan in https://github.com/nextcloud/desktop/pull/4572
* Update autoupdater doc with info about the macOS autoupdater by @claucambra in https://github.com/nextcloud/desktop/pull/4587
* Add explicit capture for lambda by @mgallien in https://github.com/nextcloud/desktop/pull/4553
* Change three dots to an ellipsis and add a space by @Valdnet in https://github.com/nextcloud/desktop/pull/4551
* Simplify and remove the notification "cache" by @claucambra in https://github.com/nextcloud/desktop/pull/4508
* Use proper online status for user ('dnd', 'online', 'invisible', etc.) to enable or disable desktop notifications. by @allexzander in https://github.com/nextcloud/desktop/pull/4507
* Do not replace strings in action links coming from the notification api. by @camilasan in https://github.com/nextcloud/desktop/pull/4522
* Revamp notifications for macOS and add support for actionable update notifications by @claucambra in https://github.com/nextcloud/desktop/pull/4512
* Make the make_universal.py script more verbose for easier debugging by @claucambra in https://github.com/nextcloud/desktop/pull/4501
* docs: Replace "preceded" with "followed" by @carlcsaposs in https://github.com/nextcloud/desktop/pull/4249
* Remove "…" from "Create Debug Archive" button by @spacegaier in https://github.com/nextcloud/desktop/pull/4380


### Fixed
* Prevent the 'Cancel' button of the user status selector getting squashed by @claucambra in https://github.com/nextcloud/desktop/pull/4843
* Ensure that clear status message combo box is at least implicit width by @claucambra in https://github.com/nextcloud/desktop/pull/4844
* Fix alignment of predefined status contents regardless of emoji fonts by @claucambra in https://github.com/nextcloud/desktop/pull/4845
* Prevent crashing when trying to create error-ing QML component in systray.cpp, output error to log by @mgallien in  https://github.com/nextcloud/desktop/pull/4850
* Build script for AppImage should not assume Nextcloud is the name by @mgallien in  https://github.com/nextcloud/desktop/pull/4866
* Fix File Activities dialog not showing up by @allexzander in https://github.com/nextcloud/desktop/pull/4867
* Fix account switching and hover issues with UserLine component by @claucambra in https://github.com/nextcloud/desktop/pull/4839
* Fix unified search item placeholder image source by @claucambra in https://github.com/nextcloud/desktop/pull/4831
* Fix greek translation for application name in menu by @gapan in https://github.com/nextcloud/desktop/pull/4827
* Remove libglib-2.0.so.0 and libgobject-2.0.so.0 from Appimage. by @camilasan in https://github.com/nextcloud/desktop/pull/4830
* Fix QML warnings by @claucambra in https://github.com/nextcloud/desktop/pull/4818
* Fix bugs with setting 'Away' user status by @claucambra in https://github.com/nextcloud/desktop/pull/4822
* ensure SyncEngine use an initialized instance of SyncOptions by @mgallien in https://github.com/nextcloud/desktop/pull/4816
* Fix crash: 'Failed to create OpenGL context'. by @allexzander in https://github.com/nextcloud/desktop/pull/4821
* i18n: Spelling unification by @Valdnet in https://github.com/nextcloud/desktop/pull/4820
* Ensure that throttled notifications still appear in tray activity model by @claucambra in https://github.com/nextcloud/desktop/pull/4734
* Do not reboot PC when running an MSI via autoupdate. by @allexzander in https://github.com/nextcloud/desktop/pull/4799
* Update macOS Info.plist by @claucambra in https://github.com/nextcloud/desktop/pull/4755
* Ensure debug archive contents are readable by any user by @claucambra in https://github.com/nextcloud/desktop/pull/4756
* Stop clearing notifications when new notifications are received by @claucambra in https://github.com/nextcloud/desktop/pull/4735
* Fix ActivityItemContent QML paintedWidth errors by @claucambra in https://github.com/nextcloud/desktop/pull/4738
* Respect skipAutoUpdateCheck in nextcloud.cfg with Sparkle on macOS by @claucambra in https://github.com/nextcloud/desktop/pull/4717
* Only set _FORTIFY_SOURCE when a higher level of this flag has not been set by @claucambra in https://github.com/nextcloud/desktop/pull/4703
* Fix bad quote in CMakeLists PNG generation message by @claucambra in https://github.com/nextcloud/desktop/pull/4700
* Ensure the dispatch source only gets deallocated after the dispatch_source_cancel is done, avoiding crashing of the Finder Sync Extension on macOS by @claucambra in https://github.com/nextcloud/desktop/pull/4643
* Close call notifications when the call has been joined by the user, or the call has ended by @claucambra in https://github.com/nextcloud/desktop/pull/4672
* Correct spelling by @Valdnet in https://github.com/nextcloud/desktop/pull/4678
* Fix the system tray menu not being correctly replaced in setupContextMenu on GNOME by @claucambra in https://github.com/nextcloud/desktop/pull/4655
* Fix crashing when selecting user status and predefined statuses not appearing by @claucambra in https://github.com/nextcloud/desktop/pull/4616
* Force OpenGL via Angle and using warp direct3d software rasterizer by @mgallien in https://github.com/nextcloud/desktop/pull/4582
* Fix for the share dialog: mode.absolutePath being undefined prevented the share dialog from being opened by the user. by @camilasan in https://github.com/nextcloud/desktop/pull/4640
* Add contrast to the text/icon of buttons if the server defined color is light. by @camilasan in https://github.com/nextcloud/desktop/pull/4641
* Fix segfault when _transferDataSocket is nullptr. by @camilasan in https://github.com/nextcloud/desktop/pull/4656
* Remove assert from test, it is no longer useful. by @camilasan in https://github.com/nextcloud/desktop/pull/4645
* Fix building the client on macOS without the application bundle by @claucambra in https://github.com/nextcloud/desktop/pull/4612
* Fix build on macOS versions pre-11 (down to 10.14) by @claucambra in https://github.com/nextcloud/desktop/pull/4563
* l10n: Fixed grammar by @rakekniven in https://github.com/nextcloud/desktop/pull/4495
* Fix 'TypeError: Cannot readproperty 'messageSent' of undefined'. by @camilasan in https://github.com/nextcloud/desktop/pull/4573
* Fix crash caused by overflow in FinderSyncExtension by @claucambra in https://github.com/nextcloud/desktop/pull/4562
* Explicitly ask user for notification authorisation on launch (macOS) by @claucambra in https://github.com/nextcloud/desktop/pull/4556
* Stretch WebView to fit dialog's height. by @allexzander in https://github.com/nextcloud/desktop/pull/4554
* Add and use DO_NOT_REBOOT_IN_SILENT=1 parameter for MSI to not reboot during the auto-update. by @allexzander in https://github.com/nextcloud/desktop/pull/4566
* Fix visual borking in the share dialog by @claucambra in https://github.com/nextcloud/desktop/pull/4540
* Fix two factor authentication notification: 'Mark as read' was being displayed in both action buttons. by @camilasan in https://github.com/nextcloud/desktop/pull/4518
* If an exclude file is deleted, skip it and remove it from internal list by @mgallien in https://github.com/nextcloud/desktop/pull/4519
* Fixed share link expiration box being ineditable and always attempting to set invalid date by @claucambra in https://github.com/nextcloud/desktop/pull/4543
* Fix: allow manual rename files with spaces by @allexzander in https://github.com/nextcloud/desktop/pull/4454
* Fix activity list item issues with colours/layout/etc. by @claucambra in https://github.com/nextcloud/desktop/pull/4472
* Fix tray icon not displaying "Open main dialog" by @claucambra in https://github.com/nextcloud/desktop/pull/4484
* Fix: take root folder's files size into account when displaying the total size in selective sync dialog. by @allexzander in https://github.com/nextcloud/desktop/pull/4532
* Fix crashing of finder sync extension caused by dispatch_source_cancel of nullptr by @claucambra in https://github.com/nextcloud/desktop/pull/4520
* Ask for Desktop Client version by @solracsf in https://github.com/nextcloud/desktop/pull/4499
* Only add OCS-APIREQUEST header for 1st request of webflow v1 by @mgallien in https://github.com/nextcloud/desktop/pull/4510
* Use full-bleed Start Tile by @elsiehupp in https://github.com/nextcloud/desktop/pull/2982
* l10n: Remove string from translation by @rakekniven in https://github.com/nextcloud/desktop/pull/4473
* Add new and correct sparkle update signature by @claucambra in https://github.com/nextcloud/desktop/pull/4478
* Ensure cache is stored in default cache location by @claucambra in https://github.com/nextcloud/desktop/pull/4485
* l10n: Changed triple dot to ellipsis by @rakekniven in https://github.com/nextcloud/desktop/pull/4469
* Move URI scheme variable from Nextcloud.cmake to root CMakeListsts. by @allexzander in https://github.com/nextcloud/desktop/pull/4815
* Move CFAPI shell extensions variables to root CMakeLists. by @allexzander in https://github.com/nextcloud/desktop/pull/4810]


## [3.6.0-rc1] - 2022-08-16

### Added
* Enable more warnings also for gcc by @mgallien in https://github.com/nextcloud/desktop/pull/4804
* Make UserStatusSelector a dismissible page pushed onto the tray window by @claucambra in https://github.com/nextcloud/desktop/pull/4760
* Add Debian Bullseye build by @ivaradi in https://github.com/nextcloud/desktop/pull/4773
* Handle edit locally from web by @allexzander in https://github.com/nextcloud/desktop/pull/4771
* Double-clicking tray icon opens currently-selected user's local folder (if available) by @claucambra in https://github.com/nextcloud/desktop/pull/4774
* Virtual file system Windows thumbnails by @allexzander in https://github.com/nextcloud/desktop/pull/4713
* Use macOS-specific application icon by @claucambra in https://github.com/nextcloud/desktop/pull/4707
* Limit concurrent notifications by @claucambra in https://github.com/nextcloud/desktop/pull/4706
* Add a placeholder message for the recents tab of the emoji picker by @claucambra in https://github.com/nextcloud/desktop/pull/4628
* Add a custom back button to the account wizard's advanced setup page by @claucambra in https://github.com/nextcloud/desktop/pull/4686
* Add SVG icon styled for macOS Big Sur by @elsiehupp in https://github.com/nextcloud/desktop/pull/4631
* Enable Windows CI unit tests and test coverage by @allexzander in https://github.com/nextcloud/desktop/pull/4609
* Prevent call dialogs from being presented when do not disturb is set as the user status by @claucambra in https://github.com/nextcloud/desktop/pull/4611
* Generate state icons from svg by @allexzander in https://github.com/nextcloud/desktop/pull/4622
* Ignore Office temp folders on Mac ('.sb-' in folder name). by @allexzander in https://github.com/nextcloud/desktop/pull/4615
* Display chat message inside the OS notification. by @allexzander in https://github.com/nextcloud/desktop/pull/4575
* Bump up minimum macOS version by @claucambra in https://github.com/nextcloud/desktop/pull/4564
* Add new fixup workflow from nextcloud org by @mgallien in https://github.com/nextcloud/desktop/pull/4560

### Changed
* Refactor user line by @claucambra in https://github.com/nextcloud/desktop/pull/4797
* Eliminate padding around the menu separator in the account menu by @claucambra in https://github.com/nextcloud/desktop/pull/4802
* Restyle unified search skeleton items animation and simplify their code by @claucambra in https://github.com/nextcloud/desktop/pull/4718
* Clean up TalkReplyTextField, remove unnecessary parent Item by @claucambra in https://github.com/nextcloud/desktop/pull/4790
* Clicking on an activity list item for a file opens the local file if available by @claucambra in https://github.com/nextcloud/desktop/pull/4748
* Switch to using the main client CI image based on ubuntu 22.04 by @mgallien in https://github.com/nextcloud/desktop/pull/4704
* Always run MSI with full UI. by @allexzander in https://github.com/nextcloud/desktop/pull/4801
* Replace unified search text field busy indicator with custom indicator by @claucambra in https://github.com/nextcloud/desktop/pull/4753
* Make apps menu scrollable when content taller than available vertical space, preventing borking of layout by @claucambra in https://github.com/nextcloud/desktop/pull/4723
* Remove Ubuntu Impish, add Kinetic by @ivaradi in https://github.com/nextcloud/desktop/pull/4758
* Stop styling QML unified search items hierarchically, use global Style constants by @claucambra in https://github.com/nextcloud/desktop/pull/4719
* print sync direction in SyncFileStatusTracker::slotAboutToPropagate by @mgallien in https://github.com/nextcloud/desktop/pull/4679
* Use preprocessor directive rather than normal 'if' for UNNotification types by @claucambra in https://github.com/nextcloud/desktop/pull/4720
* QML-ify the UserModel, use properties rather than setter methods by @claucambra in https://github.com/nextcloud/desktop/pull/4710
* Take ints by value rather than reference in UserModel methods by @claucambra in https://github.com/nextcloud/desktop/pull/4712
* Refactor tray window opening code for clarity and efficiency by @claucambra in https://github.com/nextcloud/desktop/pull/4688
* Properly adapt the UserStatusSelectorModel to QML, eliminate hacks, make code more declarative by @claucambra in https://github.com/nextcloud/desktop/pull/4650
* Clean up systray methods, make more QML-friendly by @claucambra in https://github.com/nextcloud/desktop/pull/4687
* Add 'db/local/remote' reference to log string. by @camilasan in https://github.com/nextcloud/desktop/pull/4683
* Work around issues with window positioning on Linux DEs, hardcode tray window to screen center when new account added by @claucambra in https://github.com/nextcloud/desktop/pull/4685
* Increase the call state checking interval to not overload the server by @claucambra in https://github.com/nextcloud/desktop/pull/4693
* Use an en-dash for the userstatus panel by @szaimen in https://github.com/nextcloud/desktop/pull/4671
* Windows CI. Use specific Craft revision. by @allexzander in https://github.com/nextcloud/desktop/pull/4682
* Reply button size should be same as the input field, smaller + text color by @camilasan in https://github.com/nextcloud/desktop/pull/4577
* Make user status dialog look in line with the rest of the desktop client tray and Nextcloud by @claucambra in https://github.com/nextcloud/desktop/pull/4624
* Make client language gender-neutral and more clear by @claucambra in https://github.com/nextcloud/desktop/pull/4667
* Make the share dialog resizeable by @claucambra in https://github.com/nextcloud/desktop/pull/4663
* Redesign local folder information in the account-adding wizard by @claucambra in https://github.com/nextcloud/desktop/pull/4638
* Remove tooltip because it is only repeating the label of the link. by @camilasan in https://github.com/nextcloud/desktop/pull/4657
* Fix general section by @jospoortvliet in https://github.com/nextcloud/desktop/pull/4439
* Ensure call notification stays on top of other windows by @claucambra in https://github.com/nextcloud/desktop/pull/4659
* Rephrase login dialog button text to be in line with clients on other platforms by @claucambra in https://github.com/nextcloud/desktop/pull/4637
* Add a transparent background to the send reply button. by @camilasan in https://github.com/nextcloud/desktop/pull/4578
* Reduce spacing above the buttons: spacing should be same as space between lines in the text above by @camilasan in https://github.com/nextcloud/desktop/pull/4572
* Update autoupdater doc with info about the macOS autoupdater by @claucambra in https://github.com/nextcloud/desktop/pull/4587
* Add explicit capture for lambda by @mgallien in https://github.com/nextcloud/desktop/pull/4553
* Change three dots to an ellipsis and add a space by @Valdnet in https://github.com/nextcloud/desktop/pull/4551
* Simplify and remove the notification "cache" by @claucambra in https://github.com/nextcloud/desktop/pull/4508
* Use proper online status for user ('dnd', 'online', 'invisible', etc.) to enable or disable desktop notifications. by @allexzander in https://github.com/nextcloud/desktop/pull/4507
* Do not replace strings in action links coming from the notification api. by @camilasan in https://github.com/nextcloud/desktop/pull/4522
* Revamp notifications for macOS and add support for actionable update notifications by @claucambra in https://github.com/nextcloud/desktop/pull/4512
* Make the make_universal.py script more verbose for easier debugging by @claucambra in https://github.com/nextcloud/desktop/pull/4501
* docs: Replace "preceded" with "followed" by @carlcsaposs in https://github.com/nextcloud/desktop/pull/4249
* Remove "…" from "Create Debug Archive" button by @spacegaier in https://github.com/nextcloud/desktop/pull/4380

### Fixed
* Fix account switching and hover issues with UserLine component by @claucambra in https://github.com/nextcloud/desktop/pull/4839
* Fix unified search item placeholder image source by @claucambra in https://github.com/nextcloud/desktop/pull/4831
* Fix greek translation for application name in menu by @gapan in https://github.com/nextcloud/desktop/pull/4827
* Remove libglib-2.0.so.0 and libgobject-2.0.so.0 from Appimage. by @camilasan in https://github.com/nextcloud/desktop/pull/4830
* Fix QML warnings by @claucambra in https://github.com/nextcloud/desktop/pull/4818
* Fix bugs with setting 'Away' user status by @claucambra in https://github.com/nextcloud/desktop/pull/4822
* ensure SyncEngine use an initialized instance of SyncOptions by @mgallien in https://github.com/nextcloud/desktop/pull/4816
* Fix crash: 'Failed to create OpenGL context'. by @allexzander in https://github.com/nextcloud/desktop/pull/4821
* i18n: Spelling unification by @Valdnet in https://github.com/nextcloud/desktop/pull/4820
* Ensure that throttled notifications still appear in tray activity model by @claucambra in https://github.com/nextcloud/desktop/pull/4734
* Do not reboot PC when running an MSI via autoupdate. by @allexzander in https://github.com/nextcloud/desktop/pull/4799
* Update macOS Info.plist by @claucambra in https://github.com/nextcloud/desktop/pull/4755
* Ensure debug archive contents are readable by any user by @claucambra in https://github.com/nextcloud/desktop/pull/4756
* Stop clearing notifications when new notifications are received by @claucambra in https://github.com/nextcloud/desktop/pull/4735
* Fix ActivityItemContent QML paintedWidth errors by @claucambra in https://github.com/nextcloud/desktop/pull/4738
* Respect skipAutoUpdateCheck in nextcloud.cfg with Sparkle on macOS by @claucambra in https://github.com/nextcloud/desktop/pull/4717
* Only set _FORTIFY_SOURCE when a higher level of this flag has not been set by @claucambra in https://github.com/nextcloud/desktop/pull/4703
* Fix bad quote in CMakeLists PNG generation message by @claucambra in https://github.com/nextcloud/desktop/pull/4700
* Ensure the dispatch source only gets deallocated after the dispatch_source_cancel is done, avoiding crashing of the Finder Sync Extension on macOS by @claucambra in https://github.com/nextcloud/desktop/pull/4643
* Close call notifications when the call has been joined by the user, or the call has ended by @claucambra in https://github.com/nextcloud/desktop/pull/4672
* Correct spelling by @Valdnet in https://github.com/nextcloud/desktop/pull/4678
* Fix the system tray menu not being correctly replaced in setupContextMenu on GNOME by @claucambra in https://github.com/nextcloud/desktop/pull/4655
* Fix crashing when selecting user status and predefined statuses not appearing by @claucambra in https://github.com/nextcloud/desktop/pull/4616
* Force OpenGL via Angle and using warp direct3d software rasterizer by @mgallien in https://github.com/nextcloud/desktop/pull/4582
* Fix for the share dialog: mode.absolutePath being undefined prevented the share dialog from being opened by the user. by @camilasan in https://github.com/nextcloud/desktop/pull/4640
* Add contrast to the text/icon of buttons if the server defined color is light. by @camilasan in https://github.com/nextcloud/desktop/pull/4641
* Fix segfault when _transferDataSocket is nullptr. by @camilasan in https://github.com/nextcloud/desktop/pull/4656
* Remove assert from test, it is no longer useful. by @camilasan in https://github.com/nextcloud/desktop/pull/4645
* Fix building the client on macOS without the application bundle by @claucambra in https://github.com/nextcloud/desktop/pull/4612
* Fix build on macOS versions pre-11 (down to 10.14) by @claucambra in https://github.com/nextcloud/desktop/pull/4563
* l10n: Fixed grammar by @rakekniven in https://github.com/nextcloud/desktop/pull/4495
* Fix 'TypeError: Cannot readproperty 'messageSent' of undefined'. by @camilasan in https://github.com/nextcloud/desktop/pull/4573
* Fix crash caused by overflow in FinderSyncExtension by @claucambra in https://github.com/nextcloud/desktop/pull/4562
* Explicitly ask user for notification authorisation on launch (macOS) by @claucambra in https://github.com/nextcloud/desktop/pull/4556
* Stretch WebView to fit dialog's height. by @allexzander in https://github.com/nextcloud/desktop/pull/4554
* Add and use DO_NOT_REBOOT_IN_SILENT=1 parameter for MSI to not reboot during the auto-update. by @allexzander in https://github.com/nextcloud/desktop/pull/4566
* Fix visual borking in the share dialog by @claucambra in https://github.com/nextcloud/desktop/pull/4540
* Fix two factor authentication notification: 'Mark as read' was being displayed in both action buttons. by @camilasan in https://github.com/nextcloud/desktop/pull/4518
* If an exclude file is deleted, skip it and remove it from internal list by @mgallien in https://github.com/nextcloud/desktop/pull/4519
* Fixed share link expiration box being ineditable and always attempting to set invalid date by @claucambra in https://github.com/nextcloud/desktop/pull/4543
* Fix: allow manual rename files with spaces by @allexzander in https://github.com/nextcloud/desktop/pull/4454
* Fix activity list item issues with colours/layout/etc. by @claucambra in https://github.com/nextcloud/desktop/pull/4472
* Fix tray icon not displaying "Open main dialog" by @claucambra in https://github.com/nextcloud/desktop/pull/4484
* Fix: take root folder's files size into account when displaying the total size in selective sync dialog. by @allexzander in https://github.com/nextcloud/desktop/pull/4532
* Fix crashing of finder sync extension caused by dispatch_source_cancel of nullptr by @claucambra in https://github.com/nextcloud/desktop/pull/4520
* Ask for Desktop Client version by @solracsf in https://github.com/nextcloud/desktop/pull/4499
* Only add OCS-APIREQUEST header for 1st request of webflow v1 by @mgallien in https://github.com/nextcloud/desktop/pull/4510
* Use full-bleed Start Tile by @elsiehupp in https://github.com/nextcloud/desktop/pull/2982
* l10n: Remove string from translation by @rakekniven in https://github.com/nextcloud/desktop/pull/4473
* Add new and correct sparkle update signature by @claucambra in https://github.com/nextcloud/desktop/pull/4478
* Ensure cache is stored in default cache location by @claucambra in https://github.com/nextcloud/desktop/pull/4485
* l10n: Changed triple dot to ellipsis by @rakekniven in https://github.com/nextcloud/desktop/pull/4469
* Move URI scheme variable from Nextcloud.cmake to root CMakeListsts. by @allexzander in https://github.com/nextcloud/desktop/pull/4815
* Move CFAPI shell extensions variables to root CMakeLists. by @allexzander in https://github.com/nextcloud/desktop/pull/4810

## [ChangeLog - Legacy][legacy]

[3.6.0]: https://github.com/nextcloud/desktop/compare/v3.6.0-rc1...v3.6.0
[3.6.0-rc1]: https://github.com/nextcloud/desktop/compare/v3.5.0...v3.6.0-rc1
[legacy]: https://github.com/nextcloud/desktop/blob/master/ChangeLog%20-%20Legacy