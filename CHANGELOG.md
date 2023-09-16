# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [3.8.2] - 2023-05-16

### Added
* Implement missing share settings by @allexzander in https://github.com/nextcloud/desktop/pull/5622

### Changed
* Edit locally: elide loading dialog filename label in the middle by @allexzander in https://github.com/nextcloud/desktop/pull/5612
* Properly handle all fatal errors during edit locally setup procedure by by @claucambra in https://github.com/nextcloud/desktop/pull/5583
* Parse sharees 'lookup' key to include federated sharees by @allexzander in https://github.com/nextcloud/desktop/pull/5613
* No longer override the pixman default version by @mgallien in https://github.com/nextcloud/desktop/pull/5630
* Remove some SQL debug logs to unclutter the output by @allexzander in https://github.com/nextcloud/desktop/pull/5634
* Attempt sign in when an account state is added in AccountManager by @claucambra in https://github.com/nextcloud/desktop/pull/5493

### Fixed
* Fix crash on entering new log file after file size of 512kb reached by @claucambra in https://github.com/nextcloud/desktop/pull/5603
* Fix MacOS UTF-8 normalization issue by @xavi-b in https://github.com/nextcloud/desktop/pull/4957
* Edit locally: fix crash on _chekTokenJob pointer deref by @allexzander in https://github.com/nextcloud/desktop/pull/5637
* E2EE: Fix freeze on metadata checksum validation by @allexzander in https://github.com/nextcloud/desktop/pull/5655
* Fix folder progress bar positioning in account settings on macOS by @claucambra in https://github.com/nextcloud/desktop/pull/5645
* Use addLayout to insert the VFS option when setting account by @camilasan in https://github.com/nextcloud/desktop/pull/5665
* Fix update request channel being set to wrong localised string by @claucambra in https://github.com/nextcloud/desktop/pull/5462
* Fix setting [undefined] to QFont in 'Sync now' button by @claucambra in https://github.com/nextcloud/desktop/pull/5628
* Fix account migration from legacy desktop clients (again) by @claucambra in https://github.com/nextcloud/desktop/pull/5640
* Fix unrecoverable freezing when PutMultiFileJob is used with upload rate limits enabled by @claucambra in https://github.com/nextcloud/desktop/pull/5680

## [3.8.1] - 2023-04-19

### Changed
* Do not display error status and messages when aborting a sync during hydration request in VFS mode by @allexzander in https://github.com/nextcloud/desktop/pull/5579
* In case server has no private key, let e2ee init fail by @mgallien in https://github.com/nextcloud/desktop/pull/5566
* Edit locally: restart current folder sync immediately after file opened by @allexzander in https://github.com/nextcloud/desktop/pull/5588

### Fixed
* Sort encrypted files by their id to compute checksum by @mgallien in https://github.com/nextcloud/desktop/pull/5568
* Try different permutation to recover the broken checksum by @mgallien in https://github.com/nextcloud/desktop/pull/5572
* Fix secure file drop unit tests by @allexzander in https://github.com/nextcloud/desktop/pull/5574
* Always add the item at the end of the layout by @camilasan in https://github.com/nextcloud/desktop/pull/5595
* Properly preserve the format of E2EE metadata during DB operations by @mgallien in https://github.com/nextcloud/desktop/pull/5577

## [3.8.0] - 2023-03-31

### Added
* Secure file drop by @allexzander in https://github.com/nextcloud/desktop/pull/5327
* Multiple bug fixes in E2EE by @mgallien in https://github.com/nextcloud/desktop/pull/5560
* Add Ubuntu Lunar by @ivaradi in https://github.com/nextcloud/desktop/pull/5520

### Changed
* Log to stdout when built in Debug config by @claucambra in https://github.com/nextcloud/desktop/pull/5410

### Fixed
* E2EE cut extra zeroes from derypted byte array by @allexzander in https://github.com/nextcloud/desktop/pull/5534
* Prevent ShareModel crash from accessing bad pointers by @claucambra in https://github.com/nextcloud/desktop/pull/5391
* Show server name in tray main window by @Alkl58 in https://github.com/nextcloud/desktop/pull/5513
* Enter next log file if the current log file is larger than 512 KB by @claucambra in https://github.com/nextcloud/desktop/pull/5580
* Debian build classification 'beta' cannot override 'release' by @ivaradi in https://github.com/nextcloud/desktop/pull/5521
* Follow shouldNotify flag to hide notifications when needed by @mgallien in https://github.com/nextcloud/desktop/pull/5530
* Only accept folder setup page if --overridelocaldir option is set by @camilasan in https://github.com/nextcloud/desktop/pull/5385
* Exit after creating config file when using --overrideserverurl option by @mgallien in https://github.com/nextcloud/desktop/pull/5532
* Respect --overridelocaldir option by @mgallien in https://github.com/nextcloud/desktop/pull/5546
* L10n: Correct word by @Valdnet in https://github.com/nextcloud/desktop/pull/5378
* L10n: Added dot to end of sentence by @rakekniven in https://github.com/nextcloud/desktop/pull/5427
* L10n: Fixed grammar by @rakekniven in https://github.com/nextcloud/desktop/pull/5430

