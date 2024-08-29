import squish

from pageObjects.SyncConnectionWizard import SyncConnectionWizard
from pageObjects.SyncConnection import SyncConnection
from pageObjects.Toolbar import Toolbar
from pageObjects.Activity import Activity
from pageObjects.Settings import Settings

from helpers.ConfigHelper import get_config, isWindows, set_config
from helpers.SyncHelper import (
    waitForFileOrFolderToSync,
    waitForFileOrFolderToHaveSyncError,
)
from helpers.SetupClientHelper import (
    getTempResourcePath,
    setCurrentUserSyncPath,
    substituteInLineCodes,
    getResourcePath,
)


@Given('the user has paused the file sync')
def step(context):
    SyncConnection.pauseSync()


@When('the user resumes the file sync on the client')
def step(context):
    SyncConnection.resumeSync()


@When('the user force syncs the files')
def step(context):
    SyncConnection.forceSync()


@When('the user waits for the files to sync')
def step(context):
    waitForFileOrFolderToSync(getResourcePath('/'))


@When(r'the user waits for (file|folder) "([^"]*)" to be synced', regexp=True)
def step(context, resource_type, resource):
    resource = getResourcePath(resource)
    waitForFileOrFolderToSync(resource, resource_type)


@When(r'the user waits for (file|folder) "([^"]*)" to have sync error', regexp=True)
def step(context, resource_type, resource):
    resource = getResourcePath(resource)
    waitForFileOrFolderToHaveSyncError(resource, resource_type)


@When(
    r'user "([^"]*)" waits for (file|folder) "([^"]*)" to have sync error', regexp=True
)
def step(context, username, resource_type, resource):
    resource = getResourcePath(resource, username)
    waitForFileOrFolderToHaveSyncError(resource, resource_type)


@When('the user enables virtual file support')
def step(context):
    SyncConnection.enableVFS()
    # TODO: remove snooze with proper wait
    # let the client re-sync
    squish.snooze(get_config('minSyncTimeout'))


@Then('the "|any|" button should be available')
def step(context, item):
    SyncConnection.openMenu()
    SyncConnection.hasMenuItem(item)


@Then('the "|any|" button should not be available')
def step(context, item):
    SyncConnection.openMenu()
    test.compare(
        SyncConnection.menu_item_exists(item),
        False,
        f'Menu item "{item}" does not exist.',
    )


@When('the user disables virtual file support')
def step(context):
    SyncConnection.disableVFS()
    # TODO: remove snooze with proper wait
    # let the client re-sync
    squish.snooze(get_config('minSyncTimeout'))


@When('the user clicks on the activity tab')
def step(context):
    Toolbar.openActivity()


@Then('the table of conflict warnings should include file "|any|"')
def step(context, filename):
    Activity.checkFileExist(filename)


@Then('the file "|any|" should be blacklisted')
def step(context, filename):
    test.compare(True, Activity.resourceIsBlacklisted(filename), 'File is Blacklisted')


@Then('the file "|any|" should be ignored')
def step(context, filename):
    test.compare(True, Activity.resourceIsIgnored(filename), 'File is Ignored')


@Then('the file "|any|" should be excluded')
def step(context, filename):
    test.compare(True, Activity.resource_is_excluded(filename), 'File is Excluded')


@When('the user selects "|any|" tab in the activity')
def step(context, tabName):
    Activity.clickTab(tabName)


@Then('the toolbar should have the following tabs:')
def step(context):
    for tabName in context.table:
        Toolbar.hasItem(tabName[0])


@When('the user selects the following folders to sync:')
def step(context):
    folders = []
    for row in context.table[1:]:
        folders.append(row[0])
    SyncConnectionWizard.selectFoldersToSync(folders)
    SyncConnectionWizard.addSyncConnection()


@When('the user sorts the folder list by "|any|"')
def step(context, headerText):
    if (headerText := headerText.capitalize()) in ['Size', 'Name']:
        SyncConnectionWizard.sortBy(headerText)
    else:
        raise ValueError("Sorting by '" + headerText + "' is not supported.")


@Then('the sync all checkbox should be checked')
def step(context):
    test.compare(
        SyncConnectionWizard.isRootFolderChecked(), True, 'Sync all checkbox is checked'
    )


@Then('the folders should be in the following order:')
def step(context):
    rowIndex = 0
    for row in context.table[1:]:
        expectedFolder = row[0]
        actualFolder = SyncConnectionWizard.getItemNameFromRow(rowIndex)
        test.compare(actualFolder, expectedFolder)

        rowIndex += 1


