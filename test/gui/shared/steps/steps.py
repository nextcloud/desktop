# -*- coding: utf-8 -*-
import os
import sys
from os import listdir, rename
from os.path import isfile, join, isdir
import re
import urllib.request
import json
import requests
import builtins
import shutil

from pageObjects.AccountConnectionWizard import AccountConnectionWizard
from pageObjects.SyncConnectionWizard import SyncConnectionWizard
from helpers.SetupClientHelper import *
from helpers.FilesHelper import buildConflictedRegex
from pageObjects.EnterPassword import EnterPassword
from pageObjects.PublicLinkDialog import PublicLinkDialog
from pageObjects.SharingDialog import SharingDialog
from pageObjects.SyncWizard import SyncWizard
from pageObjects.Toolbar import Toolbar
from pageObjects.Activity import Activity
from pageObjects.AccountStatus import AccountStatus

from helpers.SyncHelper import (
    SYNC_STATUS,
    getInitialSyncPatterns,
    getSyncedPattern,
    generateSyncPatternFromMessages,
    filterSyncMessages,
    filterMessagesForItem,
    isEqual,
)

# the script needs to use the system wide python
# to switch from the built-in interpreter see https://kb.froglogic.com/squish/howto/using-external-python-interpreter-squish-6-6/
# if the IDE fails to reference the script, add the folder in Edit->Preferences->PyDev->Interpreters->Libraries
sys.path.append(os.path.realpath('../../../shell_integration/nautilus/'))
from syncstate import SocketConnect


socketConnect = None

createdUsers = {}


def readSocketMessages():
    socket_messages = []
    socketConnect = getSocketConnection()
    socketConnect.read_socket_data_with_timeout(0.1)
    for line in socketConnect.get_available_responses():
        socket_messages.append(line)
    return socket_messages


def readAndUpdateSocketMessages():
    messages = readSocketMessages()
    return updateSocketMessages(messages)


def updateSocketMessages(messages):
    global socket_messages
    socket_messages.extend(filterSyncMessages(messages))
    return socket_messages


def clearSocketMessages(resource=''):
    global socket_messages
    if resource:
        resource_messages = set(filterMessagesForItem(socket_messages, resource))
        socket_messages = [
            msg for msg in socket_messages if msg not in resource_messages
        ]
    else:
        socket_messages.clear()


def listenSyncStatusForItem(item, type='FOLDER'):
    type = type.upper()
    if type != 'FILE' and type != 'FOLDER':
        raise Exception("type must be 'FILE' or 'FOLDER'")
    socketConnect = getSocketConnection()
    socketConnect.sendCommand("RETRIEVE_" + type + "_STATUS:" + item + "\n")


def getCurrentSyncStatus(resource, resourceType):
    listenSyncStatusForItem(resource, resourceType)
    messages = filterMessagesForItem(readSocketMessages(), resource)
    # return the last message from the list
    return messages[-1]


def waitForFileOrFolderToSync(
    context, resource='', resourceType='FOLDER', patterns=None
):
    resource = join(context.userData['currentUserSyncPath'], resource).rstrip('/')
    listenSyncStatusForItem(resource, resourceType)

    timeout = context.userData['maxSyncTimeout'] * 1000

    if patterns is None:
        patterns = getSyncedPattern()

    synced = waitFor(
        lambda: hasSyncPattern(patterns, resource),
        timeout,
    )
    clearSocketMessages(resource)
    if not synced:
        # if the sync pattern doesn't match then check the last sync status
        # and pass the step if the last sync status is STATUS:OK
        status = getCurrentSyncStatus(resource, resourceType)
        if status.startswith(SYNC_STATUS['OK']):
            test.log(
                "[WARN] Failed to match sync pattern for resource: "
                + resource
                + "\nBut its last status is "
                + "'"
                + SYNC_STATUS['OK']
                + "'"
                + ". So passing the step."
            )
            return
        else:
            raise Exception(
                "Timeout while waiting for sync to complete for "
                + str(timeout)
                + " milliseconds"
            )


def waitForInitialSyncToComplete(context):
    waitForFileOrFolderToSync(
        context,
        context.userData['currentUserSyncPath'],
        'FOLDER',
        getInitialSyncPatterns(),
    )