## [3.7.4] - 2023-03-09

### Changed
* Clean up account creation and deletion code by @claucambra in https://github.com/nextcloud/desktop/pull/5416
* CI/clang tidy checks init variables by @mgallien in https://github.com/nextcloud/desktop/pull/5436

### Fixed
* Check German translation for wrong wording by @tobiasKaminsky in https://github.com/nextcloud/desktop/pull/5351
* Fix "Create new folder" menu entries in settings not working correctly on macOS by @claucambra in https://github.com/nextcloud/desktop/pull/5435
* Fix share dialog infinite loading by @claucambra in https://github.com/nextcloud/desktop/pull/5442
* Fix edit locally job not finding the user account: wrong user id by @mgallien in https://github.com/nextcloud/desktop/pull/5444
* Skip e2e encrypted files with empty filename in metadata by @mgallien in https://github.com/nextcloud/desktop/pull/5448
* Always discover blacklisted folders to avoid data loss when modifying selectivesync list by @allexzander in https://github.com/nextcloud/desktop/pull/5459
* Use new connect syntax by @mgallien in https://github.com/nextcloud/desktop/pull/5451
* Add missing flag when dehydrating files with CFAPI by @mgallien in https://github.com/nextcloud/desktop/pull/5474
* Fix avatars not showing up in settings dialog account actions until clicked on by @claucambra in https://github.com/nextcloud/desktop/pull/5453
* Fix text labels in Sync Status component by @claucambra in https://github.com/nextcloud/desktop/pull/5478
* Fix infinite loading in the share dialog when public link shares are disabled on the server by @claucambra in https://github.com/nextcloud/desktop/pull/5472
* Display 'Search globally' as the last sharees list element  by @allexzander in https://github.com/nextcloud/desktop/pull/5485
* Resize WebView widget once the loginpage rendered by @xllndr in https://github.com/nextcloud/desktop/pull/5161
* Fix: do not restore virtual files by @mgallien in https://github.com/nextcloud/desktop/pull/5498
* Fix display of 2FA notification @camilasan in https://github.com/nextcloud/desktop/pull/5486

## [3.7.1] - 2023-02-07

### Fixed
* Init value for pointers by @mgallien in https://github.com/nextcloud/desktop/pull/5393

## [3.7.0] - 2023-02-02

### Added
* Feature: syncjournaldb handle errors by @allexzander in https://github.com/nextcloud/desktop/pull/4819
* Add a placeholder item for empty activity list by @claucambra in https://github.com/nextcloud/desktop/pull/4959
* Configure a list of checks for clang-tidy by @mgallien in https://github.com/nextcloud/desktop/pull/5004
* Feature: VFS windows sharing and lock state by @allexzander in https://github.com/nextcloud/desktop/pull/4942
* Add a 'Sync now' button to the sync status header in the tray window by @claucambra in https://github.com/nextcloud/desktop/pull/5018
* Use new public API to open an edit locally URL by @mgallien in https://github.com/nextcloud/desktop/pull/5116
* Add a new file details window, unify file activity and sharing by @claucambra in https://github.com/nextcloud/desktop/pull/4929
* Add support cmake unity build by @tnixeu in https://github.com/nextcloud/desktop/pull/5109
* Implement context menu entry "Leave this share" by @allexzander in https://github.com/nextcloud/desktop/pull/5081
* Add end-to-end tests to our CI by @claucambra in https://github.com/nextcloud/desktop/pull/5124
* Edit file locally restart sync by @allexzander in https://github.com/nextcloud/desktop/pull/5175
* Add interactive NC Talk notifications on macOS by @claucambra in https://github.com/nextcloud/desktop/pull/5143
* Add an "Encrypt" menu entry in file browser context menu for folders by @claucambra in https://github.com/nextcloud/desktop/pull/5263
* Add a nix flake for easy building and dev environments  by @claucambra in https://github.com/nextcloud/desktop/pull/5007
* Add an internal link share to the share dialog by @claucambra in https://github.com/nextcloud/desktop/pull/5131
* Sets a fixed version for pixman when buildign desktop client via Craft by @mgallien in https://github.com/nextcloud/desktop/pull/5269
* Remove obsolete names by @mgallien in https://github.com/nextcloud/desktop/pull/5271
* Do not sync enc folders if e2ee is not setup by @allexzander in https://github.com/nextcloud/desktop/pull/5258
* Move shellextensions to root installdir by @allexzander in https://github.com/nextcloud/desktop/pull/5295
* Allow setting up an account with apppasword and folder via command-line arguments. For deployment. by @allexzander in https://github.com/nextcloud/desktop/pull/5296
* Allow forceoverrideurl via command line by @allexzander in https://github.com/nextcloud/desktop/pull/5329
* Add ability to disable E2EE by @claucambra in https://github.com/nextcloud/desktop/pull/5167
* Sync with case clash names by @mgallien in https://github.com/nextcloud/desktop/pull/5232

