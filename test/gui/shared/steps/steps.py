# -*- coding: utf-8 -*-
import names
import os
import sys
from os import listdir
from os.path import isfile, join
import re
import urllib.request
import json
import requests

from objectmaphelper import RegularExpression
from pageObjects.AccountConnectionWizard import AccountConnectionWizard
from helpers.SetupClientHelper import *
from helpers.FilesHelper import buildConflictedRegex
from pageObjects.EnterPassword import EnterPassword
from pageObjects.PublicLinkDialog import PublicLinkDialog
from pageObjects.SharingDialog import SharingDialog
from pageObjects.SyncWizard import SyncWizard
from pageObjects.Toolbar import Toolbar
from pageObjects.Activity import Activity
from pageObjects.AccountStatus import AccountStatus

# the script needs to use the system wide python
# to switch from the built-in interpreter see https://kb.froglogic.com/squish/howto/using-external-python-interpreter-squish-6-6/
# if the IDE fails to reference the script, add the folder in Edit->Preferences->PyDev->Interpreters->Libraries
sys.path.append(os.path.realpath('../../../shell_integration/nautilus/'))
import syncstate
import functools


confdir = '/tmp/bdd-tests-owncloud-client/'
confFilePath = confdir + 'owncloud.cfg'
socketConnect = None

stateDataFromMiddleware = None


def getTestStateFromMiddleware(context):
    global stateDataFromMiddleware
    if stateDataFromMiddleware is None:
        res = requests.get(
            os.path.join(context.userData['middlewareUrl'], 'state'),
            headers={"Content-Type": "application/json"},
        )
        try:
            stateDataFromMiddleware = res.json()
        except ValueError:
            raise Exception("Could not get created users information from middleware")

    return stateDataFromMiddleware


@OnScenarioStart
def hook(context):
    try:
        os.makedirs(confdir, 0o0755)
    except:
        pass
    try:
        os.remove(confFilePath)
    except:
        pass


def addAccount(context):
    newAccount = AccountConnectionWizard()
    newAccount.addAccount(context)
    newAccount.selectSyncFolder(context)


@Given('the user has added an account with')
def step(context):
    toolbar = Toolbar()
    toolbar.clickAddAccount()

    addAccount(context)


@When('the user adds the first account with')
def step(context):
    addAccount(context)


@When('the user adds the account with wrong credentials')
def step(context):
    newAccount = AccountConnectionWizard()
    newAccount.addAccount(context)


@Then('an account should be displayed with the displayname |any| and host |any|')
def step(context, displayname, host):
    displayname = substituteInLineCodes(context, displayname)
    host = substituteInLineCodes(context, host)
    test.compare(
        str(
            waitForObjectExists(
                {
                    "name": "settingsdialog_toolbutton_" + displayname + "@" + host,
                    "type": "QToolButton",
                    "visible": 1,
                }
            ).text
        ),
        displayname + "\n" + host,
    )


def getDisplaynameForUser(context, username):
    usersDataFromMiddleware = getTestStateFromMiddleware(context)
    return usersDataFromMiddleware['created_users'][username]['displayname']


def getPasswordForUser(context, username):
    usersDataFromMiddleware = getTestStateFromMiddleware(context)
    return usersDataFromMiddleware['created_users'][username]['password']


@Given('user "|any|" has set up a client with default settings')
def step(context, username):
    password = getPasswordForUser(context, username)
    displayName = getDisplaynameForUser(context, username)
    setUpClient(context, username, displayName, confFilePath)
    enterUserPassword = EnterPassword()
    enterUserPassword.enterPassword(password)


@Given('the user has started the client')
def step(context):
    startClient(context)


@When('the user adds an account with')
def step(context):
    toolbar = Toolbar()
    toolbar.clickAddAccount()

    addAccount(context)


@When('the user adds an account with the following secure server address')
def step(context):
    for row in context.table[0:]:
        row[1] = substituteInLineCodes(context, row[1])
        if row[0] == 'server':
            server = row[1]

    newAccount = AccountConnectionWizard()
    newAccount.addServer(server)


