from pageObjects.AccountConnectionWizard import AccountConnectionWizard
from pageObjects.SyncConnectionWizard import SyncConnectionWizard
from pageObjects.EnterPassword import EnterPassword
from pageObjects.Toolbar import Toolbar
from pageObjects.AccountSetting import AccountSetting

from helpers.SetupClientHelper import substituteInLineCodes, getClientDetails
from helpers.UserHelper import getDisplaynameForUser, getPasswordForUser
from helpers.SetupClientHelper import setUpClient, startClient
from helpers.SyncHelper import waitForInitialSyncToComplete
from helpers.SetupClientHelper import getResourcePath
from helpers.ConfigHelper import get_config, isWindows, isLinux


@Given(r'the user has added (the first|another) account with', regexp=True)
def step(context, accountType):
    if accountType == 'another':
        Toolbar.openNewAccountSetup()
    account_details = getClientDetails(context)
    AccountConnectionWizard.addAccount(account_details)
    space = ""
    if get_config("ocis"):
        space = "Personal"
    # wait for files to sync
    waitForInitialSyncToComplete(getResourcePath('/', account_details["user"], space))


@When('the user adds the following wrong user credentials:')
def step(context):
    account_details = getClientDetails(context)
    AccountConnectionWizard.addUserCreds(
        account_details['user'], account_details['password']
    )


@Then('the account with displayname "|any|" and host "|any|" should be displayed')
def step(context, displayname, host):
    displayname = substituteInLineCodes(displayname)
    host = substituteInLineCodes(host)

    test.compare(
        Toolbar.getDisplayedAccountText(displayname, host),
        displayname + "\n" + host,
    )


@Then('the account with displayname "|any|" and host "|any|" should not be displayed')
def step(context, displayname, host):
    displayname = substituteInLineCodes(displayname)
    host = substituteInLineCodes(host)
    account_title = displayname + "\n" + host
    timeout = get_config("lowestSyncTimeout") * 1000

    test.compare(
        False,
        Toolbar.hasItem(account_title, timeout),
        f"Expected account '{displayname}' to be removed",
    )


@Given('user "|any|" has set up a client with default settings')
def step(context, username):
    password = getPasswordForUser(username)
    displayName = getDisplaynameForUser(username)
    setUpClient(username, displayName)
    EnterPassword.loginAfterSetup(username, password)

    # wait for files to sync
    waitForInitialSyncToComplete(getResourcePath('/', username))


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
    waitForInitialSyncToComplete(getResourcePath('/', account_details["user"]))


@Given('the user has entered the following account information:')
def step(context):
    account_details = getClientDetails(context)
    AccountConnectionWizard.addAccountInformation(account_details)


@When('the user "|any|" logs out of the client-UI')
def step(context, username):
    AccountSetting.logout()


@Then('user "|any|" should be signed out')
def step(context, username):
    displayname = getDisplaynameForUser(username)
    server = get_config('localBackendUrl')
    test.compare(
        AccountSetting.isUserSignedOut(displayname, server),
        True,
        "User '%s' is signed out" % username,
    )


@Given('user "|any|" has logged out of the client-UI')
def step(context, username):
    AccountSetting.logout()
    displayname = getDisplaynameForUser(username)
    server = get_config('localBackendUrl')
    if not AccountSetting.isUserSignedOut(displayname, server):
        raise Exception("Failed to logout user '%s'" % username)


@When('user "|any|" logs in to the client-UI')
def step(context, username):
    AccountSetting.login()
    password = getPasswordForUser(username)
    EnterPassword.reLogin(username, password)

    # wait for files to sync
    waitForInitialSyncToComplete(getResourcePath('/', username))


@When('user "|any|" logs in to the client-UI with oauth2')
def step(context, username):
    AccountSetting.login()
    password = getPasswordForUser(username)
    EnterPassword.reLogin(username, password, True)

    # wait for files to sync
    waitForInitialSyncToComplete(getResourcePath('/', username))


