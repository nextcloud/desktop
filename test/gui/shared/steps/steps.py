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
    'alt3': 'aVeryLongPassword42TheMeaningOfLife'
}

defaultUsers = {
    'Alice': {
        'displayname': 'Alice Hansen',
        'password': passwords['alt1'],
        'email': 'alice@example.org'
    },
    'Brian': {
        'displayname': 'Brian Murphy',
        'password': passwords['alt2'],
        'email': 'brian@example.org'
    },
    'Carol': {
        'displayname': 'Carol King',
        'password': passwords['alt3'],
        'email': 'carol@example.org'
    }
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
    addAccount(context)

@Then('an account should be displayed with the displayname |any| and host |any|')
def step(context, displayname, host):
    displayname = substituteInLineCodes(context, displayname)
    host = substituteInLineCodes(context, host)
    test.compare(
        str(
            waitForObjectExists(
                {"name": "settingsdialog_toolbutton_" + displayname + "@" + host, "type": "QToolButton", "visible": 1}
                ).text
            ), displayname + "\n" + host
        )

def startClient():
    startApplication("owncloud -s --logfile - --language en_US --confdir " + confdir)
    snooze(1)

def setUpClient(context, username, password, pollingInterval):
    userSetting = '''
    [Accounts]
    0/Folders/1/ignoreHiddenFiles=true
    0/Folders/1/localPath={client_sync_path}
    0/Folders/1/paused=false
    0/Folders/1/targetPath=/
    0/Folders/1/version=2
    0/Folders/1/virtualFilesMode=off
    0/dav_user={davUserName}
    0/display-name={displayUserName}
    0/http_oauth=false
    0/http_user={davUserName}
    0/url={local_server}
    0/user={displayUserFirstName}
    0/version=1
    version=2
    '''
    userFirstName = username.split()
    userSetting = userSetting + pollingInterval
    args = {'displayUserName': username,
        'davUserName': userFirstName[0].lower(),
        'displayUserFirstName': userFirstName[0],
        'client_sync_path': context.userData['clientSyncPath'],
        'local_server': context.userData['localBackendUrl']
        }
    userSetting = userSetting.format(**args)
    configFile = open(confFilePath, "w")
    configFile.write(userSetting)
    configFile.close()

    startClient()

    try:
        waitForObject(names.enter_Password_Field, 10000)
        type(waitForObject(names.enter_Password_Field), password)
        clickButton(waitForObject(names.enter_Password_OK_QPushButton))
    except LookupError:
        pass

def getPasswordForUser(userId):
    if userId in defaultUsers.keys():
        return defaultUsers[userId]['password']

@Given('user "|any|" has set up a client with default settings and polling interval "|any|"')
def step(context, username, interval):
    pollingInterval='''[ownCloud]
    remotePollInterval={pollingInterval}
    '''
    args = {
        'pollingInterval': interval
    }
    pollingInterval = pollingInterval.format(**args)
    password = getPasswordForUser(username)
    setUpClient(context, username, password, pollingInterval)

@Given('user "|any|" has set up a client with default settings and password "|any|"')
def step(context, username, password):
    setUpClient(context, username, password,'')

@Given('the user has started the client')
def step(context):
    startClient()

@When('the user adds an account with')
def step(context):
    clickButton(waitForObject(names.settings_settingsdialog_toolbutton_Add_account_QToolButton))

    addAccount(context)

def addAccount(context):
    for row in context.table[0:]:
        row[1] = substituteInLineCodes(context, row[1])
        if row[0] == 'server':
            server = row[1]
        elif row[0] == 'user':
            user = row[1]
        elif row[0] == 'password':
            password = row[1]
        elif row[0] == 'localfolder':
            localfolder = row[1]
    try:
        os.makedirs(localfolder, 0o0755)
    except:
        pass

    mouseClick(waitForObject(names.leUrl_OCC_PostfixLineEdit), 0, 0, Qt.NoModifier, Qt.LeftButton)
    type(waitForObject(names.leUrl_OCC_PostfixLineEdit), server)
    clickButton(waitForObject(names.owncloudWizard_qt_passive_wizardbutton1_QPushButton))
    mouseClick(waitForObject(names.leUrl_OCC_PostfixLineEdit), 0, 0, Qt.NoModifier, Qt.LeftButton)
    type(waitForObject(names.leUsername_QLineEdit), user)
    type(waitForObject(names.leUsername_QLineEdit), "<Tab>")
    type(waitForObject(names.lePassword_QLineEdit), password)
    clickButton(waitForObject(names.owncloudWizard_qt_passive_wizardbutton1_QPushButton))
    clickButton(waitForObject(names.pbSelectLocalFolder_QPushButton))
    mouseClick(waitForObject(names.fileNameEdit_QLineEdit), 0, 0, Qt.NoModifier, Qt.LeftButton)
    type(waitForObject(names.fileNameEdit_QLineEdit), localfolder)
    clickButton(waitForObject(names.qFileDialog_Choose_QPushButton))
    clickButton(waitForObject(names.owncloudWizard_qt_passive_wizardbutton1_QPushButton))

def isItemSynced(type, itemName):
    if type != 'FILE' and type != 'FOLDER':
        raise Exception("type must be 'FILE' or 'FOLDER'")
    socketConnect = syncstate.SocketConnect()
    socketConnect.sendCommand("RETRIEVE_" + type + "_STATUS:" + itemName + "\n");

    if not socketConnect.read_socket_data_with_timeout(0.1):
        return False
    for line in socketConnect.get_available_responses():
        if (line.startswith('STATUS:OK') and line.endswith(itemName)):
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
        context.userData['clientSyncTimeout'] * 1000
    )

