from pageObjects.AccountConnectionWizard import AccountConnectionWizard
from pageObjects.SyncConnectionWizard import SyncConnectionWizard
from pageObjects.EnterPassword import EnterPassword
from pageObjects.Toolbar import Toolbar
from pageObjects.AccountSetting import AccountSetting

from helpers.SetupClientHelper import (
    setUpClient,
    startClient,
    substituteInLineCodes,
    getClientDetails,
    generate_account_config,
    getResourcePath,
)
from helpers.UserHelper import getDisplaynameForUser, getPasswordForUser
from helpers.SyncHelper import waitForInitialSyncToComplete, listenSyncStatusForItem
from helpers.ConfigHelper import get_config, isWindows, isLinux


@When('the user adds the following user credentials:')
def step(context):
    account_details = getClientDetails(context)
    AccountConnectionWizard.addUserCreds(
        account_details['user'], account_details['password']
    )


@Then('the account with displayname "|any|" and host "|any|" should be displayed')
def step(context, displayname, _):
    displayname = substituteInLineCodes(displayname)
    Toolbar.account_exists(displayname)


@Then('the account with displayname "|any|" and host "|any|" should not be displayed')
def step(context, displayname, host):
    displayname = substituteInLineCodes(displayname)
    host = substituteInLineCodes(host)
    account_title = displayname + '\n' + host
    timeout = get_config('lowestSyncTimeout') * 1000

    test.compare(
        False,
        Toolbar.hasItem(account_title, timeout),
        f"Expected account '{displayname}' to be removed",
    )


@Given('user "|any|" has set up a client with default settings')
def step(context, username):
    password = getPasswordForUser(username)
    setUpClient(username)
    enter_password = EnterPassword()
    if get_config('ocis'):
        enter_password.accept_certificate()

    enter_password.loginAfterSetup(username, password)

    # wait for files to sync
    waitForInitialSyncToComplete(getResourcePath('/', username))


@Given('the user has set up the following accounts with default settings:')
def step(context):
    users = []
    for row in context.table:
        users.append(row[0])
    sync_paths = generate_account_config(users)
    startClient()
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
        password = getPasswordForUser(username)
        listenSyncStatusForItem(sync_paths[username])
        enter_password.loginAfterSetup(username, password)
        # wait for files to sync
        waitForInitialSyncToComplete(sync_paths[username])


@Given('the user has started the client')
def step(context):
    startClient()


@When('the user starts the client')
def step(context):
    startClient()


@When('the user opens the add-account dialog')
def step(context):
    Toolbar.openNewAccountSetup()


@When('the user adds the following account:')
def step(context):
    account_details = getClientDetails(context)
    AccountConnectionWizard.addAccount(account_details)
    # wait for files to sync
    waitForInitialSyncToComplete(getResourcePath('/', account_details['user']))


@Given('the user has entered the following account information:')
def step(context):
    account_details = getClientDetails(context)
    AccountConnectionWizard.addAccountInformation(account_details)


@When('the user "|any|" logs out using the client-UI')
def step(context, _):
    AccountSetting.logout()


@Then('user "|any|" should be signed out')
def step(context, username):
    displayname = getDisplaynameForUser(username)
    server = get_config('localBackendUrl')
    test.compare(
        AccountSetting.isUserSignedOut(displayname, server),
        True,
        f'User "{username}" is signed out',
    )


@Given('user "|any|" has logged out from the client-UI')
def step(context, username):
    AccountSetting.logout()
    displayname = getDisplaynameForUser(username)
    server = get_config('localBackendUrl')
    if not AccountSetting.isUserSignedOut(displayname, server):
        raise LookupError(f'Failed to logout user {username}')


@When('user "|any|" logs in using the client-UI')
def step(context, username):
    AccountSetting.login()
    password = getPasswordForUser(username)
    enter_password = EnterPassword()
    enter_password.reLogin(username, password)

    # wait for files to sync
    waitForInitialSyncToComplete(getResourcePath('/', username))


@When('user "|any|" logs in using the client-UI with oauth2')
def step(context, username):
    AccountSetting.login()
    password = getPasswordForUser(username)
    enter_password = EnterPassword()
    enter_password.reLogin(username, password, True)

    # wait for files to sync
    waitForInitialSyncToComplete(getResourcePath('/', username))