def hasSyncPattern(patterns, resource=None):
    if isinstance(patterns[0], str):
        patterns = [patterns]
    messages = readAndUpdateSocketMessages()
    if resource:
        messages = filterMessagesForItem(messages, resource)
    for pattern in patterns:
        pattern_len = len(pattern)
        for idx, _ in enumerate(messages):
            actual_pattern = generateSyncPatternFromMessages(
                messages[idx : idx + pattern_len]
            )
            if len(actual_pattern) < pattern_len:
                break
            if pattern_len == len(actual_pattern) and isEqual(pattern, actual_pattern):
                return True
    # 100 milliseconds polling interval
    snooze(0.1)
    return False


# gets all users information created in a test scenario
def getCreatedUsersFromMiddleware(context):
    createdUsers = {}
    try:
        res = requests.get(
            os.path.join(context.userData['middlewareUrl'], 'state'),
            headers={"Content-Type": "application/json"},
        )
        createdUsers = res.json()['created_users']
    except ValueError:
        raise Exception("Could not get created users information from middleware")

    return createdUsers


@Given(r'the user has added (the first|another) account with', regexp=True)
def step(context, accountType):
    newAccount = AccountConnectionWizard()
    if accountType == 'another':
        Toolbar.openNewAccountSetup()

    newAccount.addAccount(context)


@When('the user adds the following wrong user credentials:')
def step(context):
    newAccount = AccountConnectionWizard()
    newAccount.addUserCreds(context)


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


def getUserInfo(context, username, attribute):
    # add and update users to the global createdUsers dict if not already there
    # so that we don't have to request for user information in every scenario
    # but instead get user information from the global dict
    global createdUsers
    if username in createdUsers:
        return createdUsers[username][attribute]
    else:
        createdUsers = {**createdUsers, **getCreatedUsersFromMiddleware(context)}
        return createdUsers[username][attribute]


def getDisplaynameForUser(context, username):
    return getUserInfo(context, username, 'displayname')


def getPasswordForUser(context, username):
    return getUserInfo(context, username, 'password')


@Given('user "|any|" has set up a client with default settings')
def step(context, username):
    password = getPasswordForUser(context, username)
    displayName = getDisplaynameForUser(context, username)
    setUpClient(context, username, displayName, context.userData['clientConfigFile'])

    if context.userData['ocis']:
        newAccount = AccountConnectionWizard()
        newAccount.acceptCertificate()
        newAccount.oidcLogin(username, password, True)
    else:
        AccountStatus.waitUntilConnectionIsConfigured(
            context.userData['maxSyncTimeout'] * 1000
        )
        enterUserPassword = EnterPassword()
        enterUserPassword.enterPassword(password)

    # wait for files to sync
    waitForInitialSyncToComplete(context)


@Given('the user has started the client')
def step(context):
    startClient(context)


@When(r'^the user adds (the first|another) account with$', regexp=True)
def step(context, accountType):
    newAccount = AccountConnectionWizard()
    if accountType == 'another':
        Toolbar.openNewAccountSetup()

    newAccount.addAccount(context)


@Given('the user has added the following account information:')
def step(context):
    newAccount = AccountConnectionWizard()
    newAccount.addAccountCredential(context)


def getSocketConnection():
    global socketConnect
    if not socketConnect or not socketConnect.connected:
        socketConnect = SocketConnect()
    return socketConnect


# Using socket API to check file sync status
def hasSyncStatus(itemName, status):
    sync_messages = readAndUpdateSocketMessages()
    sync_messages = filterMessagesForItem(sync_messages, itemName)
    for line in sync_messages:
        if line.startswith(status) and line.rstrip('/').endswith(itemName.rstrip('/')):
            return True
    return False