def substituteInLineCodes(context, value):
    from urllib.parse import urlparse
    value = value.replace('%local_server%', context.userData['localBackendUrl'])
    value = value.replace('%client_sync_path%', context.userData['clientSyncPath'])
    value = value.replace('%local_server_hostname%', urlparse(context.userData['localBackendUrl']).netloc)

    return value

def sanitizePath(path):
    return path.replace('//','/')

def shareResource(resource):
    socketConnect = syncstate.SocketConnect()
    socketConnect.sendCommand("SHARE:" + resource + "\n")
    if not socketConnect.read_socket_data_with_timeout(0.1):
        return False
    for line in socketConnect.get_available_responses():
        if (line.startswith('SHARE:OK') and line.endswith(resource)):
            return True
        elif line.endswith(resource):
            return False

def executeStepThroughMiddleware(context, step):
    body = {
    "step": step}
    if hasattr(context, "table"):
        body["table"] = context.table

    params = json.dumps(body).encode('utf8')

    req = urllib.request.Request(
        context.userData['middlewareUrl'] + 'execute',
        data=params,
        headers={"Content-Type": "application/json"}, method='POST'
    )
    try:
        urllib.request.urlopen(req)
    except urllib.error.HTTPError as e:
        raise Exception(
            "Step execution through test middleware failed. Error: " + e.read().decode()
        )

@When('the user adds "|any|" as collaborator of resource "|any|" with permissions "|any|" using the client-UI')
def step(context, receiver, resource, permissions):
    resource = sanitizePath(substituteInLineCodes(context, resource))
    waitFor(lambda: isFileSynced(resource), context.userData['clientSyncTimeout'] * 1000)
    waitFor(lambda: shareResource(resource), context.userData['clientSyncTimeout'] * 1000)

    mouseClick(waitForObject(names.sharingDialogUG_shareeLineEdit_QLineEdit), 0, 0, Qt.NoModifier, Qt.LeftButton)
    type(waitForObject(names.sharingDialogUG_shareeLineEdit_QLineEdit), receiver)
    mouseClick(waitForObjectItem(names.o_QListView, receiver), 0, 0, Qt.NoModifier, Qt.LeftButton)
    permissionsList = permissions.split(",")

    editChecked = waitForObjectExists(names.scrollArea_permissionsEdit_QCheckBox).checked
    shareChecked = waitForObjectExists(names.scrollArea_permissionShare_QCheckBox).checked
    if ('edit' in permissionsList and editChecked == False) or ('edit' not in permissionsList and editChecked == True):
        clickButton(waitForObject(names.scrollArea_permissionsEdit_QCheckBox))
    if ('share' in permissionsList and shareChecked == False) or ('share' not in permissionsList and shareChecked == True):
        clickButton(waitForObject(names.scrollArea_permissionShare_QCheckBox))

    clickButton(waitForObject(names.sharingDialog_Close_QPushButton))