def isItemSynced(type, itemName):
    if type != 'FILE' and type != 'FOLDER':
        raise Exception("type must be 'FILE' or 'FOLDER'")
    socketConnect = syncstate.SocketConnect()
    socketConnect.sendCommand("RETRIEVE_" + type + "_STATUS:" + itemName + "\n")

    if not socketConnect.read_socket_data_with_timeout(0.1):
        return False
    for line in socketConnect.get_available_responses():
        if line.startswith('STATUS:OK') and line.endswith(itemName):
            return True
        elif line.endswith(itemName):
            return False


def isFolderSynced(folderName):
    return isItemSynced('FOLDER', folderName)


def isFileSynced(fileName):
    return isItemSynced('FILE', fileName)


def waitForFileToBeSynced(context, fileName):
    waitFor(
        lambda: isFileSynced(
            sanitizePath(context.userData['clientSyncPathUser1'] + fileName)
        ),
        context.userData['clientSyncTimeout'] * 1000,
    )


def waitForFolderToBeSynced(context, folderName):
    waitFor(
        lambda: isFolderSynced(
            sanitizePath(context.userData['clientSyncPathUser1'] + folderName)
        ),
        context.userData['clientSyncTimeout'] * 1000,
    )


def sanitizePath(path):
    return path.replace('//', '/')


def shareResource(resource):
    socketConnect = syncstate.SocketConnect()
    socketConnect.sendCommand("SHARE:" + resource + "\n")
    if not socketConnect.read_socket_data_with_timeout(0.1):
        return False
    for line in socketConnect.get_available_responses():
        if line.startswith('SHARE:OK') and line.endswith(resource):
            return True
        elif line.endswith(resource):
            return False


def executeStepThroughMiddleware(context, step):
    body = {"step": step}
    if hasattr(context, "table"):
        body["table"] = context.table

    params = json.dumps(body).encode('utf8')

    req = urllib.request.Request(
        context.userData['middlewareUrl'] + 'execute',
        data=params,
        headers={"Content-Type": "application/json"},
        method='POST',
    )
    try:
        urllib.request.urlopen(req)
    except urllib.error.HTTPError as e:
        raise Exception(
            "Step execution through test middleware failed. Error: " + e.read().decode()
        )


@When(
    'the user adds "|any|" as collaborator of resource "|any|" with permissions "|any|" using the client-UI'
)
def step(context, receiver, resource, permissions):
    openSharingDialog(context, resource)
    shareItem = SharingDialog()
    shareItem.addCollaborator(receiver, permissions)


@When(
    'the user adds group "|any|" as collaborator of resource "|any|" with permissions "|any|" using the client-UI'
)
def step(context, receiver, resource, permissions):
    openSharingDialog(context, resource)
    shareItem = SharingDialog()
    shareItem.addCollaborator(receiver, permissions, True)


@Then(
    'user "|any|" should be listed in the collaborators list for file "|any|" with permissions "|any|" on the client-UI'
)
def step(context, receiver, resource, permissions):
    collaboratorShouldBeListed(context, receiver, resource, permissions)


@Then(
    'group "|any|" should be listed in the collaborators list for file "|any|" with permissions "|any|" on the client-UI'
)
def step(context, receiver, resource, permissions):
    receiver += " (group)"
    collaboratorShouldBeListed(context, receiver, resource, permissions)


def collaboratorShouldBeListed(context, receiver, resource, permissions):
    resource = substituteInLineCodes(context, resource)
    socketConnect = syncstate.SocketConnect()
    socketConnect.sendCommand("SHARE:" + resource + "\n")
    permissionsList = permissions.split(',')

    test.compare(
        str(waitForObjectExists(names.scrollArea_sharedWith_QLabel).text), receiver
    )
    test.compare(
        waitForObjectExists(names.scrollArea_permissionsEdit_QCheckBox).checked,
        ('edit' in permissionsList),
    )
    test.compare(
        waitForObjectExists(names.scrollArea_permissionShare_QCheckBox).checked,
        ('share' in permissionsList),
    )


@When('the user waits for the files to sync')
def step(context):
    waitForFolderToBeSynced(context, '/')


@When('the user waits for file "|any|" to be synced')
def step(context, fileName):
    waitForFileToBeSynced(context, fileName)