# useful for checking sync status such as 'error', 'ignore'
# but not quite so reliable for checking 'ok' sync status
def waitForFileOrFolderToHaveSyncStatus(
    context, resource, resourceType, status=SYNC_STATUS['OK'], timeout=None
):
    resource = sanitizePath(join(context.userData['currentUserSyncPath'], resource))

    listenSyncStatusForItem(resource, resourceType)

    if not timeout:
        timeout = context.userData['maxSyncTimeout'] * 1000

    result = waitFor(
        lambda: hasSyncStatus(resource, status),
        timeout,
    )

    if not result:
        if status == SYNC_STATUS['ERROR']:
            expected = "have sync error"
        elif status == SYNC_STATUS['IGNORE']:
            expected = "be sync ignored"
        else:
            expected = "be synced"
        raise Exception(
            "Expected "
            + resourceType
            + " '"
            + resource
            + "' to "
            + expected
            + ", but not."
        )


def waitForFileOrFolderToHaveSyncError(context, resource, resourceType):
    waitForFileOrFolderToHaveSyncStatus(
        context, resource, resourceType, SYNC_STATUS['ERROR']
    )


def folderExists(folderPath, timeout=1000):
    return waitFor(
        lambda: isdir(sanitizePath(folderPath)),
        timeout,
    )


def fileExists(filePath, timeout=1000):
    return waitFor(
        lambda: isfile(sanitizePath(filePath)),
        timeout,
    )


def sanitizePath(path):
    return path.replace('//', '/')


def shareResource(resource):
    socketConnect = getSocketConnection()
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
    SharingDialog.closeSharingDialog()


@When('the user adds following collaborators of resource "|any|" using the client-UI')
def step(context, resource):
    openSharingDialog(context, resource)
    shareItem = SharingDialog()

    # In the following loop we are trying to share resource with given permission to one user at a time given from the data table in the feature file
    for count, row in enumerate(context.table[1:]):
        receiver = row[0]
        permissions = row[1]
        shareItem.addCollaborator(receiver, permissions, False, count + 1)

    SharingDialog.closeSharingDialog()


@When(
    'the user selects "|any|" as collaborator of resource "|any|" using the client-UI'
)
def step(context, receiver, resource):
    openSharingDialog(context, resource)
    shareItem = SharingDialog()
    shareItem.selectCollaborator(receiver)


@When(
    'the user adds group "|any|" as collaborator of resource "|any|" with permissions "|any|" using the client-UI'
)
def step(context, receiver, resource, permissions):
    openSharingDialog(context, resource)
    shareItem = SharingDialog()
    shareItem.addCollaborator(receiver, permissions, True)
    SharingDialog.closeSharingDialog()


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


def collaboratorShouldBeListed(
    context, receiver, resource, permissions, receiverCount=0
):
    resource = getResourcePath(context, resource)
    socketConnect = getSocketConnection()
    socketConnect.sendCommand("SHARE:" + resource + "\n")
    permissionsList = permissions.split(',')

    # findAllObjects: This functionfinds and returns a list of object references identified by the symbolic or real (multi-property) name objectName.
    sharedWithObj = SharingDialog.getCollaborators()

    #     we use sharedWithObj list from above while verifying if users are listed or not.
    #     For this we need an index value i.e receiverCount
    #     For 1st user in the list the index will be 0 which is receiverCount default value
    #     For 2nd user in the list the index will be 1 and so on

    test.compare(str(sharedWithObj[receiverCount].text), receiver)
    test.compare(
        SharingDialog.hasEditPermission(),
        ('edit' in permissionsList),
    )
    test.compare(
        SharingDialog.hasSharePermission(),
        ('share' in permissionsList),
    )
    SharingDialog.closeSharingDialog()


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


@When(
    'user "|any|" creates a file "|any|" with the following content inside the sync folder'
)
def step(context, username, filename):
    fileContent = "\n".join(context.multiLineText)
    syncPath = getUserSyncPath(context, username)
    waitAndWriteFile(context, join(syncPath, filename), fileContent)


@When('user "|any|" creates a folder "|any|" inside the sync folder')
def step(context, username, foldername):
    createFolder(context, foldername, username)


@Given('user "|any|" has created a folder "|any|" inside the sync folder')
def step(context, username, foldername):
    createFolder(context, foldername, username)


