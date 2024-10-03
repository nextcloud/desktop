from pageObjects.AccountConnectionWizard import AccountConnectionWizard
from pageObjects.SyncConnectionWizard import SyncConnectionWizard
from pageObjects.EnterPassword import EnterPassword
from pageObjects.Toolbar import Toolbar
from pageObjects.AccountSetting import AccountSetting

from helpers.SetupClientHelper import (
    setup_client,
    start_client,
    substitute_inline_codes,
    get_client_details,
    generate_account_config,
    get_resource_path,
)
from helpers.UserHelper import get_displayname_for_user, get_password_for_user
from helpers.SyncHelper import (
    wait_for_initial_sync_to_complete,
    listen_sync_status_for_item,
)
from helpers.ConfigHelper import get_config, is_windows, is_linux


@When('the user adds the following user credentials:')
def step(context):
    account_details = get_client_details(context)
    AccountConnectionWizard.add_user_credentials(
        account_details['user'], account_details['password']
    )


@Then('the account with displayname "|any|" and host "|any|" should be displayed')
def step(context, displayname, _):
    displayname = substitute_inline_codes(displayname)
    Toolbar.account_exists(displayname)


@Then('the account with displayname "|any|" and host "|any|" should not be displayed')
def step(context, displayname, host):
    displayname = substitute_inline_codes(displayname)
    host = substitute_inline_codes(host)
    account_title = displayname + '\n' + host
    timeout = get_config('lowestSyncTimeout') * 1000

    test.compare(
        False,
        Toolbar.has_item(account_title, timeout),
        f"Expected account '{displayname}' to be removed",
    )


@Given('user "|any|" has set up a client with default settings')
def step(context, username):
    password = get_password_for_user(username)
    setup_client(username)
    enter_password = EnterPassword()
    if get_config('ocis'):
        enter_password.accept_certificate()

    enter_password.login_after_setup(username, password)

    # wait for files to sync
    wait_for_initial_sync_to_complete(get_resource_path('/', username))


@Given('the user has set up the following accounts with default settings:')
def step(context):
    users = []
    for row in context.table:
        users.append(row[0])
    sync_paths = generate_account_config(users)
    start_client()
    if get_config('ocis'):
        # accept certificate for each user
        for idx, _ in enumerate(users):
            enter_password = EnterPassword(len(users) - idx)
            enter_password.accept_certificate()

    for idx, _ in enumerate(sync_paths.values()):
        # login from last dialog
        account_idx = len(sync_paths) - idx
        enter_password = EnterPassword(account_idx)
        username = enter_password.get_username()
        password = get_password_for_user(username)
        listen_sync_status_for_item(sync_paths[username])
        enter_password.login_after_setup(username, password)
        # wait for files to sync
        wait_for_initial_sync_to_complete(sync_paths[username])


@Given('the user has started the client')
def step(context):
    start_client()


@When('the user starts the client')
def step(context):
    start_client()


@When('the user opens the add-account dialog')
def step(context):
    Toolbar.open_new_account_setup()


@When('the user adds the following account:')
def step(context):
    account_details = get_client_details(context)
    AccountConnectionWizard.add_account(account_details)
    # wait for files to sync
    wait_for_initial_sync_to_complete(get_resource_path('/', account_details['user']))


@Given('the user has entered the following account information:')
def step(context):
    account_details = get_client_details(context)
    AccountConnectionWizard.add_account_information(account_details)


@When('the user "|any|" logs out using the client-UI')
def step(context, _):
    AccountSetting.logout()


@Then('user "|any|" should be signed out')
def step(context, username):
    test.compare(
        AccountSetting.is_user_signed_out(),
        True,
        f'User "{username}" is signed out',
    )


@Given('user "|any|" has logged out from the client-UI')
def step(context, username):
    AccountSetting.logout()
    if not AccountSetting.is_user_signed_out():
        raise LookupError(f'Failed to logout user {username}')


@When('user "|any|" logs in using the client-UI')
def step(context, username):
    AccountSetting.login()
    password = get_password_for_user(username)
    enter_password = EnterPassword()
    enter_password.relogin(username, password)

    # wait for files to sync
    wait_for_initial_sync_to_complete(get_resource_path('/', username))


@When('user "|any|" logs in using the client-UI with oauth2')
def step(context, username):
    AccountSetting.login()
    password = get_password_for_user(username)
    enter_password = EnterPassword()
    enter_password.relogin(username, password, True)

    # wait for files to sync
    wait_for_initial_sync_to_complete(get_resource_path('/', username))


@When('user "|any|" opens login dialog')
def step(context, _):
    AccountSetting.login()


