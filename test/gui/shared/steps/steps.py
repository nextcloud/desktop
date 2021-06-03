# -*- coding: utf-8 -*-
import names
import os
import sys
from os import listdir
from os.path import isfile, join
import re
import urllib.request
import json

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


# the script needs to use the system wide python
# to switch from the built-in interpreter see https://kb.froglogic.com/squish/howto/using-external-python-interpreter-squish-6-6/
# if the IDE fails to reference the script, add the folder in Edit->Preferences->PyDev->Interpreters->Libraries
sys.path.append(os.path.realpath('../../../shell_integration/nautilus/'))
import syncstate

confdir = '/tmp/bdd-tests-owncloud-client/'
confFilePath = confdir + 'owncloud.cfg'
socketConnect = None

passwords = {
    'alt1': '1234',
    'alt2': 'AaBb2Cc3Dd4',
    'alt3': 'aVeryLongPassword42TheMeaningOfLife',
}

defaultUsers = {
    'Alice': {
        'displayname': 'Alice Hansen',
        'password': passwords['alt1'],
        'email': 'alice@example.org',
    },
    'Brian': {
        'displayname': 'Brian Murphy',
        'password': passwords['alt2'],
        'email': 'brian@example.org',
    },
    'Carol': {
        'displayname': 'Carol King',
        'password': passwords['alt3'],
        'email': 'carol@example.org',
    },
}


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


@When('the user adds the first account with')
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


def getDisplayname(username):
    if username in defaultUsers.keys():
        return defaultUsers[username]['displayname']


def getPasswordForUser(username):
    if username in defaultUsers.keys():
        return defaultUsers[username]['password']


@Given('user "|any|" has set up a client with default settings')
def step(context, username):
    password = getPasswordForUser(username)
    displayName = getDisplayname(username)
    setUpClient(context, username, displayName, confFilePath)
    enterUserPassword = EnterPassword()
    enterUserPassword.enterPassword(password)


@Given('the user has started the client')
def step(context):
    startClient(context)


@When('the user adds an account with')
def step(context):
    clickButton(
        waitForObject(names.settings_settingsdialog_toolbutton_Add_account_QToolButton)
    )

    newAccount = AccountConnectionWizard()
    newAccount.addAccount(context)


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
        lambda: isFileSynced(context.userData['clientSyncPath'] + fileName),
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


@Then(
    'user "|any|" should be listed in the collaborators list for file "|any|" with permissions "|any|" on the client-UI'
)
def step(context, receiver, resource, permissions):
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
    waitFor(
        lambda: isFolderSynced(context.userData['clientSyncPath']),
        context.userData['clientSyncTimeout'] * 1000,
    )


@When('the user waits for file "|any|" to be synced')
def step(context, fileName):
    waitForFileToBeSynced(context, fileName)


@Given('the user has waited for file "|any|" to be synced')
def step(context, fileName):
    waitForFileToBeSynced(context, fileName)


@When('the user creates a file "|any|" with the following content on the file system')
def step(context, filename):
    fileContent = "\n".join(context.multiLineText)
    f = open(context.userData['clientSyncPath'] + filename, "w")
    f.write(fileContent)
    f.close()


@Given(r"^(.*) on the server (.*)$", regexp=True)
def step(context, stepPart1, stepPart2):
    executeStepThroughMiddleware(context, "Given " + stepPart1 + " " + stepPart2)


@Then(r"^(.*) on the server (.*)$", regexp=True)
def step(context, stepPart1, stepPart2):
    executeStepThroughMiddleware(context, "Then " + stepPart1 + " " + stepPart2)


@Then(r"^(.*) on the server$", regexp=True)
def step(context, stepPart1):
    executeStepThroughMiddleware(context, "Then " + stepPart1)


@Then('the file "|any|" should exist on the file system with the following content')
def step(context, filePath):
    expected = "\n".join(context.multiLineText)
    filePath = context.userData['clientSyncPath'] + filePath
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
    waitFor(
        lambda: isFolderSynced(context.userData['clientSyncPath']),
        context.userData['clientSyncTimeout'] * 1000,
    )
    syncWizard = SyncWizard()
    syncWizard.performAction("Pause sync")


@Given('the user has changed the content of local file "|any|" to:')
def step(context, filename):
    fileContent = "\n".join(context.multiLineText)
    f = open(context.userData['clientSyncPath'] + filename, "w")
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
        for f in listdir(context.userData['clientSyncPath'])
        if isfile(join(context.userData['clientSyncPath'], f))
    ]
    found = False
    pattern = re.compile(buildConflictedRegex(filename))
    for file in onlyfiles:
        if pattern.match(file):
            f = open(context.userData['clientSyncPath'] + file, 'r')
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


@When('the user selects the unsynced files tab')
def step(context):
    activity = Activity()
    activity.clickTab("Not Synced")


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
    errorText = shareItem.getErrorText()
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
