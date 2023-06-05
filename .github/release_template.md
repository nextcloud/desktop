## Reason

* Needed for branded client X.x.x release (link to branded release ticket)

### Template
[Release Template](https://github.com/owncloud/client/blob/master/.github/release_template.md)

__TODO__:
* Check if anything is missing from [release_template_outdated_2020.md](https://github.com/owncloud/client/blob/master/.github/release_template_outdated_2020.md) and merge here. We use the same template for a patch, minor or major release now.
* For each item add a link to the respective action if available

### Prerequisites

* [ ] QA: Update [Test Plans](https://confluence.owncloud.com/display/OG/Desktop+Client+Test+Plan+Maintenance)
* [ ] Update [supported platforms](https://doc.owncloud.com/desktop/next/installing.html#system-requirements) @michaelstingl
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
* [ ] QA: [Antivirus scan](https://confluence.owncloud.com/display/OG/Virus+Scanning)
* [ ] QA: [Upload](https://confluence.owncloud.com/display/OG/Upload+linux+gpg+keys+to+key+server) linux gpg keys to key server
* [ ] DEV: Prepare the update server for new version (AppImages included)
   * [ ] DEV: Provide 'testpilotcloud' on **Beta** update channel
* [ ] Beta/RC [Communication](https://confluence.owncloud.com/x/loJK)
    * [ ] Inform ``#updates`` that we have Beta/RC 
    * [ ] Ping ``#marketing`` to update website links for beta (needed for the following posts)
    * [ ] Central post https://central.owncloud.org/tags/c/news/desktop
* [ ] QA: Check Crash reporter (WIN/Mac/Linux Appimage: start 'owncloud --debug' on cmd line, system tray right click menu: 'Crash now - qt fatal' -> report window not empty, sending the report works)
* [ ] QA: Communicate documentation changes  
   * [ ] Inform ``#documentation-internal`` (@mmattel) about the start of testing phase (latest a week before the release!). They'll prepare a PR with respective doc version
   * [ ] Open issues in ``docs-client-desktop`` repo for already known doc-relevant items and mark them accordingly, e.g. backport to 2.X.x necessary

### QA

* [Automated tests](https://confluence.owncloud.com/display/OG/Automated+Tests):
   * [ ] QA: GUI tests passed on a tag
   * [ ] QA: All [Linux platform install](https://confluence.owncloud.com/display/OG/Automated+Tests#AutomatedTests-LinuxInstallTest)
   * [ ] DEV: [Smash box test](https://drone.owncloud.com/owncloud/smashbox-testing): [Info](https://confluence.owncloud.com/display/OG/Automated+Tests#AutomatedTests-Smashbox) Make sure tests run on latest version 
* Manual tests:
   * [ ] QA: [Changelog](https://github.com/owncloud/client/blob/master/CHANGELOG.md) test
   * [ ] QA: Regression test

### Final Rebuild after QA Approval

* [ ] QA: Inform on ``#documentation-internal`` that the tag for the final release will be set a day or at least half a day __before__ (only for a major/minor release). They'll merge docs PR before that.
* [ ] DEV: Create final release tag (e.g., `v4.5.6`)
* [ ] DEV: Create [builds](https://confluence.owncloud.com/display/OG/Build+and+Tags#BuildandTags-Tags) for themes 'ownCloud' and 'testpilotcloud' for final release tag
* [ ] QA: Check [squish tests](https://confluence.owncloud.com/display/OG/Squish+Testing#SquishTesting-Finalreleasestep) running successfuly on [drone](https://drone.owncloud.com/owncloud/client) for the final tag v3.X.x
* [ ] DEV: Create [a (draft) release](https://github.com/owncloud/client/releases) with Download links - save as a draft until smoke tested
* [ ] QA: Create a new release issue for a branded release if needed [Branded Client Release Template](https://confluence.owncloud.com/pages/viewpage.action?spaceKey=OG&title=Desktop+Client+Release+Process)

### Final QA

* [ ] QA: [Smoke test](https://confluence.owncloud.com/display/OG/Manual+Tests#ManualTests-SmokeTest)
* [ ] DEV: Publish the release in GitHub
* [ ] QA: Check [documentation](https://confluence.owncloud.com/display/OG/Documentation)

### Communicate the Availability
* [ ] Final [Marketing and Communication](https://confluence.owncloud.com/display/OG/Marketing+and+Communication)
   * [ ] QA: Ping marketing to update links on https://owncloud.com/desktop-app/ (provide links from github releases; needed for the following posts)
   * [ ] QA: Central post https://central.owncloud.org/tags/c/news/desktop
   * [ ] QA: Inform on ``#updates`` channel
   * [ ] QA: Inform [packagers](https://confluence.owncloud.com/x/QYLEAg)
* [ ] Inform ``#marketing`` (@bwalter, @mfeilner) to remind them to update Wikipedia + Wikidata

### Final Infrastructure Check

* [ ] QA: Verify marketing has updated all the links ([owncloud.com](https://owncloud.com/desktop-app), [wiki de](https://de.wikipedia.org/wiki/OwnCloud), [wiki en](https://en.wikipedia.org/wiki/OwnCloud), [wikidata](https://www.wikidata.org/wiki/Q20763576))
* [ ] QA/DEV: Update [stable channel](https://confluence.owncloud.com/display/OG/Online+Updater%2C+Crash+reporter%2C+Transifex#OnlineUpdater,Crashreporter,Transifex-UpdatetheUpdater) in the owncloud hosted auto updater
* [ ] QA: Ensure that the [client release template](https://github.com/owncloud/client/blob/master/.github/release_template.md) is up to date

### A Few Days After the Release

* [ ] DEV: Check the [crash reporter](https://confluence.owncloud.com/display/OG/Online+Updater%2C+Crash+reporter%2C+Transifex#OnlineUpdater,Crashreporter,Transifex-CrashReporter) for bad/frequent crashes