# To create folders in a temporary directory, we set isTempFolder True
# And if isTempFolder is True, the createFolder function create folders in tempFolderPath
def createFolder(context, foldername, username=None, isTempFolder=False):
    syncPath = None
    if username and not isTempFolder:
        syncPath = getUserSyncPath(context, username)
    elif isTempFolder:
        syncPath = context.userData['tempFolderPath']
    else:
        syncPath = context.userData['currentUserSyncPath']
    path = join(syncPath, foldername)
    os.makedirs(path)


def renameFileFolder(context, source, destination):
    source = join(context.userData['currentUserSyncPath'], source)
    destination = join(context.userData['currentUserSyncPath'], destination)
    rename(source, destination)


@When('user "|any|" creates a file "|any|" with size "|any|" inside the sync folder')
def step(context, username, filename, filesize):
    createFileWithSize(context, filename, filesize)


def createFileWithSize(context, filename, filesize, isTempFolder=False):
    if isTempFolder:
        path = context.userData['tempFolderPath']
    else:
        path = context.userData['currentUserSyncPath']
    file = join(path, filename)
    cmd = "truncate -s {filesize} {file}".format(filesize=filesize, file=file)
    os.system(cmd)


@When('the user copies the folder "|any|" to "|any|"')
def step(context, sourceFolder, destinationFolder):
    source_dir = join(context.userData['currentUserSyncPath'], sourceFolder)
    destination_dir = join(context.userData['currentUserSyncPath'], destinationFolder)
    shutil.copytree(source_dir, destination_dir)


@When(r'the user renames a (file|folder) "([^"]*)" to "([^"]*)"', regexp=True)
def step(context, type, source, destination):
    renameFileFolder(context, source, destination)


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
    filePath = context.userData['currentUserSyncPath'] + filePath
    f = open(filePath, 'r')
    contents = f.read()
    test.compare(
        expected,
        contents,
        "file expected to exist with content "
        + expected
        + " but does not have the expected content",
    )


@Then(r'^the (file|folder) "([^"]*)" should exist on the file system$', regexp=True)
def step(context, resourceType, resource):
    resourcePath = join(context.userData['currentUserSyncPath'], resource)
    resourceExists = False
    if resourceType == 'file':
        resourceExists = fileExists(
            resourcePath, context.userData['maxSyncTimeout'] * 1000
        )
    elif resourceType == 'folder':
        resourceExists = folderExists(
            resourcePath, context.userData['maxSyncTimeout'] * 1000
        )
    else:
        raise Exception("Unsupported resource type '" + resourceType + "'")

    test.compare(
        True,
        resourceExists,
        "Assert " + resourceType + " '" + resource + "' exists on the system",
    )


@Then(r'^the (file|folder) "([^"]*)" should not exist on the file system$', regexp=True)
def step(context, resourceType, resource):
    resourcePath = join(context.userData['currentUserSyncPath'], resource)
    resourceExists = False
    if resourceType == 'file':
        resourceExists = fileExists(resourcePath, 1000)
    elif resourceType == 'folder':
        resourceExists = folderExists(resourcePath, 1000)
    else:
        raise Exception("Unsupported resource type '" + resourceType + "'")

    test.compare(
        False,
        resourceExists,
        "Assert " + resourceType + " '" + resource + "' doesn't exist on the system",
    )


@Given('the user has paused the file sync')
def step(context):
    SyncWizard.pauseSync(context)


@Given('the user has changed the content of local file "|any|" to:')
def step(context, filename):
    fileContent = "\n".join(context.multiLineText)
    waitAndWriteFile(
        context, join(context.userData['currentUserSyncPath'], filename), fileContent
    )


@When('the user resumes the file sync on the client')
def step(context):
    SyncWizard.resumeSync(context)


@Then(
    'a conflict file for "|any|" should exist on the file system with the following content'
)
def step(context, filename):
    expected = "\n".join(context.multiLineText)

    onlyfiles = [
        f
        for f in listdir(context.userData['currentUserSyncPath'])
        if isfile(join(context.userData['currentUserSyncPath'], f))
    ]
    found = False
    pattern = re.compile(buildConflictedRegex(filename))
    for file in onlyfiles:
        if pattern.match(file):
            f = open(context.userData['currentUserSyncPath'] + file, 'r')
            contents = f.read()
            if contents == expected:
                found = True
                break

    if not found:
        raise Exception("Conflict file not found with given name")