### Changed
* Set UnifiedSearchResultNothingFound visibility less messily by @claucambra in https://github.com/nextcloud/desktop/pull/4751
* Clean up QML type and singleton registration by @claucambra in https://github.com/nextcloud/desktop/pull/4817
* Simplify activity list delegates by making them ItemDelegates, clean up by @claucambra in https://github.com/nextcloud/desktop/pull/4786
* Improve activity list highlighting/keyboard item selection by @claucambra in https://github.com/nextcloud/desktop/pull/4781
* Replace private API QZipWriter with KArchive by @claucambra in https://github.com/nextcloud/desktop/pull/4768
* Makes Qt WebEngine optional only on macOS by @mgallien in https://github.com/nextcloud/desktop/pull/4875
* Updated link to documentation by @BMerz in https://github.com/nextcloud/desktop/pull/4792
* Accept valid lsColJob reply XML content types by @claucambra in https://github.com/nextcloud/desktop/pull/4919
* Refactor ActivityListModel population mechanisms by @claucambra in https://github.com/nextcloud/desktop/pull/4736
* Make account setup wizard's adjustWizardSize resize to current page size instead of largest wizard page by @claucambra in https://github.com/nextcloud/desktop/pull/4911
* Deallocate call notification dialog objects when closed by @claucambra in https://github.com/nextcloud/desktop/pull/4939
* Do not format text in QML components as HTML by @claucambra in https://github.com/nextcloud/desktop/pull/4944
* Ensure strings in main window QML are presented as plain text and not HTML by @claucambra in https://github.com/nextcloud/desktop/pull/4972
* Improve handling of file name clashes by @claucambra in https://github.com/nextcloud/desktop/pull/4970
* Add a QSortFilterProxyModel-based SortedActivityListModel by @claucambra in https://github.com/nextcloud/desktop/pull/4933
* Bring back .lnk files on Windows and always treat them as non-virtual files. by @allexzander in https://github.com/nextcloud/desktop/pull/4968
* Ensure placeholder message in emoji picker wraps correctly by @claucambra in https://github.com/nextcloud/desktop/pull/4960
* Make activity action button an actual button, clean up contents by @claucambra in https://github.com/nextcloud/desktop/pull/4784
* Improve the error box QML component by @claucambra in https://github.com/nextcloud/desktop/pull/4976
* Don't set up tray context menu on macOS, even if not building app bundle by @claucambra in https://github.com/nextcloud/desktop/pull/4988
* CI: check clang tidy in ci by @mgallien in https://github.com/nextcloud/desktop/pull/4995
* Check our code with clang-tidy by @mgallien in https://github.com/nextcloud/desktop/pull/4999
* Alway use constexpr for all text constants by @mgallien in https://github.com/nextcloud/desktop/pull/4996
* Switch AppImage CI to latest tag: client-appimage-6 by @mgallien in https://github.com/nextcloud/desktop/pull/5003
* Apply modernize-use-using via clang-tidy by @mgallien in https://github.com/nextcloud/desktop/pull/4993
* Use [[nodiscard]] by @mgallien in https://github.com/nextcloud/desktop/pull/4992
* Update client image by @camilasan in https://github.com/nextcloud/desktop/pull/5002
* Check the format via some github action by @mgallien in https://github.com/nextcloud/desktop/pull/4991
* Update after tx migrate by @tobiasKaminsky in https://github.com/nextcloud/desktop/pull/5019
* Improve 'Handle local file editing' feature. Add loading popup. Add force sync before opening a file. by @allexzander in https://github.com/nextcloud/desktop/pull/4990
* Do not ignore return value by @mgallien in https://github.com/nextcloud/desktop/pull/4998
* Improve logs when adding sync errors in activity list of main dialog by @mgallien in https://github.com/nextcloud/desktop/pull/5032
* Improve "pretty user name"-related strings, display in webflow credentials by @claucambra in https://github.com/nextcloud/desktop/pull/5013
* Update CMake usage in README build instructions by @NeroBurner in https://github.com/nextcloud/desktop/pull/5086
* Clean up methods in sync engine by @claucambra in https://github.com/nextcloud/desktop/pull/5071
* Make Systray's void methods slots by @claucambra in https://github.com/nextcloud/desktop/pull/5042
* Remove unneeded parameter from CleanupPollsJob constructor by @claucambra in https://github.com/nextcloud/desktop/pull/5070
* Modernise and improve code in AccountManager by @claucambra in https://github.com/nextcloud/desktop/pull/5026
* Validate and sanitise edit locally token and relpath before sending to server by @claucambra in https://github.com/nextcloud/desktop/pull/5093
* Refactor FolderMan's "Edit Locally" capabilities as separate class by @claucambra in https://github.com/nextcloud/desktop/pull/5107
* Modernise and improve code in AccountSettings by @claucambra in https://github.com/nextcloud/desktop/pull/5027
* Remove unused internal link widget from old share dialog by @claucambra in https://github.com/nextcloud/desktop/pull/5123
* Use separate variable for cfg file name in CMAKE. by @allexzander in https://github.com/nextcloud/desktop/pull/5136
* Remove unused app pointer in CocoaInitializer by @claucambra in https://github.com/nextcloud/desktop/pull/5127
* Do not use copy-assignment of QDialog. by @allexzander in https://github.com/nextcloud/desktop/pull/5148
* Remove unused remotePath in User::processCompletedSyncItem by @claucambra in https://github.com/nextcloud/desktop/pull/5118
* Properly escape a path when creating a test file during tests by @mgallien in https://github.com/nextcloud/desktop/pull/5151
* Fully qualify types in signals and slots by @mgallien in https://github.com/nextcloud/desktop/pull/5088
* Switch back to upstream craft by @mgallien in https://github.com/nextcloud/desktop/pull/5178
* Modernize the Dolphin action plugin by @ivaradi in https://github.com/nextcloud/desktop/pull/5192
* CI: do not modify configuration file during tests by @mgallien in https://github.com/nextcloud/desktop/pull/5200
* cmake: Use FindPkgConfig's pkg_get_variable instead of custom macro by @marv in https://github.com/nextcloud/desktop/pull/5199
* Clearly tell user that E2EE has been enabled for an account by @claucambra in https://github.com/nextcloud/desktop/pull/5164
* Remove close/dismiss button from encryption message by @claucambra in https://github.com/nextcloud/desktop/pull/5163
* Update macOS shell integration deployment targets by @claucambra in https://github.com/nextcloud/desktop/pull/5227
* Differentiate between E2EE not being enabled at all vs. E2EE being enabled already through another device in account settings message by @claucambra in https://github.com/nextcloud/desktop/pull/5179
* Ensure more QML text components are rendering things as plain text by @claucambra in https://github.com/nextcloud/desktop/pull/5231
* Make use of plain text-enforcing qml labels by @claucambra in https://github.com/nextcloud/desktop/pull/5233
* Format some QLabels as plain text by @claucambra in https://github.com/nextcloud/desktop/pull/5247
* Do not create GUI from a random thread and show error on real error by @mgallien in https://github.com/nextcloud/desktop/pull/5253
* Only show mnemonic request dialog when user explicitly wants to enable E2EE by @claucambra in https://github.com/nextcloud/desktop/pull/5181
* Replace share settings popup with a page on a StackView by @claucambra in https://github.com/nextcloud/desktop/pull/5194
* Show file details within the tray dialog, rather than in a separate dialog by @claucambra in https://github.com/nextcloud/desktop/pull/5139
* Silence sync termination errors when running EditLocallyJob. by @allexzander in https://github.com/nextcloud/desktop/pull/5261
* Remove unused HeaderBanner component by @claucambra in https://github.com/nextcloud/desktop/pull/5245
* Use QFileInfo::exists where we are only creating a QFileInfo to check if file exists by @claucambra in https://github.com/nextcloud/desktop/pull/5291
* Make correct use of Qt signal 'emit' keyword by @claucambra in https://github.com/nextcloud/desktop/pull/5287
* Remove unused variables by @claucambra in https://github.com/nextcloud/desktop/pull/5290
* Declare all QRegularExpressions statically by @claucambra in https://github.com/nextcloud/desktop/pull/5289
* Improve backup dark mode palette for Windows by @claucambra in https://github.com/nextcloud/desktop/pull/5298
* Replace now deprecated FSEventStreamScheduleWithRunLoop with FSEventStreamSetDispatchQueue by @claucambra in https://github.com/nextcloud/desktop/pull/5272
* Drop dependency on Qt Quick Controls 1 by @Flowdalic in https://github.com/nextcloud/desktop/pull/5309
* Update legal notice to 2023 by @claucambra in https://github.com/nextcloud/desktop/pull/5361
* Don't try to lock folders when editing locally by @claucambra in https://github.com/nextcloud/desktop/pull/5317
* Remove unused monochrome icons setting by @claucambra in https://github.com/nextcloud/desktop/pull/5366
* Always unlock E2EE folders, even when network failure or crash. by @allexzander in https://github.com/nextcloud/desktop/pull/5370
* Improve config upgrade warning dialog by @camilasan in https://github.com/nextcloud/desktop/pull/5384