@Given('the user has waited for file "|any|" to be synced')
def step(context, fileName):
    waitForFileToBeSynced(context, fileName)


@When('the user creates a file "|any|" with the following content on the file system')
def step(context, filename):
    fileContent = "\n".join(context.multiLineText)
    f = open(context.userData['clientSyncPathUser1'] + filename, "w")
    f.write(fileContent)
    f.close()


@Given(r"^(.*) on the server (.*)$", regexp=True)
def step(context, stepPart1, stepPart2):
    executeStepThroughMiddleware(context, "Given " + stepPart1 + " " + stepPart2)
    global usersDataFromMiddleware
    usersDataFromMiddleware = None


@Given(r"^(.*) on the server$", regexp=True)
def step(context, stepPart1):
    executeStepThroughMiddleware(context, "Given " + stepPart1)
    global usersDataFromMiddleware
    usersDataFromMiddleware = None


@Then(r"^(.*) on the server (.*)$", regexp=True)
def step(context, stepPart1, stepPart2):
    executeStepThroughMiddleware(context, "Then " + stepPart1 + " " + stepPart2)


@Then(r"^(.*) on the server$", regexp=True)
def step(context, stepPart1):
    executeStepThroughMiddleware(context, "Then " + stepPart1)


@Then('the file "|any|" should exist on the file system with the following content')
def step(context, filePath):
    expected = "\n".join(context.multiLineText)
    filePath = context.userData['clientSyncPathUser1'] + filePath
    f = open(filePath, 'r')
    contents = f.read()
    test.compare(
        expected,
        contents,
        "file expected to exist with content "
        + expected
        + " but does not have the expected content",
    )


@Given('the user has paused the file sync')
def step(context):
    waitForFolderToBeSynced(context, '/')
    syncWizard = SyncWizard()
    syncWizard.performAction("Pause sync")


@Given('the user has changed the content of local file "|any|" to:')
def step(context, filename):
    fileContent = "\n".join(context.multiLineText)
    f = open(context.userData['clientSyncPathUser1'] + filename, "w")
    f.write(fileContent)
    f.close()


@When('the user resumes the file sync on the client')
def step(context):
    syncWizard = SyncWizard()
    syncWizard.performAction("Resume sync")


@When('the user triggers force sync on the client')
def step(context):
    mouseClick(
        waitForObjectItem(names.stack_folderList_QTreeView, "_1"),
        720,
        36,
        Qt.NoModifier,
        Qt.LeftButton,
    )
    activateItem(waitForObjectItem(names.settings_QMenu, "Force sync now"))


@Then(
    'a conflict file for "|any|" should exist on the file system with the following content'
)
def step(context, filename):
    expected = "\n".join(context.multiLineText)

    namepart = filename.split('.')[0]
    extpart = filename.split('.')[1]
    onlyfiles = [
        f
        for f in listdir(context.userData['clientSyncPathUser1'])
        if isfile(join(context.userData['clientSyncPathUser1'], f))
    ]
    found = False
    pattern = re.compile(buildConflictedRegex(filename))
    for file in onlyfiles:
        if pattern.match(file):
            f = open(context.userData['clientSyncPathUser1'] + file, 'r')
            contents = f.read()
            if contents == expected:
                found = True
                break

    if not found:
        raise Exception("Conflict file not found with given name")


@When('the user clicks on the activity tab')
def step(context):
    toolbar = Toolbar()
    toolbar.clickActivity()


@Then('a conflict warning should be shown for |integer| files')
def step(context, files):
    clickTab(waitForObject(names.stack_QTabWidget), "Not Synced ({})".format(files))
    test.compare(
        waitForObjectExists(
            names.oCC_IssuesWidget_treeWidget_QTreeWidget
        ).topLevelItemCount,
        files,
    )
    test.compare(
        waitForObjectExists(names.oCC_IssuesWidget_treeWidget_QTreeWidget).visible, True
    )
    test.compare(
        waitForObjectExists(
            names.o_treeWidget_Conflict_Server_version_downloaded_local_copy_renamed_and_not_uploaded_QModelIndex
        ).displayText,
        "Conflict: Server version downloaded, local copy renamed and not uploaded.",
    )