@When('the user clicks on the activity tab')
def step(context):
    Toolbar.openActivity()


@Then('the table of conflict warnings should include file "|any|"')
def step(context, filename):
    activity = Activity()
    activity.checkFileExist(filename)


@Then('the file "|any|" should be blacklisted')
def step(context, filename):
    activity = Activity()
    test.compare(
        True,
        activity.checkBlackListedResourceExist(context, filename),
        "File is blacklisted",
    )


@When('the user selects "|any|" tab in the activity')
def step(context, tabName):
    activity = Activity()
    activity.clickTab(tabName)


def openSharingDialog(context, resource, itemType='file'):
    resource = getResourcePath(context, resource)
    resourceExist = waitFor(
        lambda: os.path.exists(resource), context.userData['maxSyncTimeout'] * 1000
    )
    if not resourceExist:
        raise Exception("{} doesn't exists".format(resource))
    waitFor(lambda: shareResource(resource), context.userData['maxSyncTimeout'] * 1000)


@When('the user opens the public links dialog of "|any|" using the client-UI')
def step(context, resource):
    openSharingDialog(context, resource)
    publicLinkDialog = PublicLinkDialog()
    publicLinkDialog.openPublicLinkDialog()


@When("the user toggles the password protection using the client-UI")
def step(context):
    publicLinkDialog = PublicLinkDialog()
    publicLinkDialog.togglePassword()


@Then('the password progress indicator should not be visible in the client-UI')
def step(context):
    waitFor(lambda: (test.vp("publicLinkPasswordProgressIndicatorInvisible")))


@Then(
    'the password progress indicator should not be visible in the client-UI - expected to fail'
)
def step(context):
    waitFor(lambda: (test.xvp("publicLinkPasswordProgressIndicatorInvisible")))


@When('the user opens the sharing dialog of "|any|" using the client-UI')
def step(context, resource):
    openSharingDialog(context, resource, 'folder')


def getSharingDialogText():
    shareItem = SharingDialog()
    errorText = shareItem.getSharingDialogMessage()
    return errorText


@Then('the text "|any|" should be displayed in the sharing dialog')
def step(context, fileShareContext):
    errorText = getSharingDialogText()
    test.compare(
        errorText,
        fileShareContext,
    )


@Then('the error text "|any|" should be displayed in the sharing dialog')
def step(context, fileShareContext):
    errorText = getSharingDialogText()
    test.compare(
        errorText,
        fileShareContext,
    )


def createPublicLinkShare(
    context, resource, password='', permissions='', expireDate='', name=''
):
    resource = getResourcePath(context, resource)
    openSharingDialog(context, resource)
    publicLinkDialog = PublicLinkDialog()
    publicLinkDialog.openPublicLinkDialog()
    publicLinkDialog.createPublicLink(
        context, resource, password, permissions, expireDate, name
    )


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


@Then('the expiration date of the last public link of file "|any|" should be "|any|"')
def step(context, resource, expiryDate):
    openSharingDialog(context, resource)
    publicLinkDialog = PublicLinkDialog()
    publicLinkDialog.openPublicLinkDialog()

    if expiryDate.strip("%") == "default":
        expiryDate = PublicLinkDialog.getDefaultExpiryDate()

    publicLinkDialog.verifyExpirationDate(expiryDate)

    SharingDialog.closeSharingDialog()


def setExpirationDateWithVerification(resource, publicLinkName, expireDate):
    publicLinkDialog = PublicLinkDialog()
    publicLinkDialog.verifyResource(resource)
    publicLinkDialog.verifyPublicLinkName(publicLinkName)
    publicLinkDialog.setExpirationDate(expireDate)


@When('the user edits the public link named "|any|" of file "|any|" changing following')
def step(context, publicLinkName, resource):
    expireDate = ''
    for row in context.table:
        if row[0] == 'expireDate':
            expireDate = row[1]
            break
    setExpirationDateWithVerification(resource, publicLinkName, expireDate)


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