### Fixed
* Fix wrong estimated time when doing sync. by @allexzander in https://github.com/nextcloud/desktop/pull/4902
* Fix: selective sync abort error by @allexzander in https://github.com/nextcloud/desktop/pull/4903
* Fix: onflict resolution when selecting folder by @allexzander in https://github.com/nextcloud/desktop/pull/4914
* Fix fileactivitylistmodel QML registration by @claucambra in https://github.com/nextcloud/desktop/pull/4920
* Fix menu bar height calculation on macOS by @claucambra in https://github.com/nextcloud/desktop/pull/4917
* Fix ActivityItem activityHover error by @claucambra in https://github.com/nextcloud/desktop/pull/4921
* Fix add account window text clipping, enlarge text by @claucambra in https://github.com/nextcloud/desktop/pull/4910
* Fix low-resolution file changed overlay icons in activities by @claucambra in https://github.com/nextcloud/desktop/pull/4930
* Ensure that the file being processed has had its etag properly sanitised, log etag more by @claucambra in https://github.com/nextcloud/desktop/pull/4940
* Fix two factor authentication notification by @camilasan in https://github.com/nextcloud/desktop/pull/4967
* Fix 'Reply' primary property. by @camilasan in https://github.com/nextcloud/desktop/pull/4985
* Fix sync progress bar colours in dark mode by @claucambra in https://github.com/nextcloud/desktop/pull/4986
* Fix predefined status text formatting by @claucambra in https://github.com/nextcloud/desktop/pull/4987
* Avoid possibly crashing static_cast by @mgallien in https://github.com/nextcloud/desktop/pull/4994
* Fix link shares default expire date being enforced as maximum expire date even when maximum date enforcement is disabled on the server by @claucambra in https://github.com/nextcloud/desktop/pull/4982
* Fix files not unlocking after lock time expired by @claucambra in https://github.com/nextcloud/desktop/pull/4962
* Command-line client. Do not trust SSL certificates by default, unless '--trust' option is set. by @allexzander in https://github.com/nextcloud/desktop/pull/5022
* Fix files lock fail metadata by @mgallien in https://github.com/nextcloud/desktop/pull/5024
* Fix invisible user status selector button not being checked when user is in Offline mode by @claucambra in https://github.com/nextcloud/desktop/pull/5012
* Use correct version copmparison on NSIS updater: fix update from rc by @mgallien in https://github.com/nextcloud/desktop/pull/4979
* Check token for edit locally requests by @mgallien in https://github.com/nextcloud/desktop/pull/5039
* Fix the dismiss button: display it whenever possible. by @camilasan in https://github.com/nextcloud/desktop/pull/4989
* Fix account not found when doing local file editing. by @allexzander in https://github.com/nextcloud/desktop/pull/5040
* Fix call notification dialog buttons by @claucambra in https://github.com/nextcloud/desktop/pull/5074
* Validate certificate for E2EE against private key by @mgallien in https://github.com/nextcloud/desktop/pull/4949
* Emit missing signal to update folder sync status icon by @mgallien in https://github.com/nextcloud/desktop/pull/5087
* Fix macOS autoupdater settings by @claucambra in https://github.com/nextcloud/desktop/pull/5102
* Fix compatibility with newer python3-nautilus by @nteodosio in https://github.com/nextcloud/desktop/pull/5105
* Only show Sync Now button if account is connected by @claucambra in https://github.com/nextcloud/desktop/pull/5097
* E2EE. Do not generate keypair without user request. by @allexzander in https://github.com/nextcloud/desktop/pull/5067
* Fix incorrect current user index when adding or removing a user account. Also fix incorrect user avatar lookup by id. by @allexzander in https://github.com/nextcloud/desktop/pull/5092
* Fix: delete folders during propagation even when propagation has errors by @mgallien in https://github.com/nextcloud/desktop/pull/5104
* Ensure 'Sync now' button doesn't have its text elided by @claucambra in https://github.com/nextcloud/desktop/pull/5129
* Fix share delegate button icon colors in dark mode by @claucambra in https://github.com/nextcloud/desktop/pull/5132
* Make user status selector modal, show user header by @claucambra in https://github.com/nextcloud/desktop/pull/5145
* Fix typo of connector by @hefee in https://github.com/nextcloud/desktop/pull/5157
* Remove reference to inexistent property in NCCustomButton by @claucambra in https://github.com/nextcloud/desktop/pull/5173
* Fix ActivityList delegate warnings by @claucambra in https://github.com/nextcloud/desktop/pull/5172
* Ensure forcing a folder to be synced unpauses syncing on said folder by @claucambra in https://github.com/nextcloud/desktop/pull/5152
* Fix renaming of folders with a deep hierarchy inside them by @mgallien in https://github.com/nextcloud/desktop/pull/5182
* Fix instances of: c++11 range-loop might detach Qt container warnings by @mgallien in https://github.com/nextcloud/desktop/pull/5089
* Fix tray window margins, stop cutting into window border by @claucambra in https://github.com/nextcloud/desktop/pull/5202
* Fix bad custom button alignments, sizings, etc. by @claucambra in https://github.com/nextcloud/desktop/pull/5189
* CI: do not override configuration file by @mgallien in https://github.com/nextcloud/desktop/pull/5206
* Fix CfApiShellExtensionsIPCTest by @allexzander in https://github.com/nextcloud/desktop/pull/5209
* l10n: Fixed grammar by @rakekniven in https://github.com/nextcloud/desktop/pull/5220
* Prevent bad encrypting of folder if E2EE has not been correctly set up by @claucambra in https://github.com/nextcloud/desktop/pull/5223
* Case clash conflicts should not terminate sync by @mgallien in https://github.com/nextcloud/desktop/pull/5224
* l10n: Correct spelling by @Valdnet in https://github.com/nextcloud/desktop/pull/5221
* Fix CI errors for Edit Locally. by @allexzander in https://github.com/nextcloud/desktop/pull/5241
* Lock file when editing locally by @claucambra in https://github.com/nextcloud/desktop/pull/5226
* Fix BasicComboBox internal layout by @claucambra in https://github.com/nextcloud/desktop/pull/5216
* Explicitly size and align user status selector text input to avoid bugs with alternate QtQuick styles by @claucambra in https://github.com/nextcloud/desktop/pull/5214
* So not use bulk upload for e2ee files by @mgallien in https://github.com/nextcloud/desktop/pull/5256
* Avoid the Get-Task-Allow Entitlement (macOS Notarization) by @claucambra in https://github.com/nextcloud/desktop/pull/5274
* Fix migration from old settings configuration files by @mgallien in https://github.com/nextcloud/desktop/pull/5141
* l10n: Remove space by @Valdnet in https://github.com/nextcloud/desktop/pull/5297
* Update file's metadata in the local database when the etag changes while file remains unchanged. Fix subsequent conflict when locking and unlocking. by @allexzander in https://github.com/nextcloud/desktop/pull/5293
* Fix warnings on QPROPERTY-s by @claucambra in https://github.com/nextcloud/desktop/pull/5286
* Fix full-text search results not being opened in browser by @claucambra in https://github.com/nextcloud/desktop/pull/5279
* Fix bad string for translation. by @allexzander in https://github.com/nextcloud/desktop/pull/5358
* Fix migration from legacy client when override server url is set by @claucambra in https://github.com/nextcloud/desktop/pull/5322
* Fix fetch more unified search result item not being clickable by @claucambra in https://github.com/nextcloud/desktop/pull/5266
* Edit locally. Do not lock if locking is disabled on the server. by @allexzander in https://github.com/nextcloud/desktop/pull/5371
* Revert "Merge pull request #5366 from nextcloud/bugfix/remove-mono-icons-setting" by @claucambra in https://github.com/nextcloud/desktop/pull/5372
* Open calendar notifications in the browser. by @camilasan in https://github.com/nextcloud/desktop/pull/4684
* Migrate old configs by @camilasan in https://github.com/nextcloud/desktop/pull/5362
* Fix displaying of file details button for local syncfileitem activities by @claucambra in https://github.com/nextcloud/desktop/pull/5379