@Then('user "|any|" should be listed in the collaborators list for file "|any|" with permissions "|any|" on the client-UI')
def step(context, receiver, resource, permissions):
    resource = substituteInLineCodes(context, resource)
    socketConnect = syncstate.SocketConnect()
    socketConnect.sendCommand("SHARE:" + resource + "\n")
    permissionsList = permissions.split(',')

    test.compare(str(waitForObjectExists(names.scrollArea_sharedWith_QLabel).text), receiver)
    test.compare(waitForObjectExists(names.scrollArea_permissionsEdit_QCheckBox).checked, ('edit' in permissionsList))
    test.compare(waitForObjectExists(names.scrollArea_permissionShare_QCheckBox).checked, ('share' in permissionsList))

@When('the user waits for the files to sync')
def step(context):
    waitFor(lambda: isFolderSynced(context.userData['clientSyncPath']), context.userData['clientSyncTimeout'] * 1000)

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
    executeStepThroughMiddleware(
        context,
        "Given " + stepPart1 + " " + stepPart2
    )

@Then(r"^(.*) on the server (.*)$", regexp=True)
def step(context, stepPart1, stepPart2):
    executeStepThroughMiddleware(
        context,
        "Then " + stepPart1 + " " + stepPart2
    )


@Then(r"^(.*) on the server$", regexp=True)
def step(context, stepPart1):
    executeStepThroughMiddleware(
        context,
        "Then " + stepPart1
    )

@Then('the file "|any|" should exist on the file system with the following content')
def step(context, filePath):
    expected = "\n".join(context.multiLineText)
    filePath = context.userData['clientSyncPath'] + filePath
    f = open(filePath, 'r')
    contents = f.read()
    test.compare(expected, contents, "file expected to exist with content " + expected + " but does not have the expected content")


@Given('the user has paused the file sync')
def step(context):
    openContextMenu(waitForObjectItem(names.stack_folderList_QTreeView, "_1"), 0, 0, Qt.NoModifier)
    activateItem(waitForObjectItem(names.settings_QMenu, "Pause sync"))


@Given('the user has changed the content of local file "|any|" to:')
def step(context, filename):
    fileContent = "\n".join(context.multiLineText)
    f = open(context.userData['clientSyncPath'] + filename, "w")
    f.write(fileContent)
    f.close()


@When('the user resumes the file sync on the client')
def step(context):
    openContextMenu(waitForObjectItem(names.stack_folderList_QTreeView, "_1"), 0, 0, Qt.NoModifier)
    activateItem(waitForObjectItem(names.settings_QMenu, "Resume sync"))


@When('the user triggers force sync on the client')
def step(context):
    mouseClick(waitForObjectItem(names.stack_folderList_QTreeView, "_1"), 720, 36, Qt.NoModifier, Qt.LeftButton)
    activateItem(waitForObjectItem(names.settings_QMenu, "Force sync now"))


@Then('a conflict file for "|any|" should exist on the file system with the following content')
def step(context, filename):
    expected = "\n".join(context.multiLineText)

    namepart = filename.split('.')[0]
    extpart = filename.split('.')[1]
    onlyfiles = [f for f in listdir(context.userData['clientSyncPath']) if isfile(join(context.userData['clientSyncPath'], f))]
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
    clickButton(waitForObject(names.settings_settingsdialog_toolbutton_Activity_QToolButton))


@Then('a conflict warning should be shown for |integer| files')
def step(context, files):
    clickTab(waitForObject(names.stack_QTabWidget), "Not Synced ({})".format(files))
    test.compare(
        waitForObjectExists(names.oCC_IssuesWidget_treeWidget_QTreeWidget).topLevelItemCount,
        files
    )
    test.compare(waitForObjectExists(names.oCC_IssuesWidget_treeWidget_QTreeWidget).visible, True)
    test.compare(waitForObjectExists(names.o_treeWidget_Conflict_Server_version_downloaded_local_copy_renamed_and_not_uploaded_QModelIndex).displayText, "Conflict: Server version downloaded, local copy renamed and not uploaded.")