@Then('the table of conflict warnings should include file "|any|"')
def step(context, filename):
    activity = Activity()
    activity.checkFileExist(filename)


@When('the user selects "|any|" tab in the activity')
def step(context, tabName):
    activity = Activity()
    activity.clickTab(tabName)


def openSharingDialog(context, resource, itemType='file'):
    resource = sanitizePath(substituteInLineCodes(context, resource))

    if itemType == 'folder':
        waitFor(
            lambda: isFolderSynced(resource),
            context.userData['clientSyncTimeout'] * 1000,
        )
    elif itemType == 'file':
        waitFor(
            lambda: isFileSynced(resource), context.userData['clientSyncTimeout'] * 1000
        )
    else:
        raise Exception("No such item type for resource")

    waitFor(
        lambda: shareResource(resource), context.userData['clientSyncTimeout'] * 1000
    )


@When('the user opens the public links dialog of "|any|" using the client-UI')
def step(context, resource):
    resource = sanitizePath(substituteInLineCodes(context, resource))
    openSharingDialog(context, resource)
    publicLinkDialog = PublicLinkDialog()
    publicLinkDialog.openPublicLinkDialog()


@When("the user toggles the password protection using the client-UI")
def step(context):
    publicLinkDialog = PublicLinkDialog()
    publicLinkDialog.togglesPassword()


@Then('the password progress indicator should not be visible in the client-UI')
def step(context):
    waitFor(lambda: (test.vp("publicLinkPasswordProgressIndicatorInvisible")))


@Then(
    'the password progress indicator should not be visible in the client-UI - expected to fail'
)
def step(context):
    waitFor(lambda: (test.xvp("publicLinkPasswordProgressIndicatorInvisible")))


@When('user "|any|" opens the sharing dialog of "|any|" using the client-UI')
def step(context, receiver, resource):
    openSharingDialog(context, resource, 'folder')


@Then('the error text "|any|" should be displayed in the sharing dialog')
def step(context, fileShareContext):
    shareItem = SharingDialog()
    errorText = shareItem.getSharingDialogMessage()
    test.compare(
        errorText,
        fileShareContext,
    )


def createPublicLinkShare(context, resource, password='', permissions=''):
    resource = sanitizePath(substituteInLineCodes(context, resource))
    openSharingDialog(context, resource)
    publicLinkDialog = PublicLinkDialog()
    publicLinkDialog.openPublicLinkDialog()
    publicLinkDialog.createPublicLink(context, resource, password, permissions)


@When(
    'the user creates a new public link for file "|any|" without password using the client-UI'
)
def step(context, resource):
    createPublicLinkShare(context, resource)


@When(
    'the user creates a new public link for file "|any|" with password "|any|" using the client-UI'
)
def step(context, resource, password):
    createPublicLinkShare(context, resource, password)


@When('the user edits the public link named "|any|" of file "|any|" changing following')
def step(context, publicLinkName, resource):
    publicLinkDialog = PublicLinkDialog()
    publicLinkDialog.setExpirationDate(context, publicLinkName, resource)


@When(
    'the user creates a new public link with permissions "|any|" for folder "|any|" without password using the client-UI'
)
def step(context, permissions, resource):
    createPublicLinkShare(context, resource, '', permissions)


@When(
    'the user creates a new public link with permissions "|any|" for folder "|any|" with password "|any|" using the client-UI'
)
def step(context, permissions, resource, password):
    createPublicLinkShare(context, resource, password, permissions)


def createPublicShareWithRole(context, resource, role):
    resource = sanitizePath(substituteInLineCodes(context, resource))
    openSharingDialog(context, resource)
    publicLinkDialog = PublicLinkDialog()
    publicLinkDialog.openPublicLinkDialog()
    publicLinkDialog.createPublicLinkWithRole(role)


@When(
    'the user creates a new public link for folder "|any|" using the client-UI with these details:'
)
def step(context, resource):
    role = ''
    for row in context.table:
        if row[0] == 'role':
            role = row[1]
            break

    if role == '':
        raise Exception("No role has been found")
    else:
        createPublicShareWithRole(context, resource, role)