@When('the user creates a new public link with following settings using the client-UI:')
def step(context):
    linkSettings = {}
    for row in context.table:
        linkSettings[row[0]] = row[1]

    if "path" not in linkSettings:
        raise Exception("'path' is required but not given.")

    if "expireDate" in linkSettings and linkSettings['expireDate'] == "%default%":
        linkSettings['expireDate'] = linkSettings['expireDate'].strip("%")

    createPublicLinkShare(
        context,
        resource=linkSettings['path'],
        password=linkSettings['password'] if "password" in linkSettings else None,
        expireDate=linkSettings['expireDate'] if "expireDate" in linkSettings else None,
    )


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


@When('the user "|any|" logs out of the client-UI')
def step(context, username):
    AccountStatus.logout()


@Then('user "|any|" should be signed out')
def step(context, username):
    displayname = getDisplaynameForUser(context, username)
    server = context.userData['localBackendUrl']
    test.compare(
        AccountStatus.isUserSignedOut(displayname, server),
        True,
        "User '%s' is signed out" % username,
    )


@Given('user "|any|" has logged out of the client-UI')
def step(context, username):
    AccountStatus.logout()
    displayname = getDisplaynameForUser(context, username)
    server = context.userData['localBackendUrl']
    if not AccountStatus.isUserSignedOut(displayname, server):
        raise Exception("Failed to logout user '%s'" % username)


@When('user "|any|" logs in to the client-UI')
def step(context, username):
    AccountStatus.login()
    password = getPasswordForUser(context, username)

    if context.userData['ocis']:
        account = AccountConnectionWizard()
        account.oidcLogin(username, password, True)
    else:
        enterUserPassword = EnterPassword()
        enterUserPassword.enterPassword(password)

    # wait for files to sync
    waitForInitialSyncToComplete(context)


@Then('user "|any|" should be connect to the client-UI')
def step(context, username):
    displayname = getDisplaynameForUser(context, username)
    server = context.userData['localBackendUrl']
    test.compare(
        AccountStatus.isUserSignedIn(displayname, server),
        True,
        "User '%s' is connected" % username,
    )


@When('the user removes the connection for user "|any|" and host |any|')
def step(context, username, host):
    displayname = getDisplaynameForUser(context, username)
    displayname = substituteInLineCodes(context, displayname)
    host = substituteInLineCodes(context, host)

    AccountStatus.removeAccountConnection()


@Then('connection wizard should be visible')
def step(context):
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
    shareItem = SharingDialog()
    shareItem.removePermissions(permissions)


@When("the user closes the sharing dialog")
def step(context):
    SharingDialog.closeSharingDialog()


@Then(
    '"|any|" permissions should not be displayed for user "|any|" for resource "|any|" on the client-UI'
)
def step(context, permissions, user, resource):
    permissionsList = permissions.split(',')

    shareItem = SharingDialog()
    shareItem.verifyResource(resource)
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


# performing actions immediately after completing the sync from the server does not work
# The test should wait for a while before performing the action
# issue: https://github.com/owncloud/client/issues/8832
def waitForClientToBeReady(context):
    global waitedAfterSync
    if not waitedAfterSync:
        snooze(context.userData['minSyncTimeout'])
        waitedAfterSync = True


def writeFile(resource, content):
    f = open(resource, "w")
    f.write(content)
    f.close()


def waitAndWriteFile(context, path, content):
    waitForClientToBeReady(context)
    writeFile(path, content)


def waitAndTryToWriteFile(context, resource, content):
    waitForClientToBeReady(context)
    try:
        writeFile(resource, content)
    except:
        pass


@When('the user overwrites the file "|any|" with content "|any|"')
def step(context, resource, content):
    print("starting file overwrite")
    resource = join(context.userData['currentUserSyncPath'], resource)
    waitAndWriteFile(context, resource, content)
    print("file has been overwritten")


@When('the user tries to overwrite the file "|any|" with content "|any|"')
def step(context, resource, content):
    resource = context.userData['currentUserSyncPath'] + resource
    waitAndTryToWriteFile(context, resource, content)


@When('user "|any|" tries to overwrite the file "|any|" with content "|any|"')
def step(context, user, resource, content):
    resource = getResourcePath(context, resource, user)
    waitAndTryToWriteFile(context, resource, content)