@When('user "|any|" opens login dialog')
def step(context, username):
    AccountSetting.login()


@When('user "|any|" enters the password "|any|"')
def step(context, username, password):
    EnterPassword.reLogin(username, password)


@Then('user "|any|" should be connect to the client-UI')
def step(context, username):
    displayname = getDisplaynameForUser(username)
    server = get_config('localBackendUrl')
    AccountSetting.waitUntilAccountIsConnected(displayname, server)
    AccountSetting.wait_until_sync_folder_is_configured()


@When('the user removes the connection for user "|any|" and host |any|')
def step(context, username, host):
    displayname = getDisplaynameForUser(username)
    displayname = substituteInLineCodes(displayname)
    host = substituteInLineCodes(host)

    Toolbar.openAccount(displayname, host)
    AccountSetting.removeAccountConnection()


@Then('connection wizard should be visible')
def step(context):
    test.compare(
        AccountConnectionWizard.isNewConnectionWindowVisible(),
        True,
        "Connection window is visible",
    )


@When('the user accepts the certificate')
def step(context):
    AccountConnectionWizard.acceptCertificate()


@Then('the error "|any|" should be displayed in the account connection wizard')
def step(context, errorMsg):
    test.verify(errorMsg in AccountConnectionWizard.getErrorMessage())


@When('the user adds the server "|any|"')
def step(context, server):
    server_url = substituteInLineCodes(server)
    AccountConnectionWizard.addServer(server_url)


@When('the user selects manual sync folder option in advanced section')
def step(context):
    AccountConnectionWizard.selectManualSyncFolderOption()
    AccountConnectionWizard.nextStep()


@Then("credentials wizard should be visible")
def step(context):
    test.compare(
        AccountConnectionWizard.isCredentialWindowVisible(),
        True,
        "Credentials wizard is visible",
    )


@When('the user selects vfs option in advanced section')
def step(context):
    AccountConnectionWizard.selectVFSOption()
    AccountConnectionWizard.nextStep()


@When('the user selects download everything option in advanced section')
def step(context):
    AccountConnectionWizard.selectDownloadEverythingOption()
    AccountConnectionWizard.nextStep()


@When("the user opens the advanced configuration")
def step(context):
    AccountConnectionWizard.selectAdvancedConfig()


@Then("the user should be able to choose the local download directory")
def step(context):
    test.compare(True, AccountConnectionWizard.canChangeLocalSyncDir())


@Then("the download everything option should be selected by default for Linux")
def step(context):
    if isLinux():
        test.compare(
            True,
            AccountConnectionWizard.isSyncEverythingOptionChecked(),
            "Sync everything option is checked",
        )


@Then("the VFS option should be selected by default for Windows")
def step(context):
    if isWindows():
        test.compare(
            True,
            AccountConnectionWizard.isVFSOptionChecked(),
            "VFS option is checked",
        )


@When(r'^the user presses the "([^"]*)" key(?:s)?', regexp=True)
def step(context, key):
    AccountSetting.pressKey(key)


@Then('the log dialog should be opened')
def step(context):
    test.compare(True, AccountSetting.isLogDialogVisible(), "Log dialog is opened")


@When('the user adds the following oauth2 account:')
def step(context):
    account_details = getClientDetails(context)
    account_details.update({'oauth': True})
    AccountConnectionWizard.addAccount(account_details)
    # wait for files to sync
    waitForInitialSyncToComplete(getResourcePath('/', account_details["user"]))


@Step('the user cancels the sync connection wizard')
def step(context):
    SyncConnectionWizard.cancelFolderSyncConnectionWizard()


@Then("the sync folder should not be added")
def step(context):
    test.vp("empty_sync_connection")


@When('user "|any|" logs out from the login required dialog')
def step(context, username):
    AccountSetting.logoutFromLoginRequiredDialog()


@When("the user quits the client")
def step(context):
    Toolbar.quit_owncloud()