@When('user "|any|" enters the password "|any|"')
def step(context, username, password):
    enter_password = EnterPassword()
    enter_password.relogin(username, password)


@Then('user "|any|" should be connected to the server')
def step(context, _):
    AccountSetting.wait_until_account_is_connected()
    AccountSetting.wait_until_sync_folder_is_configured()


@When('the user removes the connection for user "|any|" and host |any|')
def step(context, username, _):
    displayname = get_displayname_for_user(username)
    displayname = substitute_inline_codes(displayname)

    Toolbar.open_account(displayname)
    AccountSetting.remove_account_connection()


@Then('connection wizard should be visible')
def step(context):
    test.compare(
        AccountConnectionWizard.is_new_connection_window_visible(),
        True,
        'Connection window is visible',
    )


@When('the user accepts the certificate')
def step(context):
    AccountConnectionWizard.accept_certificate()


@Then('the error "|any|" should be displayed in the account connection wizard')
def step(context, error_message):
    test.verify(error_message in AccountConnectionWizard.get_error_message())


@When('the user adds the server "|any|"')
def step(context, server):
    server_url = substitute_inline_codes(server)
    AccountConnectionWizard.add_server(server_url)


@When('the user selects manual sync folder option in advanced section')
def step(context):
    AccountConnectionWizard.select_manual_sync_folder_option()
    AccountConnectionWizard.next_step()


@Then('credentials wizard should be visible')
def step(context):
    test.compare(
        AccountConnectionWizard.is_credential_window_visible(),
        True,
        'Credentials wizard is visible',
    )


@When('the user selects vfs option in advanced section')
def step(context):
    AccountConnectionWizard.select_vfs_option()
    AccountConnectionWizard.next_step()


@When('the user selects download everything option in advanced section')
def step(context):
    AccountConnectionWizard.select_download_everything_option()
    AccountConnectionWizard.next_step()


@When('the user opens the advanced configuration')
def step(context):
    AccountConnectionWizard.select_advanced_config()


@Then('the user should be able to choose the local download directory')
def step(context):
    test.compare(True, AccountConnectionWizard.can_change_local_sync_dir())


@Then('the download everything option should be selected by default for Linux')
def step(context):
    if is_linux():
        test.compare(
            True,
            AccountConnectionWizard.is_sync_everything_option_checked(),
            'Sync everything option is checked',
        )


@Then('the VFS option should be selected by default for Windows')
def step(context):
    if is_windows():
        test.compare(
            True,
            AccountConnectionWizard.is_vfs_option_checked(),
            'VFS option is checked',
        )


@When(r'^the user presses the "([^"]*)" key(?:s)?', regexp=True)
def step(context, key):
    AccountSetting.press_key(key)


@Then('the log dialog should be opened')
def step(context):
    test.compare(True, AccountSetting.is_log_dialog_visible(), 'Log dialog is opened')


@When('the user adds the following oauth2 account:')
def step(context):
    account_details = get_client_details(context)
    account_details.update({'oauth': True})
    AccountConnectionWizard.add_account(account_details)
    # wait for files to sync
    wait_for_initial_sync_to_complete(get_resource_path('/', account_details['user']))


@Step('the user cancels the sync connection wizard')
def step(context):
    SyncConnectionWizard.cancel_folder_sync_connection_wizard()


@When('user "|any|" logs out from the login required dialog')
def step(context, _):
    enter_password = EnterPassword()
    enter_password.logout()


@When('the user quits the client')
def step(context):
    Toolbar.quit_owncloud()


@Then('"|any|" account should be opened')
def step(context, displayname):
    displayname = substitute_inline_codes(displayname)
    if not Toolbar.account_has_focus(displayname):
        raise LookupError(f"Account '{displayname}' should be opened, but it is not")


@Then(
    r'the default local sync path should contain "([^"]*)" in the (configuration|sync connection) wizard',
    regexp=True,
)
def step(context, sync_path, wizard):
    sync_path = substitute_inline_codes(sync_path)

    actual_sync_path = ''
    if wizard == 'configuration':
        actual_sync_path = AccountConnectionWizard.get_local_sync_path()
    else:
        actual_sync_path = SyncConnectionWizard.get_local_sync_path()

    test.compare(
        actual_sync_path,
        sync_path,
        'Compare sync path contains the expected path',
    )


@Then('the warning "|any|" should appear in the sync connection wizard')
def step(context, warn_message):
    actual_message = SyncConnectionWizard.get_warn_label()
    test.compare(
        True,
        warn_message in actual_message,
        'Contains warning message',
    )
