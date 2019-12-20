<!--
This is the template for new release issues.
(originated from https://github.com/owncloud/client/wiki/Release%20Checklist%20Template)
(20181109jw: One of these two should be deleted. They will never be in sync otherwise.)
-->

Open an issue called 'Release 2.x.0' in Client repository and copy below text into a task and tick the items:
<hr>

Major/Minor release templete. Enter here, when we have three estimated dates:
* Date of feature freeze
* Date of QA start
* Date of final

### Before branching off:

* [ ] Dev: Check [dependencies](https://handbook.owncloud.com/release_processes/client/dependencies.html) for updates - @TheOneRing @guruz
* [ ] Dev: Update [ChangeLog](https://handbook.owncloud.com/release_processes/client/change_log.html) - @TheOneRing @guruz
* [ ] QA: Update [Test Plans](https://handbook.owncloud.com/release_processes/client/testlink.html) - @HanaGemela @jnweiger
* [ ] QA: Review list of [supported platforms](https://handbook.owncloud.com/release_processes/client/supported_platforms.html) -  @HanaGemela @jnweiger @TheOneRing @guruz
* [ ] QA: Update [documentation](https://handbook.owncloud.com/release_processes/client/documentation.html) -  @HanaGemela @jnweiger
* [ ] QA: Check Sprint Board for remaining issues -  @HanaGemela @jnweiger

### On the day of the first daily build of the new branch:
* [ ] Dev: Internally announce it feature freeze - @TheOneRing @guruz
* [ ] Dev: Edit [`VERSION.cmake`](https://handbook.owncloud.com/release_processes/client/branch.html#version-cmake) - @TheOneRing @guruz
* [ ] QA: Adjust [Linux Templates](https://handbook.owncloud.com/release_processes/client/branch.html#linux-templates) - @HanaGemela @jnweiger
* [ ] QA: Adjust [ownBrander](https://handbook.owncloud.com/release_processes/client/branch.html#ownbrander) - @HanaGemela @jnweiger
* [ ] QA: Adjust [AppVeyor](https://handbook.owncloud.com/release_processes/client/branch.html#appveyor) - @HanaGemela @jnweiger
* [ ] QA: Adjust [drone](https://handbook.owncloud.com/release_processes/client/branch.html#drone) - @HanaGemela @jnweiger
* [ ] QA: Adjust [translation jobs](https://handbook.owncloud.com/release_processes/client/branch.html#translations) - @HanaGemela @jnweiger
* [ ] QA: Use `obs-copyprj.sh` to backup the desktop project to `desktop:client-2.6.x` - @HanaGemela @jnweiger
* [ ] Dev: Start running automated tests on the dailies - @TheOneRing @guruz

### After the first daily build of the new branch:
* [ ] Announce the new branch to community and advertise dailies for public testing 

TODO: WHen do we call it beta or RC?
TODO: describe what dailies we have in documentation. (Platforms, Versions, Master, current preprelease, and last stable release branch.)

* [ ] QA: [Antivirus scan](https://handbook.owncloud.com/release_processes/client/virus.html) - @HanaGemela @jnweiger 

### For all (betas?) and RCs (Copy this section for each beta/rc):
* [ ] Ensure the crash reporter server is up.
TODO: log into sentry, see if there is a fresh report. sentry.io and one more component in our infrastructure. And/or trigger a crash.
( ongoing task: * [ ] Check crash reporter for bad crashes of the last stable (same crash happening to many users) )

(ongoing * [ ] Make sure previous minor/major version's branch is merged into current major branch (or everything cherry-picked))
* [ ] Add latest updates to Changelog in the client source repository.
* [ ] Branch off a release branch called VERSION-rcX or VERSION-betaX  (without v, v is for tags)
* [ ] Edit ```VERSION.cmake``` to set the suffix to beta1, beta2 etc in the release branch.

TODO: move to patch-release checklist * [ ] Make sure to increase the version number of the branched of release, e.g. if you release 2.3.2 then you should change VERSION.cmake in 2.3 to 2.3.3 since that branch now will be 2.3.3
* [ ] jenkins.int: Create build for theme 'ownCloud' using client-trigger (uncheck the "daily build" checkbox, use rcX or betaX dropdown for version suffix)
* [ ] jenkins.int: Create build for theme 'testpilotcloud' using client-trigger (uncheck the "daily build" checkbox, use the rcX or betaX dropdown for version suffix)
* Build results are in https://download.owncloud.com/desktop/testing -- win and mac binaries are there, linux packages are listed in a *repo.html file pointing to the repository.
* [ ] Check if *tar.xz.asc files are there. If not resort to https://github.com/owncloud/enterprise/wiki/Desktop-Signing-Knowledge
* [ ] Run [the smoke test](https://handbook.owncloud.com/release_processes/client/smoke_test.html)
* [ ] Linux: Run https://gitea.owncloud.services/client/linux-docker-install/src/branch/master/RUN.sh with repo=https://download.opensuse.org/repositories/isv:/ownCloud:/desktop:/testing
* [ ] review everything :-)

###############################################

* [ ] Linux: add/remove build targets in isv:ownCloud:Qt51215 and isv:ownCloud:desktop:testing to match the list of supported platforms and announced(!) deprecations. Keep in sync with https://doc.owncloud.org/server/latest/admin_manual/installation/system_requirements.html#desktop and https://github.com/owncloud/ownbrander/blob/master/brand-items.php#L1651

TODO guruz: is this still needed?
* [ ] Create a signed tag using ```git tag -u E94E7B37 tagname``` (https://github.com/owncloud/enterprise/wiki/Desktop-Signing-Knowledge)

* [ ] update the wordpress content at owncloud.org/download (Attention: No staging!)
* [ ] Inform packagers @dragotin (openSUSE), @hefee (Debian), ??? (Fedora)
* [ ] Announce on https://central.owncloud.org
TODO: itemize what goes into the announcement: deprecation warnings. ...

* [ ] Inform community mailinglists devel@owncloud.org and testpilots@owncloud.org (make sure to mention it is an rc). Link to the central post so discussion happens there.
* [ ] Check crash reporter
* [ ] Update the owncloud hosted auto updater according to https://github.com/owncloud/enterprise/blob/master/client_update_checker/README.md#deploy  (beta/unstable channel!)

### One week before the final release:
* [ ] Communicate the release schedule on mailinglist release-coordination@owncloud.com. Give a high level overview of the upcoming new features, changes etc.
* [ ] Ensure marketing is aware (marketing@owncloud.com) and prepared for the release (social, .com website, cust.communications)
* [ ] Inform GCX knows the next version is about 1 week out (gcx@owncloud.com)

### One day before final Release:
* [ ] Check crash reporter for bad crashes od this RC (same crash happening to many users)
* [ ] Check the translations coming from transifex: All synchronized? (20181109jw: where? how?)
* [ ] Run the tx.pl scripts on the final code tag (20181109jw: really? What does that test?)
* [ ] Run ```make test```
* [ ] Run smashbox (20180719 jw: FIXME: add details, how?)
* [ ] Inform product management and marketing that we are 1 day out

### On Release Day (for final release):
* [ ] Add last updates to Changelog in the client source repository.
* [ ] Branch off a release branch called VERSION  (without v, v is for tags)
* [ ] Edit ```VERSION.cmake``` to set the suffix to "" etc. Commit the result to the release branch only
* [ ] Create build for theme 'ownCloud' using client-trigger (uncheck the "daily build" checkbox, use the proper dropdown for version suffix)
* [ ] Create build for theme 'testpilotcloud' using client-trigger (uncheck the "daily build" checkbox, use the proper dropdown for version suffix)
* Build results are in https://download.owncloud.com/desktop/testing -- win and mac binaries are there, linux packages are listed in a *repo.html file. 
* [ ] Check if *tar.xz.asc files are there. If not resort to https://github.com/owncloud/enterprise/wiki/Desktop-Signing-Knowledge
* [ ] Branch isv:ownCloud:desktop to isv:ownCloud:desktop:client-X.Y.Z using https://github.com/owncloud/administration/blob/master/jenkins/obs_integration/ (the Linux packages will always land in the :testing repository)
  ```obs-deepcopy-prj.sh isv:ownCloud:desktop isv:ownCloud:desktop:client-2.5.1```
* [ ] Re-download Mac builds and check signature. Interactive in installer window
* [ ] Re-download Win build check signature. From Mac or Linux: ```osslsigncode verify ownCloud-version-setup.exe```
* [ ] Mac: Perform smoke test of non-osx10.11 package (fresh install, perform upload/download, check the version in General tab)
* [ ] Win: Perform smoke test of non-GPO package (fresh install, perform upload/download, check the version in General tab)
* [ ] Linux: Perform smoke test two distro packages (fresh install, perform upload/download, check the version in General tab)
      Latest Ubuntu + Latest Fedora
* [ ] Linux: Run https://gitea.owncloud.services/client/linux-docker-install/src/branch/master/RUN.sh repo=https://download.opensuse.org/repositories/isv:/ownCloud:/desktop:/testing
* [ ] Win/Mac Copy builds from ```testing``` to ```stable``` on download.owncloud.com, double check the download links. (make sure the .asc is there too.
* [ ] Linux: also copy the *.linux-repo.html files from ```testing``` to ```stable``` **and** edit away the `:testing` strings.
* [ ] Linux: disable publishing on project isv:ownCloud:desktop
* [ ] Linux: copy from testing to released in OBS:
  ```obs-deepcopy-prj.sh isv:ownCloud:desktop:testing isv:ownCloud:desktop```
  ```obs-deepcopy-prj.sh isv:ownCloud:testpilot:testing isv:ownCloud:testpilot```
* [ ] Linux: Re-enable OBS publishing on the project after official release date and if all distros build (check for accidentially disabled packages too) 
* [ ] Linux: Wait until everything is built and published, then disable publishing on project isv:ownCloud:desktop
* [ ] Create git signed tag in client repository using ```git tag -u E94E7B37 tagname``` (https://github.com/owncloud/enterprise/wiki/Desktop-Signing-Knowledge)
* [ ] Create same tag for MSI code
* [ ] Create same tag for Windows toolchain
* [ ] Create same tag (actually a symlink) for macOS toolchain
* [ ] Create a (draft) release on https://github.com/owncloud/client/releases
* [ ] Update https://github.com/owncloud/client/blob/master/ChangeLog
* [ ] 1h later check https://owncloud.org/changelog/desktop-client/ -> it pulls from the master ChangeLog file hourly. 
* [ ] Update https://owncloud.org/download/#owncloud-desktop-client -> Download ownCloud -> click open 'Desktop Client', edit win/mac/lin, each all three tabs "Production", "Technical Preview" [disabled], "Test pilot" enabled, edit the links.
* [ ] Update https://owncloud.com/download/#desktop-clients (both de & en, achim via #marketing)
* [ ] Announce on https://central.owncloud.org
* [ ] Announce on announcements@owncloud.org Link to the central post so discussion happens there.
* [ ] Inform packagers @dragotin (openSUSE) @hefee (Debian)
* [ ] Send out Social (tweet, blog, other)
* [ ] Send out customer communication (if any)
* [ ] Inform GCX that the new version is released (gcx@owncloud.com)
* [ ] Inform release-coordination@owncloud.com
* [ ] Ensure marketing is aware (marketing@owncloud.com)
* [ ] Take pride and celebrate!
* [ ] Tell GCX to increment the minimum supported version for enterprise customers
* [ ] Check if minimum.supported.desktop.version (https://github.com/owncloud/core/blob/master/config/config.sample.php#L1152) needs to be updated in server

### 15 minutes after after release:
* [ ] Test all advertised download links to have the expected version
* [ ] Check for build errors in OBS, do
```obs-deepcopy-prj.sh isv:ownCloud:desktop isv:ownCloud:desktop:client-2.X.X```
* [ ] disable publishing in OBS to prevent that accidential rebuilds hit the end users.
* [ ] add the previous release to https://owncloud.org/download/older-versions/

A few days after the release (for final release)
* [ ] Review changes in the release branch, merge back into master
* [ ] check the crash reporter if auto update is a good idea or we need a new release
* [ ] Update the owncloud hosted auto updater according to https://github.com/owncloud/enterprise/blob/master/client_update_checker/README.md#deploy  
* [ ] Increment version number in daily builds. Special case: after the last release in a branch, jump forward to the 'next release branch'... That may mean, this is nightly is the same as edge then.