@When("the user enables virtual file support")
def step(context):
    SyncWizard.enableVFS(context)


@Then('the "|any|" button should be available')
def step(context, item):
    SyncWizard.openMenu(context)
    SyncWizard.hasMenuItem(item)


@Given("the user has enabled virtual file support")
def step(context):
    SyncWizard.enableVFS(context)


@When("the user disables virtual file support")
def step(context):
    SyncWizard.disableVFS(context)


@When('the user accepts the certificate')
def step(context):
    newAccount = AccountConnectionWizard()
    newAccount.acceptCertificate()


@Then('the lock shown should be closed')
def step(context):
    test.vp("urlLock")


@Then('error "|any|" should be displayed')
def step(context, errorMsg):
    newAccount = AccountConnectionWizard()
    test.compare(str(waitForObjectExists(newAccount.ERROR_LABEL).text), errorMsg)


@When(r'the user deletes the (file|folder) "([^"]*)"', regexp=True)
def step(context, itemType, resource):
    waitForClientToBeReady(context)

    resourcePath = sanitizePath(context.userData['currentUserSyncPath'] + resource)
    if itemType == 'file':
        os.remove(resourcePath)
    elif itemType == 'folder':
        shutil.rmtree(resourcePath)
    else:
        raise Exception("No such item type for resource")

    isSyncFolderEmpty = True
    for item in listdir(context.userData['currentUserSyncPath']):
        # do not count the hidden files as they are ignored by the client
        if not item.startswith("."):
            isSyncFolderEmpty = False
            break

    # if the sync folder is empty after deleting file,
    # a dialog will popup asking to confirm "Remove all files"
    if isSyncFolderEmpty:
        try:
            AccountStatus.confirmRemoveAllFiles()
        except:
            pass


@When(
    'the user unshares the resource "|any|" for collaborator "|any|" using the client-UI'
)
def step(context, resource, receiver):
    openSharingDialog(context, resource)
    SharingDialog.unshareWith(receiver)


@Given('the user has added the following server address:')
def step(context):
    newAccount = AccountConnectionWizard()
    newAccount.addServer(context)
    test.compare(
        waitForObjectExists(newAccount.BASIC_CREDENTIAL_PAGE).visible,
        True,
        "Assert credentials page is visible",
    )


@When('the user adds the following server address:')
def step(context):
    newAccount = AccountConnectionWizard()
    newAccount.addServer(context)


@When('the user selects the following folders to sync:')
def step(context):
    syncConnection = SyncConnectionWizard()
    syncConnection.selectFoldersToSync(context)
    syncConnection.addSyncConnection()


@When('the user selects manual sync folder option in advanced section')
def step(context):
    newAccount = AccountConnectionWizard()
    newAccount.selectManualSyncFolderOption()
    newAccount.nextStep()


@When('the user sorts the folder list by "|any|"')
def step(context, headerText):
    headerText = headerText.capitalize()
    if headerText in ["Size", "Name"]:
        syncConnection = SyncConnectionWizard()
        syncConnection.sortBy(headerText)
    else:
        raise Exception("Sorting by '" + headerText + "' is not supported.")


@Then('the sync all checkbox should be checked')
def step(context):
    syncConnection = SyncConnectionWizard()
    state = waitForObject(syncConnection.SYNC_DIALOG_ROOT_FOLDER)["checkState"]
    test.compare("checked", state, "Sync all checkbox is checked")


@Then("the folders should be in the following order:")
def step(context):
    syncConnection = SyncConnectionWizard()
    rowIndex = 0
    for row in context.table[1:]:
        FOLDER_TREE_ROW = {
            "row": rowIndex,
            "container": syncConnection.SYNC_DIALOG_ROOT_FOLDER,
            "type": "QModelIndex",
        }
        expectedFolder = row[0]
        actualFolder = waitForObjectExists(FOLDER_TREE_ROW).displayText
        test.compare(actualFolder, expectedFolder)

        rowIndex += 1