### Security
* Validate and sanitise edit locally token and relpath before sending to server by @claucambra in https://github.com/nextcloud/desktop/pull/5093

## [3.6.6] - 2023-01-19

### Fixed
* Revert "Fix(l10n): capital_abcd Update translations from Transifex" by @allexzander in https://github.com/nextcloud/desktop/commit/33f3975529c0c5028c840a4c5ada037d92e12253

## [3.6.5] - 2023-01-19

### Added
* Allow forceoverrideurl via command line by @allexzander in https://github.com/nextcloud/desktop/pull/5329

### Changed
* Drop dependency on Qt Quick Controls 1 by @Flowdalic in https://github.com/nextcloud/desktop/pull/5309

### Fixed
* Do not assert when sharing to a circle by @mgallien in https://github.com/nextcloud/desktop/pull/5310
* Fix macOS shell integration class inits by @claucambra in https://github.com/nextcloud/desktop/pull/5299
* Fix typo by @cgzones in https://github.com/nextcloud/desktop/pull/5257
* Check that we update local file mtime on changes from server by @mgallien in https://github.com/nextcloud/desktop/pull/5188
* Fix regressions on pinState management when doing renames by @mgallien in https://github.com/nextcloud/desktop/pull/5201
* Fix SyncEngineTest failure when localstate is destroyed by @allexzander in https://github.com/nextcloud/desktop/pull/5273

