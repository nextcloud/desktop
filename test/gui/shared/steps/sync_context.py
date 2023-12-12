from pageObjects.SyncConnectionWizard import SyncConnectionWizard
from pageObjects.SyncConnection import SyncConnection
from pageObjects.Toolbar import Toolbar
from pageObjects.Activity import Activity
from pageObjects.Settings import Settings

from helpers.SetupClientHelper import getResourcePath
from helpers.ConfigHelper import get_config, isWindows
from helpers.SyncHelper import (
    waitForFileOrFolderToSync,
    waitForFileOrFolderToHaveSyncError,
)


@Given('the user has paused the file sync')
def step(context):
    SyncConnection.pauseSync()


@When('the user resumes the file sync on the client')
def step(context):
    SyncConnection.resumeSync()


@When('the user waits for the files to sync')
def step(context):
    waitForFileOrFolderToSync(getResourcePath('/'))


@When(r'the user waits for (file|folder) "([^"]*)" to be synced', regexp=True)
def step(context, type, resource):
    resource = getResourcePath(resource)
    waitForFileOrFolderToSync(resource, type)


@When(r'the user waits for (file|folder) "([^"]*)" to have sync error', regexp=True)
def step(context, type, resource):
    resource = getResourcePath(resource)
    waitForFileOrFolderToHaveSyncError(resource, type)


@When(
    r'user "([^"]*)" waits for (file|folder) "([^"]*)" to have sync error', regexp=True
)
def step(context, username, type, resource):
    resource = getResourcePath(resource, username)
    waitForFileOrFolderToHaveSyncError(resource, type)


@When("the user enables virtual file support")
def step(context):
    SyncConnection.enableVFS()
    # TODO: remove snooze with proper wait
    # let the client re-sync
    snooze(get_config("minSyncTimeout"))


@Then('the "|any|" button should be available')
def step(context, item):
    SyncConnection.openMenu()
    SyncConnection.hasMenuItem(item)


@When("the user disables virtual file support")
def step(context):
    SyncConnection.disableVFS()
    # TODO: remove snooze with proper wait
    # let the client re-sync
    snooze(get_config("minSyncTimeout"))


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
        Activity.checkBlackListedResourceExist(filename),
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
    folders = []
    for row in context.table[1:]:
        folders.append(row[0])
    SyncConnectionWizard.selectFoldersToSync(folders)
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


@When('the user sets the sync path in sync connection wizard')
def step(context):
    SyncConnectionWizard.setSyncPathInSyncConnectionWizard()


@When(
    'the user sets the temp folder "|any|" as local sync path in sync connection wizard'
)
def step(context, folderName):
    SyncConnectionWizard.setSyncPathInSyncConnectionWizard(folderName)


@When('the user selects "|any|" as a remote destination folder')
def step(context, folderName):
    SyncConnectionWizard.selectRemoteDestinationFolder(folderName)


@When('the user syncs the "|any|" space')
def step(context, spaceName):
    SyncConnectionWizard.syncSpace(spaceName)


@Then('the settings tab should have the following options in the general section:')
def step(context):
    for item in context.table:
        Settings.check_general_option(item[0])


@Then('the settings tab should have the following options in the advanced section:')
def step(context):
    for item in context.table:
        Settings.check_advanced_option(item[0])


@Then('the settings tab should have the following options in the network section:')
def step(context):
    for item in context.table:
        Settings.check_network_option(item[0])


@When('the user opens the about dialog')
def step(context):
    Settings.open_about_button()


@Then('the about dialog should be opened')
def step(context):
    Settings.wait_for_about_dialog_to_be_visible()


@When('the user adds the folder sync connection')
def step(context):
    SyncConnectionWizard.addSyncConnection()


@When("user unselects all the remote folders")
def step(context):
    SyncConnectionWizard.deselectAllRemoteFolders()


@When("the user |word| VFS support for Windows")
def step(context, action):
    if isWindows():
        action = action.rstrip("s")
        SyncConnectionWizard.enableOrDisableVfsSupport(action)