def buildConflictedRegex(filename):
    if '.' in filename:
        # TODO: improve this for complex filenames
        namepart = filename.split('.')[0]
        extpart = filename.split('.')[1]
        return '%s \(conflicted copy \d{4}-\d{2}-\d{2} \d{6}\)\.%s' % (namepart, extpart)
    else:
        return '%s \(conflicted copy \d{4}-\d{2}-\d{2} \d{6}\)' % (filename)


@Then('the table of conflict warnings should include file "|any|"')
def step(context, filename):
    waitForObject(names.settings_OCC_SettingsDialog)
    waitForObjectExists({
            "column": 1,
            "container": names.oCC_IssuesWidget_tableView_QTableView,
            "text": RegularExpression(buildConflictedRegex(filename)),
            "type": "QModelIndex"
    })
    

@When('the user selects the unsynced files tab')
def step(context):
    # TODO: find some way to dynamically select the tab name
    # It might take some time for all files to sync except the expected number of unsynced files
    snooze(10)
    clickTab(waitForObject(names.stack_QTabWidget), "Not Synced")


def openPublicLinkDialog(context, resource):
    resource = sanitizePath(substituteInLineCodes(context, resource))
    waitFor(lambda: isFileSynced(resource), context.userData['clientSyncTimeout'] * 1000)
    waitFor(lambda: shareResource(resource), context.userData['clientSyncTimeout'] * 1000)
    mouseClick(waitForObject(names.qt_tabwidget_tabbar_Public_Links_TabItem), 0, 0, Qt.NoModifier, Qt.LeftButton)

@When('the user opens the public links dialog of "|any|" using the client-UI')
def step(context, resource):
    openPublicLinkDialog(context,resource)


@When("the user toggles the password protection using the client-UI")
def step(context):
    clickButton(waitForObject(names.oCC_ShareLinkWidget_checkBox_password_QCheckBox))


@Then('the progress indicator should not be visible in the client-UI')
def step(context):
    test.compare(
        waitForObjectExists(names.oCC_ShareLinkWidget_checkBox_password_QProgressIndicator).visible,
        False
    )


@When('user "|any|" opens the sharing dialog of "|any|" using the client-UI')
def step(context, receiver, resource):
    resource = sanitizePath(substituteInLineCodes(context, resource))
    waitFor(lambda: isFolderSynced(resource), context.userData['clientSyncTimeout'] * 1000)
    waitFor(lambda: shareResource(resource), context.userData['clientSyncTimeout'] * 1000)


@Then('the error text "|any|" should be displayed in the sharing dialog')
def step(context, fileShareContext):
    test.compare(str(waitForObjectExists(names.sharingDialog_The_file_can_not_be_shared_because_it_was_shared_without_sharing_permission_QLabel).text), fileShareContext)


@When('the user creates a new public link for file "|any|" without password using the client-UI')
def step(context, resource):
    resource = sanitizePath(substituteInLineCodes(context, resource))
    openPublicLinkDialog(context, resource)
    test.compare(str(waitForObjectExists(names.sharingDialog_label_name_QLabel).text), resource.replace(context.userData['clientSyncPath'], ''))
    clickButton(waitForObject(names.oCC_ShareLinkWidget_createShareButton_QPushButton))
    waitFor(lambda: (waitForObject(names.linkShares_0_0_QModelIndex).displayText == "Public link"))


@When('the user creates a new public link for file "|any|" with password "|any|" using the client-UI')
def step(context, resource, password):
    resource = sanitizePath(substituteInLineCodes(context, resource))
    openPublicLinkDialog(context, resource)
    test.compare(str(waitForObjectExists(names.sharingDialog_label_name_QLabel).text), resource.replace(context.userData['clientSyncPath'], ''))
    clickButton(waitForObject(names.oCC_ShareLinkWidget_checkBox_password_QCheckBox))
    mouseClick(waitForObject(names.oCC_ShareLinkWidget_lineEdit_password_QLineEdit), 0, 0, Qt.NoModifier, Qt.LeftButton)
    type(waitForObject(names.oCC_ShareLinkWidget_lineEdit_password_QLineEdit), password)
    clickButton(waitForObject(names.oCC_ShareLinkWidget_createShareButton_QPushButton))
    waitFor(lambda: (findObject(names.linkShares_0_0_QModelIndex).displayText == "Public link"))


