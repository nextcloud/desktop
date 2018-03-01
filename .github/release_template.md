<!--
This is the template for new release issues.
(originated from https://github.com/owncloud/client/wiki/Release%20Checklist%20Template)
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
* [ ] Ensure Windows Overlay DLLs are rebuilt
* [ ] Check nightly builds are up and running, that is Jenkins jobs ownCloud-client-linux, ownCloud-client-osx and ownCloud-client-win32 all green.
* [ ] Ensure Linux nightlies are built too for all distros https://build.opensuse.org/package/show/isv:ownCloud:community:nightly/owncloud-client
  * Check if patches still apply in the linux packages
* [ ] Build branded clients through the scripting machine and smoke test one or two branded clients (especially with predefined url)
* [ ] Upload a nightly build of the windows version to virustotal.com
  * Contact AV vendors whom's engine reports a virus
* [ ] Documentation should be online before the release http://doc.owncloud.org/desktop/1.X/
* [ ] QA goes over https://github.com/owncloud/mirall/wiki/Testing-Scenarios
* [ ] Make sure to have `client/ChangeLog` updated
 * use `git log --format=oneline v<lastrelease>...master` if your memory fails you
* [ ] check if enterprise issues are fixed

For first Alpha/Beta of a Major or Minor release:
* [ ] branch off master to new version branch (e.g. master -> 2.1, when releasing 2.1)
* [ ] Adjust `VERSION.cmake` in master and count up (e.g. 2.2)
* [ ] Adjust translation jobs for [client](https://ci.owncloud.org/view/translation-sync/job/translation-sync-client/) and [NSIS](https://ci.owncloud.org/view/translation-sync/job/translation-sync-client-nsis/) to point to the release branch (e.g. 2.1).
* [ ] Make sure there is a job for the docs of the new master branch and the current release branch on rotor e.g. http://doc.owncloud.org/desktop/1.X/ exists

For all alphas, betas and RCs (Copy this section for each alpha/beta/rc):
* [ ] Add last updates to Changelog in the client source repository.
* [ ] Branch off a release branch called VERSION-rcX or VERSION-betaX  (without v, v is for tags)
* [ ] Edit ```VERSION.cmake``` to set the suffix to beta1, beta2 etc. Commit the result to the release branch only
* [ ] Make sure to increase the version number of the branched of release, e.g. if you release 2.3.2 then you should change VERSION.cmake in 2.3 to 2.3.3 since that branch now will be 2.3.3
* [ ] Create build for using owncloud-client-trigger (uncheck the "nightly build" checkbox, use the proper dropdown for version suffix) for theme 'ownCloud'
* [ ] Create build for using owncloud-client-trigger (uncheck the "nightly build" checkbox, use the proper dropdown for version suffix) for theme 'testpilotcloud'
* [ ] Only now download the last created source .tar.xz and sign it with gpg. Copy the signature into a new .asc file. (timing issue because currently 'testpilotcloud' re-creates the source .tar.xz)
* (no need to copy builds as they are already in testing directory or repository) (https://github.com/owncloud/enterprise/wiki/Desktop-Signing-Knowledge)
* [ ] Mac: Perform smoke test (Install, make sure it does not explode, and check if all version indicators are correct)
* [ ] Win: Perform smoke test (Install, make sure it does not explode, and check if all version indicators are correct)
* [ ] Linux: Smoke test of one distro package (Install, make sure it does not explode, and check if all version indicators are correct)
* [ ] Linux: Run @SamuAlfageme 's magic Linux-test-all-packages-script
* [ ] Create a signed tag using ```git tag -u E94E7B37 tagname``` (https://github.com/owncloud/enterprise/wiki/Desktop-Signing-Knowledge)
* [ ] Create a pull request to the owncloud.org repository to update the install page (strings.php, page-desktop.php) and the changelog on owncloud.org. From now on download packages from the staging webserver.
* [ ] Inform packagers @dragotin (openSUSE) @hefee (Debian) @Germano0 (Fedora)
* [ ] Announce on https://central.owncloud.org
* [ ] Inform community mailinglists devel@owncloud.org and testpilots@owncloud.org (make sure to mention it is an rc). Link to the central post so discussion happens there.
* [ ] Check crash reporter

One week before the final release:
* [ ] Communicate the release schedule on mailinglist release-coordination@owncloud.com. Give a high level overview of the upcoming new features, changes etc.
* [ ] Ensure marketing is aware (marketing@owncloud.com) and prepared for the release (social, .com website, cust. communications)
* [ ] Inform GCX knows the next version is about 1 week out (gcx@owncloud.com)

Day before final Release:
* [ ] Check the translations coming from transifex: All synchronized?
* [ ] Run the tx.pl scripts on the final code tag
* [ ] Run ```make test```
* [ ] Run smashbox
* [ ] Inform product management and marketing that we are 1 day out

On Release Day (for final release):
* [ ] Add last updates to Changelog in the client source repository.
* [ ] Branch off a release branch called VERSION  (without v, v is for tags)
* [ ] Edit ```VERSION.cmake``` to set the suffix to "" etc. Commit the result to the release branch only
* [ ] Create build for using owncloud-client-trigger (uncheck the "nightly build" checkbox, use the proper dropdown for version suffix) for theme 'ownCloud'
* [ ] Create build for using owncloud-client-trigger (uncheck the "nightly build" checkbox, use the proper dropdown for version suffix) for theme 'testpilotcloud'
* [ ] Only now download the last created source .tar.xz and sign it with gpg. Copy the signature into a new .asc file. (timing issue because currently 'testpilotcloud' re-creates the source .tar.xz) (https://github.com/owncloud/enterprise/wiki/Desktop-Signing-Knowledge)
* [ ] Branch isv:ownCloud:desktop to isv:ownCloud:desktop:client-X.Y.Z before overwriting https://github.com/owncloud/administration/blob/master/jenkins/obs_integration/obs-backup-prj.sh (the linux packages will land in the :testing repository still)
  * Update [OBS repository](https://build.opensuse.org/project/show?project=isv%3AownCloud%3Adesktop) `isv:ownCloud:desktop`
* [ ] Re-download Mac builds and check signature. Interactive in installer window
* [ ] Re-download Win build check signature. From Mac or Linux: ```osslsigncode verify ownCloud-version-setup.exe```
* [ ] Mac: Perform smoke test (Install, make sure it does not explode, and check if all version indicators are correct)
* [ ] Win: Perform smoke test (Install, make sure it does not explode, and check if all version indicators are correct)
* [ ] Linux: Smoke test of one distro package (Install, make sure it does not explode, and check if all version indicators are correct)
* [ ] Linux: Run @SamuAlfageme 's client-linux-tests Jenkins job (this tests only package installations!) (maybe adjust REPO_URL so it takes it from OBS :testing repository..?)
* [ ] Win/Mac Copy builds from ```testing``` to ```stable``` on download.owncloud.com, double check the download links. (make sure the .asc is there too)
* [ ] Linux: disable publishing on project isv:ownCloud:desktop
* [ ] Linux: Use https://github.com/owncloud/administration/blob/master/jenkins/obs_integration/obs-deepcopy-prj.sh to copy from isv:ownCloud:community:testing to isv:ownCloud:desktop
* [ ] Linux: Re-enable OBS publishing on the project after official release date and if all distros build (check for accidentially disabled packages too) 
* [ ] Linux: Wait until everything is built and published, then disable publishing on project isv:ownCloud:desktop
* [ ] Create git signed tag in client repository using ```git tag -u E94E7B37 tagname``` (https://github.com/owncloud/enterprise/wiki/Desktop-Signing-Knowledge)
* [ ] Create a (draft) release on https://github.com/owncloud/client/releases
* [ ] Update https://owncloud.org/changelog/desktop-client/
* [ ] Update https://owncloud.org/download/#owncloud-desktop-client
* [ ] Announce on https://central.owncloud.org
* [ ] Announce on announcements@owncloud.org Link to the central post so discussion happens there.
* [ ] Inform packagers @dragotin (openSUSE) @hefee (Debian) @Germano0 (Fedora)
* [ ] Send out Social (tweet, blog, other)
* [ ] Send out customer communication (if any)
* [ ] Inform GCX that the new version is released (gcx@owncloud.com)
* [ ] Inform release-coordination@owncloud.com
* [ ] Ensure marketing is aware (marketing@owncloud.com)
* [ ] Take pride and celebrate!
* [ ] Also update the testpilotcloud builds for that release version and make sure they show up on the download page
* [ ] Tell GCX to increment the minimum supported version for enterprise customers
* [ ] Check if minimum.supported.desktop.version (https://github.com/owncloud/core/blob/master/config/config.sample.php#L1152) needs to be updated in server
* [ ] Linux OBS: Update the testing repository to the latest stable version.

15 minutes after after release:
* [ ] Test all advertised download links to have the expected version
* [ ] Check for build errors in OBS
* [ ] disable publishing in OBS to prevent that accidential rebuilds hit the end users.

A few days after the release (for final release)
* [ ] Review changes in the release branch, merge back into master
* [ ] check the crash reporter if auto update is a good idea or we need a new release
* [ ] Update the updater script ```clientupdater.php```
* [ ] Execute announced deprecations. Disable builds for deprecated platforms. Update accordingly: https://doc.owncloud.org/server/latest/admin_manual/installation/system_requirements.html#desktop
* [ ] Increment version number in nightly builds. Special case: after the last release in a branch, jump forward to the 'next release branch'... That may mean, this is nightly is the same as edge then.

```