@When('the user selects "|any|" space in sync connection wizard')
def step(context, space_name):
    if get_config('ocis'):
        SyncConnectionWizard.selectSpaceToSync(space_name)
        SyncConnectionWizard.nextStep()
        set_config('syncConnectionName', space_name)


@When('the user sets the sync path in sync connection wizard')
def step(context):
    SyncConnectionWizard.setSyncPathInSyncConnectionWizard()


@When(
    'the user sets the temp folder "|any|" as local sync path in sync connection wizard'
)
def step(context, folderName):
    sync_path = getTempResourcePath(folderName)
    SyncConnectionWizard.setSyncPathInSyncConnectionWizard(sync_path)
    if get_config('ocis'):
        # empty connection name when using temporary locations
        set_config('syncConnectionName', '')
        setCurrentUserSyncPath(sync_path)


@When('the user selects "|any|" as a remote destination folder')
def step(context, folderName):
    # There's no remote destination section with oCIS server
    if not get_config('ocis'):
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


@When('user unselects all the remote folders')
def step(context):
    SyncConnectionWizard.deselectAllRemoteFolders()


@When('the user |word| VFS support for Windows')
def step(context, action):
    if isWindows():
        action = action.rstrip('s')
        SyncConnectionWizard.enableOrDisableVfsSupport(action)


@When('user unselects a folder "|any|" in selective sync')
def step(context, folder_name):
    SyncConnection.choose_what_to_sync()
    SyncConnection.unselect_folder_in_selective_sync(folder_name)


@Then('the sync folder list should be empty')
def step(context):
    test.compare(
        0,
        SyncConnection.get_folder_connection_count(),
        'Sync connections should be empty',
    )


@When('the user navigates back in the sync connection wizard')
def step(context):
    SyncConnectionWizard.back()


@When('the user creates a folder "|any|" in the remote destination wizard')
def step(context, folder_name):
    if not get_config('ocis'):
        SyncConnectionWizard.create_folder_in_remote_destination(folder_name)


@When('the user refreshes the remote destination in the sync connection wizard')
def step(context):
    if not get_config('ocis'):
        SyncConnectionWizard.refresh_remote()


@Then('the folder "|any|" should be present in the remote destination wizard')
def step(context, folder_name):
    if not get_config('ocis'):
        has_folder, folder_selector = SyncConnectionWizard.has_remote_folder(
            folder_name
        )
        test.compare(True, has_folder, 'Folder should be in the remote list')


@When('the user selects remove folder sync connection option')
def step(context):
    SyncConnection.remove_folder_sync_connection()


@When('the user cancels the folder sync connection removal dialog')
def step(context):
    SyncConnection.cancel_folder_sync_connection_removal()


@When('the user removes the folder sync connection')
def step(context):
    SyncConnection.remove_folder_sync_connection()
    SyncConnection.confirm_folder_sync_connection_removal()


@Then('the file "|any|" should have status "|any|" in the activity tab')
def step(context, file_name, status):
    Activity.hasSyncStatus(file_name, status)


@When('the user selects the "|any|" space to sync')
def step(context, space_name):
    SyncConnectionWizard.select_space_to_sync(space_name)


@When('the user opens the sync connection wizard')
def step(context):
    SyncConnectionWizard.open_sync_connection_wizard()


@Then('the button to open sync connection wizard should be disabled')
def step(context):
    test.compare(
        False,
        SyncConnectionWizard.is_add_sync_folder_button_enabled(),
        'Button to open sync connection wizard should be disabled',
    )


@When('the user checks the activities of account "|any|"')
def step(context, account):
    account = substituteInLineCodes(account)
    Activity.select_synced_filter(account)


@Then('the following activities should be displayed in synced table')
def step(context):
    for row in context.table[1:]:
        resource = row[0]
        action = row[1]
        account = substituteInLineCodes(row[2])
        test.compare(
            Activity.check_synced_table(resource, action, account),
            True,
            'Resource should be displayed in the synced table',
        )


@Then('the following activities should be displayed in not synced table')
def step(context):
    for row in context.table[1:]:
        resource = row[0]
        status = row[1]
        account = substituteInLineCodes(row[2])
        test.compare(
            Activity.check_not_synced_table(resource, status, account),
            True,
            'Resource should be displayed in the not synced table',
        )


@When('the user unchecks the "|any|" filter')
def step(context, filter_option):
    Activity.select_not_synced_filter(filter_option)
