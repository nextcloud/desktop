from os.path import join

from pageObjects.SyncConnectionWizard import SyncConnectionWizard
from pageObjects.SyncConnection import SyncConnection
from pageObjects.Toolbar import Toolbar
from pageObjects.Activity import Activity

from helpers.SetupClientHelper import getUserSyncPath
from helpers.SyncHelper import (
    waitForFileOrFolderToSync,
    waitForFileOrFolderToHaveSyncError,
)


@Given('the user has paused the file sync')
def step(context):
    SyncConnection.pauseSync(context)


@When('the user resumes the file sync on the client')
def step(context):
    SyncConnection.resumeSync(context)


@When('the user waits for the files to sync')
def step(context):
    waitForFileOrFolderToSync(context)


@When(r'the user waits for (file|folder) "([^"]*)" to be synced', regexp=True)
def step(context, type, resource):
    waitForFileOrFolderToSync(context, resource, type)


@When(r'the user waits for (file|folder) "([^"]*)" to have sync error', regexp=True)
def step(context, type, resource):
    waitForFileOrFolderToHaveSyncError(context, resource, type)


@When(
    r'user "([^"]*)" waits for (file|folder) "([^"]*)" to have sync error', regexp=True
)
def step(context, username, type, resource):
    resource = join(getUserSyncPath(context, username), resource)
    waitForFileOrFolderToHaveSyncError(context, resource, type)


@When("the user enables virtual file support")
def step(context):
    SyncConnection.enableVFS(context)


@Then('the "|any|" button should be available')
def step(context, item):
    SyncConnection.openMenu(context)
    SyncConnection.hasMenuItem(item)


@Given("the user has enabled virtual file support")
def step(context):
    SyncConnection.enableVFS(context)


@When("the user disables virtual file support")
def step(context):
    SyncConnection.disableVFS(context)


@When('the user clicks on the activity tab')
def step(context):
    Toolbar.openActivity()


@Then('the table of conflict warnings should include file "|any|"')
def step(context, filename):
    Activity.checkFileExist(filename)


@Then('the file "|any|" should be blacklisted')
def step(context, filename):
    test.compare(
        True,
        Activity.checkBlackListedResourceExist(context, filename),
        "File is blacklisted",
    )


@When('the user selects "|any|" tab in the activity')
def step(context, tabName):
    Activity.clickTab(tabName)


@Then("the following tabs in the toolbar should match the default baseline")
def step(context):
    for tabName in context.table:
        test.vp(tabName[0])


@When('the user selects the following folders to sync:')
def step(context):
    SyncConnectionWizard.selectFoldersToSync(context)
    SyncConnectionWizard.addSyncConnection()


@When('the user sorts the folder list by "|any|"')
def step(context, headerText):
    headerText = headerText.capitalize()
    if headerText in ["Size", "Name"]:
        SyncConnectionWizard.sortBy(headerText)
    else:
        raise Exception("Sorting by '" + headerText + "' is not supported.")


@Then('the sync all checkbox should be checked')
def step(context):
    test.compare(
        SyncConnectionWizard.isRootFolderChecked(), True, "Sync all checkbox is checked"
    )


@Then("the folders should be in the following order:")
def step(context):
    rowIndex = 0
    for row in context.table[1:]:
        expectedFolder = row[0]
        actualFolder = SyncConnectionWizard.getItemNameFromRow(rowIndex)
        test.compare(actualFolder, expectedFolder)

        rowIndex += 1


@Then('VFS enabled baseline image should match the default screenshot')
def step(context):
    if context.userData['ocis']:
        test.vp("VP_VFS_enabled_oCIS")
    else:
        test.vp("VP_VFS_enabled")


@Then('VFS enabled baseline image should not match the default screenshot')
def step(context):
    if context.userData['ocis']:
        test.xvp("VP_VFS_enabled_oCIS")
    else:
        test.xvp("VP_VFS_enabled")


@When('the user sets the sync path in sync connection wizard')
def step(context):
    SyncConnectionWizard.setSyncPathInSyncConnectionWizard(context)


@When('the user selects "|any|" as a remote destination folder')
def step(context, folderName):
    SyncConnectionWizard.selectRemoteDestinationFolder(folderName)
