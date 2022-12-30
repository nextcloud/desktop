from pageObjects.AccountConnectionWizard import AccountConnectionWizard
from pageObjects.EnterPassword import EnterPassword
from pageObjects.Toolbar import Toolbar
from pageObjects.AccountSetting import AccountSetting

from helpers.SetupClientHelper import substituteInLineCodes
from helpers.UserHelper import getDisplaynameForUser, getPasswordForUser
from helpers.SetupClientHelper import setUpClient, startClient
from helpers.SyncHelper import waitForInitialSyncToComplete


@Given(r'the user has added (the first|another) account with', regexp=True)
def step(context, accountType):
    if accountType == 'another':
        Toolbar.openNewAccountSetup()
    AccountConnectionWizard.addAccount(context)


@When('the user adds the following wrong user credentials:')
def step(context):
    AccountConnectionWizard.addUserCreds(context)


@Then('the account with displayname "|any|" and host "|any|" should be displayed')
def step(context, displayname, host):
    displayname = substituteInLineCodes(context, displayname)
    host = substituteInLineCodes(context, host)

    test.compare(
        Toolbar.getDisplayedAccountText(displayname, host),
        displayname + "\n" + host,
    )


@Then('the account with displayname "|any|" and host "|any|" should not be displayed')
def step(context, displayname, host):
    displayname = substituteInLineCodes(context, displayname)
    host = substituteInLineCodes(context, host)

    waitFor(
        lambda: (not object.exists(Toolbar.getItemSelector(displayname + "@" + host))),
    )


@Given('user "|any|" has set up a client with default settings')
def step(context, username):
    password = getPasswordForUser(context, username)
    displayName = getDisplaynameForUser(context, username)
    setUpClient(context, username, displayName, context.userData['clientConfigFile'])

    if context.userData['ocis']:
        AccountConnectionWizard.acceptCertificate()
        EnterPassword.oidcReLogin(username, password)
    else:
        AccountSetting.waitUntilConnectionIsConfigured(
            context.userData['maxSyncTimeout'] * 1000
        )
        EnterPassword.enterPassword(password)

    # wait for files to sync
    waitForInitialSyncToComplete(context)


@Given('the user has started the client')
def step(context):
    startClient(context)


@When(r'^the user adds (the first|another) account with$', regexp=True)
def step(context, accountType):
    if accountType == 'another':
        Toolbar.openNewAccountSetup()
    AccountConnectionWizard.addAccount(context)


@Given('the user has added the following account information:')
def step(context):
    AccountConnectionWizard.addAccountCredential(context)


@When('the user "|any|" logs out of the client-UI')
def step(context, username):
    AccountSetting.logout()


@Then('user "|any|" should be signed out')
def step(context, username):
    displayname = getDisplaynameForUser(context, username)
    server = context.userData['localBackendUrl']
    test.compare(
        AccountSetting.isUserSignedOut(displayname, server),
        True,
        "User '%s' is signed out" % username,
    )


@Given('user "|any|" has logged out of the client-UI')
def step(context, username):
    AccountSetting.logout()
    displayname = getDisplaynameForUser(context, username)
    server = context.userData['localBackendUrl']
    if not AccountSetting.isUserSignedOut(displayname, server):
        raise Exception("Failed to logout user '%s'" % username)


@When('user "|any|" logs in to the client-UI')
def step(context, username):
    AccountSetting.login()
    password = getPasswordForUser(context, username)

    if context.userData['ocis']:
        EnterPassword.oidcReLogin(username, password)
    else:
        EnterPassword.enterPassword(password)

    # wait for files to sync
    waitForInitialSyncToComplete(context)


@Then('user "|any|" should be connect to the client-UI')
def step(context, username):
    displayname = getDisplaynameForUser(context, username)
    server = context.userData['localBackendUrl']
    test.compare(
        AccountSetting.isUserSignedIn(displayname, server),
        True,
        "User '%s' is connected" % username,
    )


@When('the user removes the connection for user "|any|" and host |any|')
def step(context, username, host):
    displayname = getDisplaynameForUser(context, username)
    displayname = substituteInLineCodes(context, displayname)
    host = substituteInLineCodes(context, host)

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


@Then('error "|any|" should be displayed')
def step(context, errorMsg):
    test.compare(AccountConnectionWizard.getErrorMessage(), errorMsg)


@Given('the user has added the following server address:')
def step(context):
    AccountConnectionWizard.addServer(context)
    test.compare(
        AccountConnectionWizard.isCredentialWindowVisible(context),
        True,
        "Assert credentials page is visible",
    )


@When('the user adds the following server address:')
def step(context):
    AccountConnectionWizard.addServer(context)


@When('the user selects manual sync folder option in advanced section')
def step(context):
    AccountConnectionWizard.selectManualSyncFolderOption()
    AccountConnectionWizard.nextStep()


@Then("credentials wizard should be visible")
def step(context):
    test.compare(
        AccountConnectionWizard.isCredentialWindowVisible(context),
        True,
        "Credentials wizard is visible",
    )


@When('the user selects vfs option in advanced section')
def step(context):
    AccountConnectionWizard.selectVFSOption()


@When(r'^the user (confirms|cancels) the enable experimental vfs option$', regexp=True)
def step(context, action):
    if action == "confirms":
        AccountConnectionWizard.confirmEnableExperimentalVFSOption()
    else:
        AccountConnectionWizard.cancelEnableExperimentalVFSOption()
    AccountConnectionWizard.nextStep()
