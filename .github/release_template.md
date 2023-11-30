## Reason

* Needed for branded client X.x.x release (link to branded release ticket)

### Template
[Release Template](https://github.com/owncloud/client/blob/master/.github/release_template.md)

__TODO__:
* Check if anything is missing from [release_template_outdated_2020.md](https://github.com/owncloud/client/blob/master/.github/release_template_outdated_2020.md) and merge here. We use the same template for a patch, minor or major release now.
* For each item add a link to the respective action if available

### Administration

* [ ] QA: Update [Test Plans](https://confluence.owncloud.com/display/OG/Desktop+Client+Test+Plan+Maintenance)
* [ ] Update [supported platforms](https://doc.owncloud.com/desktop/next/installing.html#system-requirements) @michaelstingl
* [ ] QA: Check the translations coming from transifex: https://github.com/owncloud/client/commits/ -> Filter based on a release branch/tag and search for `[tx] updated client translations from transifex [skip ci]`
* [ ] DEV: Check for new OpenSSL version 
* [ ] DEV: Check [dependencies](https://confluence.owncloud.com/display/OG/Dependencies) for updates
* [ ] DEV: Update [SBOM](https://cloud.owncloud.com/f/6072843)
* [ ] DEV: For a major release create `X` version branch
  * [ ] QA: In drone adjust the branch for nightly [GUI tests](https://confluence.owncloud.com/display/OG/Squish+Testing#SquishTesting-Prerequisite) @individual-it
* [ ] QA: Communicate documentation changes  
   * [ ] Inform ``#documentation-internal`` (@mmattel) about the start of testing phase (latest a week before the release!). They'll prepare a PR with respective doc version
   * [ ] Open issues in ``docs-client-desktop`` repo for already known doc-relevant items and mark them accordingly, e.g. backport to 2.X.x necessary
* [ ] Decide if the prerelease stage will be public or internat @michaelstingl 
* [ ] Announce feature freeze in ``#release_coordination`` channel before Beta build
* [ ] Announce string freeze in ``#release_coordination`` channel a few days later
   * [ ] Inform ``#documentation-internal`` about it so that they can do the translations
  
### Copy for Each Build (Beta/RC)

* [ ] DEV: Tag (Beta or RC) and build [builds](https://confluence.owncloud.com/display/OG/Build+and+Tags#BuildandTags-Sprintbuild) for theme 'ownCloud' and 'testpilotcloud' (includes ChangeLog for the tag on https://github.com/owncloud/client/releases/)
* [ ] QA: [Smoke test](https://owncloud.testrail.io/index.php?/cases/view/2469) (for public prelease)
* [ ] DEV: GitHub release
* [ ] Beta/RC [Communication](https://confluence.owncloud.com/x/loJK)
    * [ ] Inform ``#updates`` that we have Beta/RC    
    * [ ] For public prerelease: Inform ``#marketing`` and tell them to send email to beta testers
    * [ ] For public prerelease: Write/edit Central post https://central.owncloud.org/tags/c/news/desktop with link to github release 
* [ ] DEV: Prepare the update server for new version (AppImages included)
   * [ ] DEV: Provide 'testpilotcloud' on **Beta** update channel
     
### QA 

* [ ] QA: [Antivirus scan](https://confluence.owncloud.com/display/OG/Virus+Scanning) the first RC
* [ ] QA: [Upload](https://confluence.owncloud.com/display/OG/Upload+linux+gpg+keys+to+key+server) linux gpg keys to key server
* [ ] QA: Check Crash reporter:  start 'owncloud --debug' on cmd line, system tray right click menu: 'Crash now - qt fatal' -> report window not empty, sending the report works)
  * [ ] Windows  
  * [ ] macOS
  * [ ] AppImage (Linux)
* [Automated tests](https://confluence.owncloud.com/display/OG/Automated+Tests) (for the first beta and the last RC):
   * [ ] QA: GUI tests passed on a tag
   * [ ] QA: All [Linux platform install](https://confluence.owncloud.com/display/OG/Automated+Tests#AutomatedTests-LinuxInstallTest)
* Manual tests:
   * [ ] QA: [Changelog](https://github.com/owncloud/client/blob/master/CHANGELOG.md) test
   * [ ] QA: Regression test
   * [ ] QA: Branded regression test

### Prerequisites for final release

* [ ] DEV: Create vX.Y.Z release tag in client repo and everywhere else
* [ ] DEV: bump VERSION.cmake in master to say 3.(x+1).x unless already done.

### Final Rebuild after QA Approval

* [ ] QA: Inform on ``#documentation-internal`` that the tag for the final release will be set a day or at least half a day __before__ (only for a major/minor release). They'll merge docs PR before that.
* [ ] DEV: Create final release tag (e.g., `v4.5.6`)
* [ ] DEV: Create [builds](https://confluence.owncloud.com/display/OG/Build+and+Tags#BuildandTags-Tags) for themes 'ownCloud' and 'testpilotcloud' for final release tag
* [ ] DEV: Update version for future builds
* [ ] QA: Check [squish tests](https://confluence.owncloud.com/display/OG/Squish+Testing#SquishTesting-Finalreleasestep) running successfuly on [drone](https://drone.owncloud.com/owncloud/client) for the final tag v3.X.x
* [ ] QA: Create a new release issue for a branded release if needed [Branded Client Release Template](https://github.com/owncloud/enterprise/blob/master/internal_release_templates/internal_client_release_template.md)

### Final Steps

* [ ] QA: [Smoke test](https://confluence.owncloud.com/display/OG/Manual+Tests#ManualTests-SmokeTest)
* [ ] DEV: Publish the release in GitHub
* [ ] QA: Update lines 4 and 5 of [install docs](https://github.com/owncloud/docs-client-desktop/blob/master/modules/ROOT/pages/installing.adoc) with the final build number. Merge into master and backport to the respective branch
* [ ] QA: Check that [documentation](https://doc.owncloud.com/desktop/next/) offers the new version
* [ ] DEV: Merge version branch into master

### [Marketing and Communication](https://confluence.owncloud.com/display/OG/Marketing+and+Communication)
   
* [ ] QA: Ping ``#marketing``at marketing@owncloud.com; @bwalter to update links on https://owncloud.com/desktop-app/ (provide links from github releases) and @mfeilner to update [wiki de](https://de.wikipedia.org/wiki/OwnCloud), [wiki en](https://en.wikipedia.org/wiki/OwnCloud), [wikidata](https://www.wikidata.org/wiki/Q20763576))
* [ ] QA: Central post https://central.owncloud.org/tags/c/news/desktop
* [ ] QA: Inform on ``#updates`` channel
* [ ] QA: Inform [packagers](https://confluence.owncloud.com/x/QYLEAg)

### Infrastructure Check

* [ ] QA/DEV: Update [stable channel](https://confluence.owncloud.com/display/OG/Online+Updater%2C+Crash+reporter%2C+Transifex#OnlineUpdater,Crashreporter,Transifex-UpdatetheUpdater) in the owncloud hosted auto updater
* [ ] QA: Ensure that the [client release template](https://github.com/owncloud/client/blob/master/.github/release_template.md) is up to date

### A Few Days After the Release

* [ ] DEV: Check the [crash reporter](https://confluence.owncloud.com/display/OG/Online+Updater%2C+Crash+reporter%2C+Transifex#OnlineUpdater,Crashreporter,Transifex-CrashReporter) for bad/frequent crashes