### Security
* Always generate random initialization vector when uploading encrypted file by @allexzander in https://github.com/nextcloud/desktop/pull/5324
* Fix security vulnerability when receiving empty metadataKeys from the server by @allexzander in https://github.com/nextcloud/desktop/pull/5323

## [3.6.4] - 2022-12-08

### Fixed
* Do not create GUI from a random thread and show error on real error by @mgallien in https://github.com/nextcloud/desktop/pull/5253

## [3.6.3] - 2022-12-08

### Added
* Feature: edit file locally restart sync by @allexzander in https://github.com/nextcloud/desktop/pull/5175
* Add forcefoldersync method to folder manager by @claucambra in https://github.com/nextcloud/desktop/pull/5239

### Changed
* Make user status selector modal, show user header by claucambra in https://github.com/nextcloud/desktop/pull/5145
* Make use of plain text-enforcing qml labels by @claucambra in https://github.com/nextcloud/desktop/pull/5233
* Format some QLabels as plain text by @claucambra in https://github.com/nextcloud/desktop/pull/5247

### Fixed
* Fix typo of connector by @hefee in https://github.com/nextcloud/desktop/pull/5157
* Fix renaming of folders with a deep hierarchy inside them by @mgallien in https://github.com/nextcloud/desktop/pull/5182
* Prevent bad encrypting of folder if E2EE has not been correctly set up by @claucambra in https://github.com/nextcloud/desktop/pull/5223
* Lock file when editing locally by @claucambra in
https://github.com/nextcloud/desktop/pull/5226

