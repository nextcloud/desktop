## Reason

* Needed for branded client X.x.x release (link to branded release ticket)
* ...

### Template
[Patch Release Template](https://github.com/owncloud/client/blob/master/.github/patch_release_template.md)

### Create Changelog

* [ ] DEV: Update [ChangeLog](https://confluence.owncloud.com/display/OG/ChangeLog)

### Prerequisites

* [ ] QA: Update [Test Plans](https://confluence.owncloud.com/display/OG/Test+Plan+Update) 
* [ ] QA: Update [documentation](https://confluence.owncloud.com/display/OG/Documentation)
* [ ] QA: Check the translations coming from [Transifex](https://confluence.owncloud.com/display/OG/Online+Updater%2C+Crash+reporter%2C+Transifex#OnlineUpdater,Crashreporter,Transifex-Transifex)
* [ ] DEV: Check [dependencies](https://confluence.owncloud.com/display/OG/Dependencies) for updates
* [ ] DEV: Prepare the release in a `X.x` version branch
* [ ] DEV: bump VERSION.cmake in master to say 2.(x+1).0 unless already done.

### Build

* [ ] QA: Create [builds](https://confluence.owncloud.com/display/OG/Build+and+Tags#BuildandTags-Sprintbuild) for theme 'ownCloud' and 'testpilotcloud'
* [ ] QA: Check the new subfolder of [testing download](https://download.owncloud.com/desktop/ownCloud/testing/) if `*tar.xz.asc` files are there. If not follow the [instructions](https://github.com/owncloud/enterprise/wiki/Desktop-Signing-Knowledge)
* [ ] Beta/RC Communication (https://confluence.owncloud.com/display/OG/Marketing+and+Communication)
   * [ ] Website links for beta (needed for the following posts)
   * [ ] Central post https://central.owncloud.org/tags/c/news/desktop
   * [ ] All other stakeholders


### QA

#### DEV QA
* [ ] DEV: [Smoke test](https://confluence.owncloud.com/display/OG/Manual+Tests#ManualTests-DEVSmokeTest)
* [ ] DEV: Run [automated tests](https://confluence.owncloud.com/display/OG/Automated+Tests)
#### BTR QA
* [ ] QA: [Antivirus scan](https://confluence.owncloud.com/display/OG/Virus+Scanning)
* [ ] QA: Changelog testing [Add link to 'Test Results 2.x.x' issue here]
* [ ] QA: [Regression test](https://confluence.owncloud.com/display/OG/Manual+Tests#ManualTests-RegressionTest)

### After QA Approval

* [ ] DEV: Remove git suffix in `MIRALL_VERSION_SUFFIX` in [`VERSION.cmake`](https://confluence.owncloud.com/display/OG/Branching+Off#BranchingOff-VERSION.cmake)
* [ ] DEV: On release(?) branch create a **signed** [tag](https://confluence.owncloud.com/display/OG/Build+and+Tags#BuildandTags-clientrepository) v2.x.z
* [ ] DEV: Create the same tag on branch 2.6 of [_client-plugin-vfs-win_](https://confluence.owncloud.com/display/OG/Build+and+Tags#BuildandTags-client-plugin-vfs-win)
* [ ] DEV: Create the same tag on branch 2.6 of [_wip-msi_](https://confluence.owncloud.com/display/OG/Build+and+Tags#BuildandTags-wip-msi)
* [ ] DEV: Create same tag for [Windows toolchain](https://confluence.owncloud.com/display/OG/Build+and+Tags#BuildandTags-Windowstoolchain)
* [ ] DEV: Create same tag (actually a symlink) for [macOS toolchain](https://confluence.owncloud.com/display/OG/Build+and+Tags#BuildandTags-macOStoolchain)

### Final Rebuild

* [ ] QA: Change the date in `ChangeLog` to the date of creating the tag 
* [ ] QA: Bump `MIRALL_VERSION_PATCH` in [`VERSION.cmake`](https://confluence.owncloud.com/display/OG/Branching+Off#BranchingOff-PatchRelease)
* [ ] QA: Adjust [Linux Templates](https://confluence.owncloud.com/display/OG/Branching+Off#BranchingOff-Linuxtemplates) to support the next patch release version (e.g. 2.6.2)
* [ ] QA: Trigger the [build job](https://confluence.owncloud.com/display/OG/Build+and+Tags#BuildandTags-Finalbuild) 
* [ ] QA: Ping marketing to do their [actions](https://confluence.owncloud.com/display/OG/Marketing+and+Communication#MarketingandCommunication-Marketingtasks)
* [ ] QA: Create [a (draft) release](https://github.com/owncloud/client/releases) with Download links - save as a draft until smoke tested
* [ ] QA: Create a new release issue for a branded release if needed
* [ ] QA: Give [heads-up](https://confluence.owncloud.com/display/OG/Marketing+and+Communication#MarketingandCommunication-Heads-upbeforethefinalrelease) before the final release 

### Final QA

* [ ] QA: [Smoke test](https://confluence.owncloud.com/display/OG/Manual+Tests#ManualTests-SmokeTest)
* [ ] QA: Check if [GitHub download links](https://github.com/owncloud/client/releases) point to correct location
* [ ] QA: Publish the release in GitHub
* [ ] QA: Check [minimum.supported.desktop.version](https://github.com/owncloud/core/blob/master/config/config.sample.php#L1367) on the server

### Communicate the Availability

* [ ] Final Communication https://confluence.owncloud.com/display/OG/Marketing+and+Communication#MarketingandCommunication-Central
   * [ ] Website links for beta (needed for the following posts)
   * [ ] Central post https://central.owncloud.org/tags/c/news/desktop
   * [ ] Inform other [stakeholders](https://confluence.owncloud.com/display/OG/Marketing+and+Communication#MarketingandCommunication-InformStakeholdersaboutFinal)
   * [ ] QA: Inform [packagers](https://confluence.owncloud.com/display/OG/Marketing+and+Communication#MarketingandCommunication-Packagers) - ping @dragotin (openSUSE)


### Final Infrastructure Check

* [ ] QA/DEV: Update [stable channel](https://confluence.owncloud.com/display/OG/Online+Updater%2C+Crash+reporter%2C+Transifex#OnlineUpdater,Crashreporter,Transifex-UpdatetheUpdater) in the owncloud hosted auto updater
* [ ] QA/DEV: Ensure that the [client patch release template](https://github.com/owncloud/client/blob/master/.github/patch_release_template.md) is up to date and in sync with other client templates
* [ ] QA/DEV: Ensure that the [client test plan template](https://github.com/owncloud/QA/blob/master/Desktop/Regression_Test_Plan_Patch_Release.md) is up to date.
* [ ] QA: Update [supported platforms](https://confluence.owncloud.com/display/OG/Supported+Platforms)

### A Few Days After the Release

* [ ] DEV: Check the [crash reporter](https://confluence.owncloud.com/display/OG/Online+Updater%2C+Crash+reporter%2C+Transifex#OnlineUpdater,Crashreporter,Transifex-CrashReporter) for bad/frequent crashes
