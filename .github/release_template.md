## Reason

* Needed for branded client X.x.x release (link to branded release ticket)

### Template
[Release Template](https://github.com/owncloud/client/blob/master/.github/release_template.md)

__TODO__:
* Check if anything is missing from [release_template_outdated_2020.md](https://github.com/owncloud/client/blob/master/.github/release_template_outdated_2020.md) and merge here. We use the same template for a patch, minor or major release now.
* For each item add a link to the respective action if available

### Prerequisites

* [ ] QA: Update [Test Plans](https://confluence.owncloud.com/display/OG/Desktop+Client+Test+Plan+Maintenance)
* [ ] QA: Check the translations coming from transifex: https://github.com/owncloud/client/commits/ -> Filter based on a release branch/tag and search for `[tx] updated client translations from transifex [skip ci]`
* [ ] DEV: Check for new OpenSSL version 
* [ ] DEV: Check [dependencies](https://confluence.owncloud.com/display/OG/Dependencies) for updates
* [ ] DEV: Prepare the release in a `X.x` version branch (a patch release is maintained in the minor release branch)  
  If this *is* a major or minor release:
  * [ ] DEV: Create new `X.x` version branch.
  * [ ] QA: In drone adjust the branch to run nightly (GUI tests](https://confluence.owncloud.com/display/OG/Squish+Testing#SquishTesting-Prerequisite) to the next release branch @individual-it 
* [ ] DEV: bump VERSION.cmake in master to say 3.(x+1).x unless already done.

### Build

* [ ] DEV: Tag (Beta or RC) and build [builds](https://confluence.owncloud.com/display/OG/Build+and+Tags#BuildandTags-Sprintbuild) for theme 'ownCloud' and 'testpilotcloud' (includes ChangeLog for the tag on https://github.com/owncloud/client/releases/)
* [ ] QA: [Upload](https://confluence.owncloud.com/display/OG/Upload+linux+gpg+keys+to+key+server) linux gpg keys to key server
* [ ] QA: Communicate documentation changes  
   * [ ] Inform ``#documentation-internal`` (@mmattel) about the start of testing phase (latest a week before the release!). They'll prepare a PR with respective doc version
   * [ ] Open issues in ``docs-client-desktop`` repo for already known doc-relevant items and mark them accordingly, e.g. backport to 2.X.x necessary
* [ ] DEV: Prepare the update server for new version (AppImages included)
   * [ ] DEV: Provide 'testpilotcloud' on **Beta** update channel
* [ ] Beta/RC [Communication](https://confluence.owncloud.com/display/OG/Marketing+and+Communication)
   * [ ] Website links for beta (needed for the following posts)
   * [ ] Central post https://central.owncloud.org/tags/c/news/desktop
   * [ ] All other stakeholders
* [ ] QA: Check Crash reporter (WIN/Mac/Linux Appimage: start 'owncloud --debug' on cmd line, system tray right click menu: 'Crash now - qt fatal' -> report window not empty, sending the report works)

### QA

* [Automated tests](https://confluence.owncloud.com/display/OG/Automated+Tests):
   * [ ] QA: GUI tests passed on a tag
   * [ ] QA: All [Linux platform install](https://confluence.owncloud.com/display/OG/Automated+Tests#AutomatedTests-LinuxInstallTest)
   * [ ] DEV: [Smash box test](https://drone.owncloud.com/owncloud/smashbox-testing): [Info](https://confluence.owncloud.com/display/OG/Automated+Tests#AutomatedTests-Smashbox) Make sure tests run on latest version 
* Manual tests:
   * [ ] DEV: [Smoke test](https://confluence.owncloud.com/display/OG/Manual+Tests#ManualTests-DEVSmokeTest)
   * [ ] QA: [Changelog](https://github.com/owncloud/client/blob/master/CHANGELOG.md) test
   * [ ] QA: Regression test
* [ ] QA: [Antivirus scan](https://confluence.owncloud.com/display/OG/Virus+Scanning)
* [ ] QA: If required: create a separate test plan ticket for Windows VFS testing from [VFS Template](https://github.com/owncloud/QA/blob/master/Desktop/Test_Plan_VFS.md) - add the link here

### Final Rebuild after QA Approval

* [ ] QA: Inform on ``#documentation-internal`` that the tag for the final release will be set a day or at least half a day __before__ (only for a major/minor release). They'll merge docs PR before that.
* [ ] DEV: Tag and build [builds](https://confluence.owncloud.com/display/OG/Build+and+Tags#BuildandTags-Tags) for theme 'ownCloud' and 'testpilotcloud' for final build
* [ ] QA: Check [squish tests](https://confluence.owncloud.com/display/OG/Squish+Testing#SquishTesting-Finalreleasestep) running successfuly on [drone](https://drone.owncloud.com/owncloud/client) for the final tag v3.X.x
* [ ] DEV: Adjust [Linux Templates](https://confluence.owncloud.com/display/OG/Branching+Off#BranchingOff-Linuxtemplates) to support the next patch release version (e.g. 2.9.1) @dschmidt @fmoc
* [ ] DEV: Ping ``#release-coordination`` so that marketing can do their [actions](https://confluence.owncloud.com/display/OG/Marketing+and+Communication#MarketingandCommunication-Marketingtasks)
* [ ] DEV: Create [a (draft) release](https://github.com/owncloud/client/releases) with Download links - save as a draft until smoke tested
* [ ] QA: Create a new release issue for a branded release if needed [Branded Client Release Template](https://confluence.owncloud.com/pages/viewpage.action?spaceKey=OG&title=Desktop+Client+Release+Process)
* [ ] QA: Give [heads-up](https://confluence.owncloud.com/display/OG/Marketing+and+Communication#MarketingandCommunication-Heads-upbeforethefinalrelease) before the final release 

### Final QA

* [ ] QA: [Smoke test](https://confluence.owncloud.com/display/OG/Manual+Tests#ManualTests-SmokeTest)
* [ ] DEV: Publish the release in GitHub
* [ ] QA: Check [documentation](https://confluence.owncloud.com/display/OG/Documentation)

### Communicate the Availability
* [ ] Final [Marketing and Communication](https://confluence.owncloud.com/display/OG/Marketing+and+Communication)
   * [ ] QA: Ping marketing to update links on https://owncloud.com/desktop-app/ (provide links from github releases; needed for the following posts)
   * [ ] QA: Central post https://central.owncloud.org/tags/c/news/desktop
   * [ ] QA: Inform on ``#updates`` channel, so that marketing knows about the new release
   * [ ] QA: Inform [packagers](https://confluence.owncloud.com/x/QYLEAg) - ping @dragotin (openSUSE)
* [ ] Inform ``#marketing`` (@bwalter, @mfeilner) and remind to update Wikipedia + Wikidata
  * [ ] https://de.wikipedia.org/wiki/OwnCloud
  * [ ] https://en.wikipedia.org/wiki/OwnCloud
  * [ ] https://www.wikidata.org/wiki/Q20763576

### Final Infrastructure Check

* [ ] QA: Verify marketing has updated all the links (owncloud.com, wiki de, wiki en, wikidata)
* [ ] QA/DEV: Update [stable channel](https://confluence.owncloud.com/display/OG/Online+Updater%2C+Crash+reporter%2C+Transifex#OnlineUpdater,Crashreporter,Transifex-UpdatetheUpdater) in the owncloud hosted auto updater
* [ ] QA: Ensure that the [client release template](https://github.com/owncloud/client/blob/master/.github/release_template.md) is up to date
* [ ] QA: Ensure that the [testplan patch release template](https://github.com/owncloud/QA/blob/master/Desktop/Regression_Test_Plan_Patch_Release.md) is up to date.
* [ ] QA: Ensure that the [testplan minor release template](https://github.com/owncloud/QA/blob/master/Desktop/Regression_Test_Plan_Minor_Release.md) is up to date

### A Few Days After the Release

* [ ] DEV: Check the [crash reporter](https://confluence.owncloud.com/display/OG/Online+Updater%2C+Crash+reporter%2C+Transifex#OnlineUpdater,Crashreporter,Transifex-CrashReporter) for bad/frequent crashes