## [3.6.2] - 2022-11-10

### Added
* Validate and sanitise edit locally token and relpath before sending to server by @claucambra in https://github.com/nextcloud/desktop/pull/5093

### Changed
* Refactor FolderMan's "Edit Locally" capabilities as separate class by @claucambra in https://github.com/nextcloud/desktop/pull/5107
* Use new public API to open an edit locally URL by @mgallien in https://github.com/nextcloud/desktop/pull/5116
* Use separate variable for cfg file name in CMAKE  by @allexzander in https://github.com/nextcloud/desktop/pull/5136
* Do not use copy-assignment of QDialog by @allexzander in https://github.com/nextcloud/desktop/pull/5148

### Fixed
* Fix call notification dialog buttons by @claucambra in https://github.com/nextcloud/desktop/pull/5074
* Emit missing signal to update folder sync status icon by @mgallien in https://github.com/nextcloud/desktop/pull/5087
* Fix macOS autoupdater settings by @claucambra in https://github.com/nextcloud/desktop/pull/5102
* Fix compatibility with newer python3-nautilus by @nteodosio in https://github.com/nextcloud/desktop/pull/5105
* Fix stable-3.6 compile on macOS by @claucambra in https://github.com/nextcloud/desktop/pull/5154
* Fix bad backport of CustomButton changes in Stable-3.6 by @claucambra in https://github.com/nextcloud/desktop/pull/5155
* E2EE: Do not generate keypair without user request by @allexzander in https://github.com/nextcloud/desktop/pull/5067
* Fix incorrect current user index when adding or removing a user account by @allexzander in https://github.com/nextcloud/desktop/pull/5092
* Properly escape a path when creating a test file during tests by @mgallien in https://github.com/nextcloud/desktop/pull/5151

## [3.6.1] - 2022-10-18

### Added

### Changed
* Improve 'Handle local file editing' feature by @mgallien in https://github.com/nextcloud/desktop/pull/5054
* Update after tx migrate by @tobiasKaminsky in https://github.com/nextcloud/desktop/pull/5019
* Bring back .lnk files on Windows and always treat them as non-virtual files by @allexzander in https://github.com/nextcloud/desktop/pull/4968

