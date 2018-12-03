<!--
This is the template for new release issues.
(originated from https://github.com/owncloud/client/wiki/Release%20Checklist%20Template)
(20181109jw: One of these two should be deleted. They will never be in sync otherwise.)
-->

Copy below text into a task and tick the items:

```
Some weeks before the release:
* [ ] Check if we should update the bundled sqlite3 (https://github.com/owncloud/client/tree/master/src/3rdparty/sqlite3)
* [ ] Check if we should update Sparkle on build machine (https://github.com/sparkle-project/Sparkle/releases)
* [ ] Ensure NSIS is up to date on the build machine
* [ ] Ensure up-to-date dependencies (e.g. [latest Qt version](http://qt-project.org/downloads#qt-lib) is installed on the machine and picked up (cmake output)
* [ ] Ensure the crash reporter server is up
* [ ] Check crash reporter for bad crashes
* [ ] Ensure Windows Overlay DLLs are rebuilt. Why? How?
* [ ] Check daily builds are up and running, that is Jenkins.int jobs client-linux, client-osx and client-win32-msvc, client-win32--msvc-msi all green.
* [ ] Ensure Linux daily builds are built too for all distros https://build.opensuse.org/package/show/isv:ownCloud:desktop:daily:2.X/owncloud-client
* [ ] Build branded clients through the scripting machine and smoke test one or two branded clients (especially with predefined url)
* [ ] Upload a daily build of the windows version to virustotal.com
  * Contact AV vendors whom's engine reports a virus
* [ ] Documentation should be online before the release http://doc.owncloud.org/desktop/2.X/
* [ ] QA goes over https://github.com/owncloud/client/wiki/Testing-Scenarios
* [ ] Make sure to have `client/ChangeLog` updated
 * use `git log --format=oneline v<lastrelease>...master` if your memory fails you
* [ ] check if enterprise issues are fixed

For first Alpha/Beta of a Major or Minor release:
* [ ] branch off master to new version branch (e.g. master -> 2.1, when releasing 2.1)
* [ ] Adjust `VERSION.cmake` in master and count up (e.g. 2.2)
* [ ] Add the new branch v2.X.X for the new version to gitea/jw/client-linux-build
* [ ] Add the new version to gitea/ownbrander/scripting/client-linux/templates/client/2.X.X
* [ ] Add branch to branches.only section in appveyor.yml, so PRs to that branch will be built by AppVeyor
* [ ] Adjust translation jobs for [client](https://ci.owncloud.org/view/translation-sync/job/translation-sync-client/) and [NSIS](https://ci.owncloud.org/view/translation-sync/job/translation-sync-client-nsis/) to point to the release branch (e.g. 2.1).
* [ ] Make sure there is a job for the docs of the new master branch and the current release branch on rotor e.g. http://doc.owncloud.org/desktop/1.X/ exists

For all alphas, betas and RCs (Copy this section for each alpha/beta/rc):
* [ ] Add last updates to Changelog in the client source repository.
* [ ] Branch off a release branch called VERSION-rcX or VERSION-betaX  (without v, v is for tags)
* [ ] Edit ```VERSION.cmake``` to set the suffix to beta1, beta2 etc. Commit the result to the release branch only
* [ ] Make sure to increase the version number of the branched of release, e.g. if you release 2.3.2 then you should change VERSION.cmake in 2.3 to 2.3.3 since that branch now will be 2.3.3
* [ ] Create build for theme 'ownCloud' using client-trigger (uncheck the "daily build" checkbox, use rcX or betaX dropdown for version suffix)
* [ ] Create build for theme 'testpilotcloud' using client-trigger (uncheck the "daily build" checkbox, use the rcX or betaX dropdown for version suffix)
* Build results are in https://download.owncloud.com/desktop/testing -- win and mac binaries are there, linux packages are listed in a *repo.html file.
* [ ] Check if *tar.xz.asc files are there. If not resort to https://github.com/owncloud/enterprise/wiki/Desktop-Signing-Knowledge
* [ ] Mac: Perform smoke test (Install, make sure it does not explode, and check if all version indicators are correct)
* [ ] Win: Perform smoke test (Install, make sure it does not explode, and check if all version indicators are correct)
* [ ] Linux: Smoke test of one distro package (Install, make sure it does not explode, and check if all version indicators are correct)
* [ ] Linux: Run https://jenkins.int.owncloud.com/job/client-linux-tests/ with REPO_URL=https://download.opensuse.org/repositories/isv:/ownCloud:/desktop:/testing
* [ ] Linux: add/remove build targets in isv:ownCloud:Qt5101 and isv:ownCloud:desktop:testing to match the list of supported platforms and announced(!) deprecations. Keep in sync with https://doc.owncloud.org/server/latest/admin_manual/installation/system_requirements.html#desktop
* [ ] Create a signed tag using ```git tag -u E94E7B37 tagname``` (https://github.com/owncloud/enterprise/wiki/Desktop-Signing-Knowledge)
* [ ] update the wordpress content at owncloud.org/download (Attention: No staging!)
* [ ] Inform packagers @dragotin (openSUSE), @hefee (Debian), ??? (Fedora)
* [ ] Announce on https://central.owncloud.org
* [ ] Inform community mailinglists devel@owncloud.org and testpilots@owncloud.org (make sure to mention it is an rc). Link to the central post so discussion happens there.
* [ ] Check crash reporter

One week before the final release:
* [ ] Communicate the release schedule on mailinglist release-coordination@owncloud.com. Give a high level overview of the upcoming new features, changes etc.
* [ ] Ensure marketing is aware (marketing@owncloud.com) and prepared for the release (social, .com website, cust.communications)
* [ ] Inform GCX knows the next version is about 1 week out (gcx@owncloud.com)

Day before final Release:
* [ ] Check the translations coming from transifex: All synchronized? (20181109jw: where? how?)
* [ ] Run the tx.pl scripts on the final code tag (20181109jw: really? What does that test?)
* [ ] Run ```make test```
* [ ] Run smashbox (20180719 jw: FIXME: add details, how?)
* [ ] Inform product management and marketing that we are 1 day out

On Release Day (for final release):
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
* [ ] Mac: Perform smoke test (Install, make sure it does not explode, and check if all version indicators are correct)
* [ ] Win: Perform smoke test (Install, make sure it does not explode, and check if all version indicators are correct)
* [ ] Linux: Smoke test of one distro package (Install, make sure it does not explode, and check if all version indicators are correct)
* [ ] Linux: Run https://jenkins.int.owncloud.com/job/client-linux-tests/ with REPO_URL=https://download.opensuse.org/repositories/isv:/ownCloud:/desktop:/testing
* [ ] Win/Mac Copy builds from ```testing``` to ```stable``` on download.owncloud.com, double check the download links. (make sure the .asc is there too)
* [ ] Linux: disable publishing on project isv:ownCloud:desktop
* [ ] Linux: copy from testing to released in OBS:
  ```obs-deepcopy-prj.sh isv:ownCloud:desktop:testing isv:ownCloud:desktop```
  ```obs-deepcopy-prj.sh isv:ownCloud:testpilot:testing isv:ownCloud:testpilot```
* [ ] Linux: Re-enable OBS publishing on the project after official release date and if all distros build (check for accidentially disabled packages too) 
* [ ] Linux: Wait until everything is built and published, then disable publishing on project isv:ownCloud:desktop
* [ ] Create git signed tag in client repository using ```git tag -u E94E7B37 tagname``` (https://github.com/owncloud/enterprise/wiki/Desktop-Signing-Knowledge)
* [ ] Create a (draft) release on https://github.com/owncloud/client/releases
* [ ] Update https://owncloud.org/changelog/desktop-client/
* [ ] Update https://owncloud.org/download/#owncloud-desktop-client -> Download ownCloud -> click open 'Desktop Client', edit win/mac/lin, each all three tabs "Production", "Technical Preview" [disabled], "Test pilot" enabled, edit the links.
* [ ] Announce on https://central.owncloud.org
* [ ] Announce on announcements@owncloud.org Link to the central post so discussion happens there.
* [ ] Inform packagers @dragotin (openSUSE) @hefee (Debian) @Germano0 (Fedora)
* [ ] Send out Social (tweet, blog, other)
* [ ] Send out customer communication (if any)
* [ ] Inform GCX that the new version is released (gcx@owncloud.com)
* [ ] Inform release-coordination@owncloud.com
* [ ] Ensure marketing is aware (marketing@owncloud.com)
* [ ] Take pride and celebrate!
* [ ] Tell GCX to increment the minimum supported version for enterprise customers
* [ ] Check if minimum.supported.desktop.version (https://github.com/owncloud/core/blob/master/config/config.sample.php#L1152) needs to be updated in server

15 minutes after after release:
* [ ] Test all advertised download links to have the expected version
* [ ] Check for build errors in OBS, do
```obs-deepcopy-prj.sh isv:ownCloud:desktop isv:ownCloud:desktop:client-2.X.X```
* [ ] disable publishing in OBS to prevent that accidential rebuilds hit the end users.
* [ ] add the previous release to https://owncloud.org/download/older-versions/

A few days after the release (for final release)
* [ ] Review changes in the release branch, merge back into master
* [ ] check the crash reporter if auto update is a good idea or we need a new release
* [ ] Update the owncloud hosted updater according to https://github.com/owncloud/enterprise/blob/master/client_update_checker/README.md#deploy  
* [ ] Increment version number in daily builds. Special case: after the last release in a branch, jump forward to the 'next release branch'... That may mean, this is nightly is the same as edge then.

```
