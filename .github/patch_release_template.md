## Reason

* Needed for branded client X.x.x release (link to branded release ticket)
* ...

### Template
[Patch Release Template](https://github.com/owncloud/client/blob/master/.github/patch_release_template.md)

__TODO__: [patch_release_template.md](https://github.com/owncloud/client/blob/master/.github/patch_release_template.md) is used here because it's the newest, updated version for a release ticket template. What might be missing from [release_template.md](https://github.com/owncloud/client/blob/master/.github/release_template.md) should be merged so we can use one template for patch, minor or major release in future (rename the file accordingly).

### Prerequisites

* [ ] QA: Update [Test Plans](https://confluence.owncloud.com/display/OG/Desktop+Client+Test+Plan+Maintenance) 
* [ ] QA: Update [documentation](https://confluence.owncloud.com/display/OG/Documentation)
* [ ] QA: Check the translations coming from https://github.com/owncloud/client/actions/workflows/translate.yml
* [ ] QA: Make sure squish tests are running successfully on X.x branch: go to https://github.com/owncloud/client, click on 'commits' above the source tree, click on green checkmark of latest commit, click 'Details' on 'continious-integration/drone/push' and check GUI-tests-@smokeTest
* [ ] DEV: Check for new OpenSSL version 
* [ ] DEV: Check [dependencies](https://confluence.owncloud.com/display/OG/Dependencies) for updates
* [ ] DEV: Prepare the release in a `X.x` version branch (a patch release is maintained in the minor release branch) 
* [ ] DEV: bump VERSION.cmake in master to say 2.(x+1).x unless already done.

### Build

* [ ] DEV: Tag and build [builds](https://confluence.owncloud.com/display/OG/Build+and+Tags#BuildandTags-Sprintbuild) for theme 'ownCloud' and 'testpilotcloud' (includes ChangeLog for the tag on https://github.com/owncloud/client/releases/)
* [ ] Beta/RC [Communication](https://confluence.owncloud.com/display/OG/Marketing+and+Communication)
   * [ ] Website links for beta (needed for the following posts)
   * [ ] Central post https://central.owncloud.org/tags/c/news/desktop
   * [ ] All other stakeholders
* [ ] QA: Check Crash reporter (WIN/Mac/Linux Appimage: start 'owncloud --debug' on cmd line, system tray right click menu: 'Crash now - qt fatal') 

### QA

* [ ] DEV: [Smash box test](https://drone.owncloud.com/owncloud/smashbox-testing) Make sure tests run on latest version 
* [ ] QA: [Smoke test](https://confluence.owncloud.com/display/OG/Manual+Tests#ManualTests-DEVSmokeTest)
* [ ] QA: Run [automated tests](https://confluence.owncloud.com/display/OG/Automated+Tests)
    * [ ] TODO:  add "xvfb-run owncloud-client" to RUN.sh ([Xvfb](https://en.wikipedia.org/wiki/Xvfb))
* [ ] QA: [Antivirus scan](https://confluence.owncloud.com/display/OG/Virus+Scanning)
* [ ] QA: Create testplan ticket according to release type (patch or minor, see https://confluence.owncloud.com/display/OG/Desktop+Client+Release+Process) and link here
* [ ] QA: Add changelog testing as a comment to above testplan ticket and link here (for changeLog issues see https://github.com/owncloud/client/releases/) and link here

### Final Rebuild after QA Approval

* [ ] DEV: Tag and build [builds](https://confluence.owncloud.com/display/OG/Build+and+Tags#BuildandTags-Sprintbuild) for theme 'ownCloud' and 'testpilotcloud' for final build 
     * [ ] TODO: create confluence page with info about final DEV release steps
* [ ] DEV: Adjust [Linux Templates](https://confluence.owncloud.com/display/OG/Branching+Off#BranchingOff-Linuxtemplates) to support the next patch release version (e.g. 2.9.1) @dschmidt @fmoc
* [ ] DEV: Ping marketing to do their [actions](https://confluence.owncloud.com/display/OG/Marketing+and+Communication#MarketingandCommunication-Marketingtasks)
* [ ] DEV: Create [a (draft) release](https://github.com/owncloud/client/releases) with Download links - save as a draft until smoke tested
* [ ] QA: Create a new release issue for a branded release if needed
* [ ] QA: Give [heads-up](https://confluence.owncloud.com/display/OG/Marketing+and+Communication#MarketingandCommunication-Heads-upbeforethefinalrelease) before the final release 

### Final QA

* [ ] QA: [Smoke test](https://confluence.owncloud.com/display/OG/Manual+Tests#ManualTests-SmokeTest)
* [ ] DEV: Publish the release in GitHub

### Communicate the Availability

* [ ] Final [Marketing and Communication](https://confluence.owncloud.com/display/OG/Marketing+and+Communication)
   * [ ] Website links for final release (needed for the following posts)
   * [ ] QA: Central post https://central.owncloud.org/tags/c/news/desktop
   * [ ] QA: Inform other [stakeholders](https://confluence.owncloud.com/display/OG/Marketing+and+Communication#MarketingandCommunicationInformStakeholdersaboutFinal)
   * [ ] QA: Inform [packagers](https://confluence.owncloud.com/display/OG/Marketing+and+Communication#MarketingandCommunication-Packagers) - ping @dragotin (openSUSE)


### Final Infrastructure Check

* [ ] QA/DEV: Update [stable channel](https://confluence.owncloud.com/display/OG/Online+Updater%2C+Crash+reporter%2C+Transifex#OnlineUpdater,Crashreporter,Transifex-UpdatetheUpdater) in the owncloud hosted auto updater
* [ ] QA: Ensure that the [client patch release template](https://github.com/owncloud/client/blob/master/.github/patch_release_template.md) is up to date and in sync with other client templates
* [ ] QA: Ensure that the [client test plan template](https://github.com/owncloud/QA/blob/master/Desktop/Regression_Test_Plan_Patch_Release.md) is up to date.

### A Few Days After the Release

* [ ] DEV: Check the [crash reporter](https://confluence.owncloud.com/display/OG/Online+Updater%2C+Crash+reporter%2C+Transifex#OnlineUpdater,Crashreporter,Transifex-CrashReporter) for bad/frequent crashes
