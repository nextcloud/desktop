import squish

from pageObjects.SyncConnectionWizard import SyncConnectionWizard
from pageObjects.SyncConnection import SyncConnection
from pageObjects.Toolbar import Toolbar
from pageObjects.Activity import Activity
from pageObjects.Settings import Settings

from helpers.ConfigHelper import get_config, is_windows, set_config
from helpers.SyncHelper import (
    wait_for_resource_to_sync,
    wait_for_resource_to_have_sync_error,
)
from helpers.SetupClientHelper import (
    get_temp_resource_path,
    set_current_user_sync_path,
    substitute_inline_codes,
    get_resource_path,
)


@Given('the user has paused the file sync')
def step(context):
    SyncConnection.pause_sync()


@When('the user resumes the file sync on the client')
def step(context):
    SyncConnection.resume_sync()


@When('the user force syncs the files')
def step(context):
    SyncConnection.force_sync()


@When('the user waits for the files to sync')
def step(context):
    wait_for_resource_to_sync(get_resource_path('/'))


@When(r'the user waits for (file|folder) "([^"]*)" to be synced', regexp=True)
def step(context, resource_type, resource):
    resource = get_resource_path(resource)
    wait_for_resource_to_sync(resource, resource_type)


@When(r'the user waits for (file|folder) "([^"]*)" to have sync error', regexp=True)
def step(context, resource_type, resource):
    resource = get_resource_path(resource)
    wait_for_resource_to_have_sync_error(resource, resource_type)


@When(
    r'user "([^"]*)" waits for (file|folder) "([^"]*)" to have sync error', regexp=True
)
def step(context, username, resource_type, resource):
    resource = get_resource_path(resource, username)
    wait_for_resource_to_have_sync_error(resource, resource_type)


@When('the user enables virtual file support')
def step(context):
    SyncConnection.enable_vfs()
    # TODO: remove snooze with proper wait
    # let the client re-sync
    squish.snooze(get_config('minSyncTimeout'))


@Then('the "|any|" button should be available')
def step(context, item):
    SyncConnection.open_menu()
    SyncConnection.has_menu_item(item)


@Then('the "|any|" button should not be available')
def step(context, item):
    SyncConnection.open_menu()
    test.compare(
        SyncConnection.menu_item_exists(item),
        False,
        f'Menu item "{item}" does not exist.',
    )


@When('the user disables virtual file support')
def step(context):
    SyncConnection.disable_vfs()
    # TODO: remove snooze with proper wait
    # let the client re-sync
    squish.snooze(get_config('minSyncTimeout'))


@When('the user clicks on the activity tab')
def step(context):
    Toolbar.open_activity()


@Then('the table of conflict warnings should include file "|any|"')
def step(context, filename):
    Activity.check_file_exist(filename)


@Then('the file "|any|" should be blacklisted')
def step(context, filename):
    test.compare(
        True, Activity.is_resource_blacklisted(filename), 'File is Blacklisted'
    )


@Then('the file "|any|" should be ignored')
def step(context, filename):
    test.compare(True, Activity.is_resource_ignored(filename), 'File is Ignored')


@Then('the file "|any|" should be excluded')
def step(context, filename):
    test.compare(True, Activity.is_resource_excluded(filename), 'File is Excluded')


@When('the user selects "|any|" tab in the activity')
def step(context, tab_name):
    Activity.click_tab(tab_name)


@Then('the toolbar should have the following tabs:')
def step(context):
    for tab_name in context.table:
        Toolbar.has_item(tab_name[0])


@When('the user selects the following folders to sync:')
def step(context):
    folders = []
    for row in context.table[1:]:
        folders.append(row[0])
    SyncConnectionWizard.select_folders_to_sync(folders)
    SyncConnectionWizard.add_sync_connection()


@When('the user sorts the folder list by "|any|"')
def step(context, header_text):
    if (header_text := header_text.capitalize()) in ['Size', 'Name']:
        SyncConnectionWizard.sort_by(header_text)
    else:
        raise ValueError("Sorting by '" + header_text + "' is not supported.")


@Then('the sync all checkbox should be checked')
def step(context):
    test.compare(
        SyncConnectionWizard.is_root_folder_checked(),
        True,
        'Sync all checkbox is checked',
    )


@Then('the folders should be in the following order:')
def step(context):
    row_index = 0
    for row in context.table[1:]:
        expected_folder = row[0]
        actual_folder = SyncConnectionWizard.get_item_name_from_row(row_index)
        test.compare(actual_folder, expected_folder)

        row_index += 1


@When('the user selects "|any|" space in sync connection wizard')
def step(context, space_name):
    if get_config('ocis'):
        SyncConnectionWizard.select_space(space_name)
        SyncConnectionWizard.next_step()
        set_config('syncConnectionName', space_name)


@When('the user sets the sync path in sync connection wizard')
def step(context):
    SyncConnectionWizard.set_sync_path()


@When(
    'the user sets the temp folder "|any|" as local sync path in sync connection wizard'
)
def step(context, folder_name):
    sync_path = get_temp_resource_path(folder_name)
    SyncConnectionWizard.set_sync_path(sync_path)
    if get_config('ocis'):
        # empty connection name when using temporary locations
        set_config('syncConnectionName', '')
        set_current_user_sync_path(sync_path)


@When('the user selects "|any|" as a remote destination folder')
def step(context, folder_name):
    # There's no remote destination section with oCIS server
    if not get_config('ocis'):
        SyncConnectionWizard.select_remote_destination_folder(folder_name)


@When('the user syncs the "|any|" space')
def step(context, space_name):
    SyncConnectionWizard.sync_space(space_name)


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
    SyncConnectionWizard.add_sync_connection()


@When('user unselects all the remote folders')
def step(context):
    SyncConnectionWizard.deselect_all_remote_folders()


@When('the user |word| VFS support for Windows')
def step(context, action):
    if is_windows():
        action = action.rstrip('s')
        SyncConnectionWizard.enable_disable_vfs_support(action)


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
    Activity.has_sync_status(file_name, status)


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
    account = substitute_inline_codes(account)
    Activity.select_synced_filter(account)


@Then('the following activities should be displayed in synced table')
def step(context):
    for row in context.table[1:]:
        resource = row[0]
        action = row[1]
        account = substitute_inline_codes(row[2])
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
        account = substitute_inline_codes(row[2])
        test.compare(
            Activity.check_not_synced_table(resource, status, account),
            True,
            'Resource should be displayed in the not synced table',
        )


@When('the user unchecks the "|any|" filter')
def step(context, filter_option):
    Activity.select_not_synced_filter(filter_option)
