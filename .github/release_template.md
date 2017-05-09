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
* [ ] Build branded clients through the scripting machine and smoke test one or two branded clients (especially with predefined url)
* [ ] Upload a nightly build of the windows version to virustotal.com
  * Contact AV vendors whom's engine reports a virus
* [ ] Documentation should be online before the release http://doc.owncloud.org/desktop/1.X/
* [ ] QA goes over https://github.com/owncloud/mirall/wiki/Testing-Scenarios
* [ ] Make sure to have `client/ChangeLog` updated
 * use `git log --format=oneline v<lastrelease>...master` if your memory fails you
* [ ] check if enterprise issues are fixed

One week before the release:
* [ ] Communicate the release schedule on mailinglist release-coordination@owncloud.com. Give a high level overview of the upcoming new features, changes etc.
* [ ] Ensure marketing is aware (marketing@owncloud.com) and prepared for the release (social, .com website, cust. communications)
* [ ] Inform GCX knows the next version is about 1 week out (gcx@owncloud.com)

For all Betas and RCs:
* [ ] Branch off a release branch called VERSION-rcX or VERSION-betaX  (without v, v is for tags)
* [ ] Edit ```VERSION.cmake``` to set the suffix to beta1, beta2 etc. Commit the result to the release branch only
* [ ] Create build for Windows using rotor job owncloud-client-win32 (uncheck the "nightly build" checkbox, check the "sign package" checkboxes) both themes 'ownCloud' and 'testpilotcould'
* [ ] Create build for Mac using rotor, job owncloud-client-osx (uncheck the "nightly build" checkbox, check the "sign package" checkboxes) both themes 'ownCloud' and 'testpilotcould'
* [ ] Create the beta tarball using Jenkins job ownCloud-client-source
* [ ] Create Linux builds using rotor job owncloud-client-linux building (this magically interacts with the ownCloud-client-source job)
  * [ ] theme 'ownCloud' -> isv:ownCloud:community:testing
  * [ ] theme 'testpilotcould' -> isv:ownCloud:testpilot:testing
* [ ] Copy builds from ```daily``` to ```testing``` on download.owncloud.com, double check the download links.
* [ ] Create a pull request to the owncloud.org repository to update the install page (strings.php, page-desktop.php) and the changelog on owncloud.org. From now on download packages from the staging webserver.
* [ ] Inform community mailinglists devel@owncloud.org and testpilots@owncloud.org
* [ ] Announce on https://central.owncloud.org
* [ ] Create a signed tag using ```git tag -u E94E7B37 tagname``` (https://github.com/owncloud/enterprise/wiki/Desktop-Signing-Knowledge)
* [ ] Check crash reporter

For first Beta of a Major or Minor release:
* [ ] branch off master to new version branch (e.g. master -> 2.1, when releasing 2.1)
* [ ] Adjust `VERSION.cmake` in master and count up (e.g. 2.2)
* [ ] Adjust translation jobs for [client](https://ci.owncloud.org/view/translation-sync/job/translation-sync-client/) and [NSIS](https://ci.owncloud.org/view/translation-sync/job/translation-sync-client-nsis/) to point to the release branch (e.g. 2.1).
* [ ] Make sure there is a job for the docs of the new master branch and the current release branch on rotor.

Day before Release:
* [ ] Check the translations coming from transifex: All synchronized?
* [ ] Run the tx.pl scripts on the final code tag
* [ ] Run ```make test```
* [ ] Run smashbox on the final code tag
* [ ] Inform product management and marketing that we are 1 day out

On Release Day (for final release):
* [ ] Branch off a release branch called like the version (without v, v is for tags)
* [ ] Double check ```VERSION.cmake```: Check the version number settings and suffix (beta etc.) to be removed. Commit change to release branch only!
* [ ] Make sure to increase the version number of the branched of release, e.g. if you release 2.3.2 then you should change VERSION.cmake in 2.3 to 2.3.3 since that branch now will be 2.3.3
* [ ] Add last updates to Changelog in the client source repository.
* [ ] Create tar ball (automated by `ownCloud-client-source` jenkins job) and **immediately** sign it (asc file). (https://github.com/owncloud/enterprise/wiki/Desktop-Signing-Knowledge)
* [ ] Create build for Windows using rotor job owncloud-client-win32 (uncheck the "nightly build" checkbox, check the "sign package" checkboxes) both themes 'ownCloud' and 'testpilotcould'
* [ ] Create build for Mac using rotor, job owncloud-client-osx (uncheck the "nightly build" checkbox, check the "sign package" checkboxes) both themes 'ownCloud' and 'testpilotcould'
* [ ] Stop publishing on OBS
* [ ] Branch isv:ownCloud:desktop to isv:ownCloud:desktop:client-X.Y.Z before overwriting https://github.com/owncloud/administration/blob/master/jenkins/obs_integration/obs-backup-prj.sh
* [ ] Create Linux builds using rotor job owncloud-client-linux (this magically interacts with the ownCloud-client-source job)
  * Check if patches still apply in the linux packages
  * Update [OBS repository](https://build.opensuse.org/project/show?project=isv%3AownCloud%3Adesktop) `isv:ownCloud:desktop`
  * [ ] theme 'ownCloud' -> isv:ownCloud:desktop
  * [ ] theme 'testpilotcloud' -> isv:ownCloud:testpilot
* [ ] Linux: Update the testing repository to the latest stable version.
* [ ] Inform GCX that a new tarball is available.
* [ ] Copy builds and source tar ball from ```daily``` to ```stable``` on download.owncloud.com, double check the download links.
* [ ] Check if the following packages are on download.owncloud.com/desktop/stable:
  * Windows binary package
  * Mac binary package
  * source tarballs
* [ ] Create a pull request to the owncloud.org repository to update the install page (strings.php, page-desktop.php) and the changelog on owncloud.org. From now on download packages from the staging webserver.
* [ ] Re-download Mac builds and check signature. Interactive in installer window
* [ ] Re-download Win build check signature. From Mac or Linux: ```osslsigncode verify ownCloud-version-setup.exe```
* [ ] Mac: Perform smoke test (Install, make sure it does not explode, and check if all version indicators are correct)
* [ ] Win: Perform smoke test (Install, make sure it does not explode, and check if all version indicators are correct)
* [ ] Linux: Smoke test 
* [ ] Linux: Re-enable OBS publishing
* [ ] Update ASCII Changelog on http://download.owncloud.com/download/changelog-client
* [ ] Announce on https://central.owncloud.org
* [ ] Announce on announcements@owncloud.org
* [ ] Create git signed tag in client repository using ```git tag -u E94E7B37 tagname```
* [ ] Send out Social (tweet, blog, other)
* [ ] Send out customer communication (if any)
* [ ] Inform GCX that the new version is released (gcx@owncloud.com)
* [ ] Inform release-coordination@owncloud.com
* [ ] Ensure marketing is aware (marketing@owncloud.com)
* [ ] Take pride and celebrate!
* [ ] Also update the testpilotcloud builds for that release version and make sure they show up on the download page
* [ ] Days later: Update the updater script ```clientupdater.php``` (check the crash reporter if auto update is a good idea or we need a new release)
* [ ] Tell GCX to increment the minimum supported version for enterprise customers
* [ ] Check if minimum.supported.desktop.version (https://github.com/owncloud/core/blob/master/config/config.sample.php#L1152) needs to be updated in server
```