@When(
    'the user creates a new public link for folder "|any|" with "|any|" using the client-UI'
)
def step(context, resource, role):
    createPublicShareWithRole(context, resource, role)


@When('the user logs out of the client-UI')
def step(context):
    accountStatus = AccountStatus()
    accountStatus.accountAction("Log out")


def isUserSignedOut(context, username):
    displayname = getDisplaynameForUser(context, username)
    server = context.userData['localBackendUrl']
    accountStatus = AccountStatus()
    test.compare(
        str(waitForObjectExists(accountStatus.SIGNED_OUT_TEXT_BAR).text),
        'Signed out from <a href="'
        + server
        + '">'
        + server
        + '</a> as <i>'
        + displayname
        + '</i>.',
    )


def isUserSignedIn(context, username):
    displayname = getDisplaynameForUser(context, username)
    server = context.userData['localBackendUrl']
    accountStatus = AccountStatus()

    test.compare(
        str(waitForObjectExists(accountStatus.SIGNED_OUT_TEXT_BAR).text),
        'Connected '
        + 'to <a href="'
        + server
        + '">'
        + server
        + '</a> as <i>'
        + displayname
        + '</i>.',
    )


@Then('user "|any|" should be signed out')
def step(context, username):
    isUserSignedOut(context, username)


@Given('user "|any|" has logged out of the client-UI')
def step(context, username):
    waitForFolderToBeSynced(context, '/')
    # TODO: find some way to dynamically to check if files are synced
    # It might take some time for all files to sync
    snooze(5)
    accountStatus = AccountStatus()
    accountStatus.accountAction("Log out")
    isUserSignedOut(context, username)


@When('user "|any|" logs in to the client-UI')
def step(context, username):
    accountStatus = AccountStatus()
    accountStatus.accountAction("Log in")
    password = getPasswordForUser(context, username)
    enterUserPassword = EnterPassword()
    enterUserPassword.enterPassword(password)


@Then('user "|any|" should be connect to the client-UI')
def step(context, username):
    # TODO: find some way to dynamically to check if files are synced
    # It might take some time for all files to sync and connect to ther server
    snooze(5)
    isUserSignedIn(context, username)


@When('the user removes the connection for user "|any|" and host |any|')
def step(context, username, host):
    displayname = getDisplaynameForUser(context, username)
    displayname = substituteInLineCodes(context, displayname)
    host = substituteInLineCodes(context, host)

    clickButton(
        waitForObject(
            {
                "name": "settingsdialog_toolbutton_" + displayname + "@" + host,
                "type": "QToolButton",
                "visible": 1,
            }
        )
    )

    waitForFolderToBeSynced(context, '/')
    accountStatus = AccountStatus()
    accountStatus.removeConnection()


@Then('an account with the displayname |any| and host |any| should not be displayed')
def step(context, displayname, host):
    displayname = substituteInLineCodes(context, displayname)
    host = substituteInLineCodes(context, host)
    toolbar = Toolbar()
    displayedAccountText = toolbar.getDisplayedAccountText(displayname, host)

    test.compare(
        displayedAccountText,
        displayname + "\n" + host,
    )


@Then('connection wizard should be visible')
def step(context):
    test.compare(
        str(waitForObjectExists(names.owncloudWizard_label_2_QLabel).text),
        'Ser&ver Address',
    )
    waitForObject(AccountConnectionWizard.SERVER_ADDRESS_BOX)


@Then("the following tabs in the toolbar should match the default baseline")
def step(context):
    for tabName in context.table:
        test.vp(tabName[0])


@When(
    'the user removes permissions "|any|" for user "|any|" of resource "|any|" using the client-UI'
)
def step(context, permissions, receiver, resource):
    openSharingDialog(context, resource)
    test.compare(
        str(waitForObjectExists(names.scrollArea_sharedWith_QLabel).text), receiver
    )

    shareItem = SharingDialog()
    shareItem.removePermissions(permissions)


@When("the user closes the sharing dialog")
def step(context):
    clickButton(waitForObject(names.sharingDialog_Close_QPushButton))