### Fixed
* Fix two factor auth notification: activity item was disabled by @camilasan in https://github.com/nextcloud/desktop/pull/5057
* Fix account not found when doing local file editing by @mgallien in https://github.com/nextcloud/desktop/pull/5056
* Check token for edit locally requests by @mgallien in https://github.com/nextcloud/desktop/pull/5055
* Fix command-line client: do not trust SSL certificates by default, unless '--trust' option is set by @allexzander in https://github.com/nextcloud/desktop/pull/5022
* Fix invisible user status selector button not being checked when user is in Offline mode by @claucambra in https://github.com/nextcloud/desktop/pull/5012
* Fix the dismiss button: display it whenever possible by @camilasan in https://github.com/nextcloud/desktop/pull/4989
* Fix predefined status text formatting by @claucambra in https://github.com/nextcloud/desktop/pull/4987
* Fix sync progress bar colours in dark mode by @claucambra in https://github.com/nextcloud/desktop/pull/4986
* Fix 'Reply' primary property. by @camilasan in https://github.com/nextcloud/desktop/pull/4985
* Fix link shares default expire date being enforced as maximum expire date even when maximum date enforcement is disabled on the server by @claucambra in https://github.com/nextcloud/desktop/pull/4982
* Use correct version copmparison on NSIS updater: fix update from rc by @mgallien in https://github.com/nextcloud/desktop/pull/4979
* Ensure strings in main window QML are presented as plain text and not HTML by @claucambra in https://github.com/nextcloud/desktop/pull/4972
* Improve handling of file name clashes by @claucambra in https://github.com/nextcloud/desktop/pull/4970
* Ensure placeholder message in emoji picker wraps correctly by @claucambra in https://github.com/nextcloud/desktop/pull/4960
* Do not format text in QML components as HTML by @claucambra in https://github.com/nextcloud/desktop/pull/4944
* Ensure that the file being processed has had its etag properly sanitised, log etag more by @claucambra in https://github.com/nextcloud/desktop/pull/4940
* Deallocate call notification dialog objects when closed by @claucambra in https://github.com/nextcloud/desktop/pull/4939
* Fix low-resolution file changed overlay icons in activities by @claucambra in https://github.com/nextcloud/desktop/pull/4930
* Accept valid lsColJob reply XML content types by @claucambra in https://github.com/nextcloud/desktop/pull/4919
* Fix menu bar height calculation on macOS by @claucambra in https://github.com/nextcloud/desktop/pull/4917
* Bugfix/conflict resolution when selecting folder by @allexzander in https://github.com/nextcloud/desktop/pull/4914
* Fix add account window text clipping, enlarge text by @claucambra in https://github.com/nextcloud/desktop/pull/4910
* Fix selective sync abort error by @allexzander in https://github.com/nextcloud/desktop/pull/4903
* Fix wrong estimated time when doing sync by @allexzander in https://github.com/nextcloud/desktop/pull/4902

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
[3.8.2]: https://github.com/nextcloud/desktop/compare/v3.8.1...v3.8.2
[3.8.1]: https://github.com/nextcloud/desktop/compare/v3.8.0...v3.8.1
[3.8.0]: https://github.com/nextcloud/desktop/compare/v3.7.4...v3.8.0
[3.7.4]: https://github.com/nextcloud/desktop/compare/v3.7.1...v3.7.4
[3.7.1]: https://github.com/nextcloud/desktop/compare/v3.7.0...v3.7.1
[3.7.0]: https://github.com/nextcloud/desktop/compare/v3.6.6...v3.7.0
[3.6.6]: https://github.com/nextcloud/desktop/compare/v3.6.5...v3.6.6
[3.6.5]: https://github.com/nextcloud/desktop/compare/v3.6.4...v3.6.5
[3.6.4]: https://github.com/nextcloud/desktop/compare/v3.6.3...v3.6.4
[3.6.3]: https://github.com/nextcloud/desktop/compare/v3.6.2...v3.6.3
[3.6.2]: https://github.com/nextcloud/desktop/compare/v3.6.1...v3.6.2
[3.6.1]: https://github.com/nextcloud/desktop/compare/v3.6.0...v3.6.1
[3.6.0]: https://github.com/nextcloud/desktop/compare/v3.6.0-rc1...v3.6.0
[3.6.0-rc1]: https://github.com/nextcloud/desktop/compare/v3.5.0...v3.6.0-rc1
[legacy]: https://github.com/nextcloud/desktop/blob/master/ChangeLog%20-%20Legacy