@When('user "|any|" opens login dialog')
def step(context, _):
    AccountSetting.login()


@When('user "|any|" enters the password "|any|"')
def step(context, username, password):
    enter_password = EnterPassword()
    enter_password.reLogin(username, password)


@Then('user "|any|" should be connected to the server')
def step(context, username):
    displayname = getDisplaynameForUser(username)
    server = get_config('localBackendUrl')
    AccountSetting.waitUntilAccountIsConnected(displayname, server)
    AccountSetting.wait_until_sync_folder_is_configured()


@When('the user removes the connection for user "|any|" and host |any|')
def step(context, username, _):
    displayname = getDisplaynameForUser(username)
    displayname = substituteInLineCodes(displayname)

    Toolbar.openAccount(displayname)
    AccountSetting.removeAccountConnection()


@Then('connection wizard should be visible')
def step(context):
    test.compare(
        AccountConnectionWizard.isNewConnectionWindowVisible(),
        True,
        'Connection window is visible',
    )


@When('the user accepts the certificate')
def step(context):
    AccountConnectionWizard.acceptCertificate()


@Then('the error "|any|" should be displayed in the account connection wizard')
def step(context, error_message):
    test.verify(error_message in AccountConnectionWizard.getErrorMessage())


@When('the user adds the server "|any|"')
def step(context, server):
    server_url = substituteInLineCodes(server)
    AccountConnectionWizard.addServer(server_url)


@When('the user selects manual sync folder option in advanced section')
def step(context):
    AccountConnectionWizard.selectManualSyncFolderOption()
    AccountConnectionWizard.nextStep()


@Then('credentials wizard should be visible')
def step(context):
    test.compare(
        AccountConnectionWizard.isCredentialWindowVisible(),
        True,
        'Credentials wizard is visible',
    )


@When('the user selects vfs option in advanced section')
def step(context):
    AccountConnectionWizard.selectVFSOption()
    AccountConnectionWizard.nextStep()


@When('the user selects download everything option in advanced section')
def step(context):
    AccountConnectionWizard.selectDownloadEverythingOption()
    AccountConnectionWizard.nextStep()


@When('the user opens the advanced configuration')
def step(context):
    AccountConnectionWizard.selectAdvancedConfig()


@Then('the user should be able to choose the local download directory')
def step(context):
    test.compare(True, AccountConnectionWizard.canChangeLocalSyncDir())


@Then('the download everything option should be selected by default for Linux')
def step(context):
    if isLinux():
        test.compare(
            True,
            AccountConnectionWizard.isSyncEverythingOptionChecked(),
            'Sync everything option is checked',
        )


@Then('the VFS option should be selected by default for Windows')
def step(context):
    if isWindows():
        test.compare(
            True,
            AccountConnectionWizard.isVFSOptionChecked(),
            'VFS option is checked',
        )


@When(r'^the user presses the "([^"]*)" key(?:s)?', regexp=True)
def step(context, key):
    AccountSetting.pressKey(key)


@Then('the log dialog should be opened')
def step(context):
    test.compare(True, AccountSetting.isLogDialogVisible(), 'Log dialog is opened')


@When('the user adds the following oauth2 account:')
def step(context):
    account_details = getClientDetails(context)
    account_details.update({'oauth': True})
    AccountConnectionWizard.addAccount(account_details)
    # wait for files to sync
    waitForInitialSyncToComplete(getResourcePath('/', account_details['user']))


@Step('the user cancels the sync connection wizard')
def step(context):
    SyncConnectionWizard.cancelFolderSyncConnectionWizard()


@When('user "|any|" logs out from the login required dialog')
def step(context, _):
    enter_password = EnterPassword()
    enter_password.logout()


@When('the user quits the client')
def step(context):
    Toolbar.quit_owncloud()


@Then('"|any|" account should be opened')
def step(context, displayname):
    displayname = substituteInLineCodes(displayname)
    if not Toolbar.account_has_focus(displayname):
        raise LookupError(f"Account '{displayname}' should be opened, but it is not")


@Then(
    r'the default local sync path should contain "([^"]*)" in the (configuration|sync connection) wizard',
    regexp=True,
)
def step(context, sync_path, wizard):
    sync_path = substituteInLineCodes(sync_path)

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