@When('the user edits the public link named "|any|" of file "|any|" changing following')
def step(context, publicLinkName, resource):
    test.compare(str(waitForObjectExists(names.sharingDialog_label_name_QLabel).text), resource)
    test.compare(str(waitForObjectExists(names.linkShares_0_0_QModelIndex).text), publicLinkName)
    expDate = []
    for row in context.table:
        if row[0] == 'expireDate':
            expDate = row[1].split("-")
    expYear = expDate[0][2:]
    mouseClick(waitForObject(names.oCC_ShareLinkWidget_qt_spinbox_lineedit_QLineEdit), 0, 0, Qt.NoModifier, Qt.LeftButton)
    nativeType("<Delete>")
    nativeType("<Delete>")
    nativeType(expDate[1])
    nativeType(expDate[2])
    nativeType(expYear)
    nativeType("<Tab>")
    waitFor(lambda: (test.vp("publicLinkExpirationProgressIndicator")))


@When('the user creates a new public link with permissions "|any|" for folder "|any|" without password using the client-UI')
def step(context, permissions, resource):
    resource = sanitizePath(substituteInLineCodes(context, resource))
    openPublicLinkDialog(context, resource)
    radioObjectName = ''
    if permissions == 'Download / View':
        radioObjectName = names.oCC_ShareLinkWidget_radio_readOnly_QRadioButton
    elif permissions == 'Download / View / Edit':
        radioObjectName = names.oCC_ShareLinkWidget_radio_readWrite_QRadioButton
    elif permissions == 'Upload only (File Drop)':
        radioObjectName = names.oCC_ShareLinkWidget_radio_uploadOnly_QRadioButton
    test.compare(str(waitForObjectExists(radioObjectName).text), permissions)

    clickButton(waitForObject(radioObjectName))
    clickButton(waitForObject(names.oCC_ShareLinkWidget_createShareButton_QPushButton))


@When('the user creates a new public link with permissions "|any|" for folder "|any|" with password "|any|" using the client-UI')
def step(context, permissions, resource, password):
    resource = sanitizePath(substituteInLineCodes(context, resource))
    openPublicLinkDialog(context, resource)
    clickButton(waitForObject(names.oCC_ShareLinkWidget_checkBox_password_QCheckBox))
    mouseClick(waitForObject(names.oCC_ShareLinkWidget_lineEdit_password_QLineEdit), 0, 0, Qt.NoModifier, Qt.LeftButton)
    type(waitForObject(names.oCC_ShareLinkWidget_lineEdit_password_QLineEdit), password)
    clickButton(waitForObject(names.oCC_ShareLinkWidget_createShareButton_QPushButton))
    waitFor(lambda: (findObject(names.linkShares_0_0_QModelIndex).displayText == "Public link"))


def createPublicShare(context, resource, role):
    resource = sanitizePath(substituteInLineCodes(context, resource))
    radioObjectName = ''

    if role == 'Viewer':
        radioObjectName = names.oCC_ShareLinkWidget_radio_readOnly_QRadioButton
    elif role == 'Editor':
        radioObjectName = names.oCC_ShareLinkWidget_radio_readWrite_QRadioButton
    elif role == 'Contributor':
        radioObjectName = names.oCC_ShareLinkWidget_radio_uploadOnly_QRadioButton
    else:
        raise Exception("No such role found for resource")

    openPublicLinkDialog(context, resource)
    clickButton(waitForObject(radioObjectName))
    clickButton(waitForObject(names.oCC_ShareLinkWidget_createShareButton_QPushButton))

@When('the user creates a new public link for folder "|any|" using the client-UI with these details:')
def step(context, resource):
    role = ''
    for row in context.table:
        if row[0] == 'role':
            role=row[1]
            break

    if role == '':
        raise Exception("No role has been found")
    else:
        createPublicShare(context, resource, role)


@When('the user creates a new public link for folder "|any|" with "|any|" using the client-UI')
def step(context, resource, role):
    createPublicShare(context, resource, role)