@When('the user deletes the public link for file "|any|"')
def step(context, resource):
    openSharingDialog(context, resource)
    publicLinkDialog = PublicLinkDialog()
    publicLinkDialog.openPublicLinkDialog()
    PublicLinkDialog.deletePublicLink()


@When(
    'the user changes the password of public link "|any|" to "|any|" using the client-UI'
)
def step(context, publicLinkName, password):
    publicLinkDialog = PublicLinkDialog()
    publicLinkDialog.verifyPublicLinkName(publicLinkName)
    publicLinkDialog.changePassword(password)


@Then(
    'the following users should be listed in as collaborators for file "|any|" on the client-UI'
)
def step(context, resource):
    #     Here we are trying to verify if the user added in when step are listed in the client-UI or not
    #     We now have a variable name receiverCount which is used in collaboratorShouldBeListed function call
    receiverCount = 0
    for row in context.table[1:]:
        receiver = row[0]
        permissions = row[1]

        collaboratorShouldBeListed(
            context, receiver, resource, permissions, receiverCount
        )
        receiverCount += 1


def searchCollaborator(collaborator):
    shareItem = SharingDialog()
    shareItem.searchCollaborator(collaborator)


@When('the user searches for collaborator "|any|" using the client-UI')
def step(context, collaborator):
    searchCollaborator(collaborator)


@When(
    'the user searches for collaborator with autocomplete characters "|any|" using the client-UI'
)
def step(context, collaborator):
    searchCollaborator(collaborator)


@Then('the following users should be listed as suggested collaborators:')
def step(context):
    shareItem = SharingDialog()
    for collaborator in context.table[1:]:
        exists = False
        try:
            waitForObjectItem(shareItem.SUGGESTED_COLLABORATOR, collaborator[0])
            exists = True
        except LookupError as e:
            pass

        test.compare(exists, True, "Assert user '" + collaborator[0] + "' is listed")


@Then('the collaborators should be listed in the following order:')
def step(context):
    for index, collaborator in enumerate(context.table[1:], start=1):
        test.compare(
            SharingDialog.getCollaboratorName(index),
            collaborator[0],
        )


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


@When('user "|any|" creates the following files inside the sync folder:')
def step(context, username):
    syncPath = getUserSyncPath(context, username)

    waitForClientToBeReady(context)

    for row in context.table[1:]:
        filename = syncPath + row[0]
        writeFile(join(syncPath, filename), '')


@Given(
    'the user has created a folder "|any|" with "|any|" files each of size "|any|" bytes in temp folder'
)
def step(context, foldername, filenumber, filesize):
    createFolder(context, foldername, isTempFolder=True)
    filesize = builtins.int(filesize)
    for i in range(0, builtins.int(filenumber)):
        filename = f"file{i}.txt"
        createFileWithSize(context, join(foldername, filename), filesize, True)


@When('user "|any|" moves folder "|any|" from the temp folder into the sync folder')
def step(context, username, foldername):
    source_dir = join(context.userData['tempFolderPath'], foldername)
    destination_dir = getUserSyncPath(context, username)
    shutil.move(source_dir, destination_dir)


@Then("credentials wizard should be visible")
def step(context):
    if context.userData['ocis']:
        waitForObject(AccountConnectionWizard.OAUTH_CREDENTIAL_PAGE)
    else:
        waitForObject(AccountConnectionWizard.BASIC_CREDENTIAL_PAGE)


@When('the user sets the sync path in sync connection wizard')
def step(context):
    syncConnection = SyncConnectionWizard()
    syncConnection.setSyncPathInSyncConnectionWizard(context)


@When('the user selects "|any|" as a remote destination folder')
def step(context, folderName):
    syncConnection = SyncConnectionWizard()
    syncConnection.selectRemoteDestinationFolder(folderName)


@When('the user selects vfs option in advanced section')
def step(context):
    newAccount = AccountConnectionWizard()
    newAccount.selectVFSOption()


@When(r'^the user (confirms|cancels) the enable experimental vfs option$', regexp=True)
def step(context, action):
    newAccount = AccountConnectionWizard()
    if action == "confirms":
        newAccount.confirmEnableExperimentalVFSOption()
    else:
        newAccount.cancelEnableExperimentalVFSOption()
    newAccount.nextStep()