@Then(
    '"|any|" permissions should not be displayed for user "|any|" for resource "|any|" on the client-UI'
)
def step(context, permissions, user, resource):
    permissionsList = permissions.split(',')

    shareItem = SharingDialog()
    editChecked, shareChecked = shareItem.getAvailablePermission()

    if 'edit' in permissionsList:
        test.compare(editChecked, False)

    if 'share' in permissionsList:
        test.compare(shareChecked, False)


@Then('the error "|any|" should be displayed')
def step(context, errorMessage):
    sharingDialog = SharingDialog()
    test.compare(sharingDialog.getErrorText(), errorMessage)


@When(
    'the user tires to share resource "|any|" with the group "|any|" using the client-UI'
)
def step(context, resource, group):
    openSharingDialog(context, resource)

    sharingDialog = SharingDialog()
    sharingDialog.selectCollaborator(group, True)


@When('the user overwrites the file "|any|" with content "|any|"')
def step(context, resource, content):
    print("starting file overwrite")
    waitForFileToBeSynced(context, resource)
    waitForFolderToBeSynced(context, '/')

    # overwriting the file immediately after it has been synced from the server seems to have some problem.
    # The client does not see the change although the changes have already been made thus we are having a race condition
    # So for now we add 5sec static wait
    # an issue https://github.com/owncloud/client/issues/8832 has been created for it

    snooze(5)

    f = open(context.userData['clientSyncPathUser1'] + resource, "w")
    f.write(content)
    f.close()

    print("file has been overwritten")
    waitForFileToBeSynced(context, resource)


def enableVFSSupport(vfsBtnText):
    # The enabling/disabling VFS button do not have it's own object
    # But it is inside the "stack_folderList_QTreeView" object.
    # So we are clicking at (718, 27) of "stack_folderList_QTreeView" object to enable/disable VFS
    mouseClick(
        waitForObjectItem(names.stack_folderList_QTreeView, "_1"),
        718,
        27,
        Qt.NoModifier,
        Qt.LeftButton,
    )
    activateItem(waitForObjectItem(names.settings_QMenu, vfsBtnText))
    clickButton(
        waitForObject(names.stack_Enable_experimental_placeholder_mode_QPushButton)
    )


@When("the user enables virtual file support")
def step(context):
    enableVFSSupport("Enable virtual file support (experimental)...")


@Then('the "|any|" button should be available')
def step(context, btnText):
    # The enabling/disabling VFS button do not have it's own object
    # But it is inside the "stack_folderList_QTreeView" object.
    # So we are clicking at (718, 27) of "stack_folderList_QTreeView" object to enable/disable VFS
    mouseClick(
        waitForObjectItem(names.stack_folderList_QTreeView, "_1"),
        718,
        27,
        Qt.NoModifier,
        Qt.LeftButton,
    )
    waitForObjectItem(names.settings_QMenu, btnText)


@Given("the user has enabled virtual file support")
def step(context):
    enableVFSSupport("Enable virtual file support (experimental)...")


@When("the user disables virtual file support")
def step(context):
    # The enabling/disabling VFS button do not have it's own object
    # But it is inside the "stack_folderList_QTreeView" object.
    # So we are clicking at (718, 27) of "stack_folderList_QTreeView" object to enable/disable VFS
    mouseClick(
        waitForObjectItem(names.stack_folderList_QTreeView, "_1"),
        733,
        27,
        Qt.NoModifier,
        Qt.LeftButton,
    )
    activateItem(
        waitForObjectItem(names.settings_QMenu, "Disable virtual file support...")
    )
    clickButton(
        waitForObject(names.disable_virtual_file_support_Disable_support_QPushButton)
    )


@When('the user accepts the certificate')
def step(context):
    clickButton(waitForObject(names.oCC_SslErrorDialog_cbTrustConnect_QCheckBox))
    clickButton(waitForObject(names.oCC_SslErrorDialog_OK_QPushButton))


@Then('the lock shown should be closed')
def step(context):
    test.vp("urlLock")


@Then('error "|any|" should be displayed')
def step(context, errorMsg):
    newAccount = AccountConnectionWizard()
    test.compare(str(waitForObjectExists(newAccount.ERROR_LABEL).text), errorMsg)
