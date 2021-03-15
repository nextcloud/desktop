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
# to switch from the build-in interpreter see https://kb.froglogic.com/squish/howto/using-external-python-interpreter-squish-6-6/
# if the IDE fails to reference the script, add the folder in Edit->Preferences->PyDev->Interpreters->Libraries
sys.path.append(os.path.realpath('../../../shell_integration/nautilus/'))
import syncstate

confdir = '/tmp/bdd-tests-owncloud-client/'
confFilePath = confdir + 'owncloud.cfg'
socketConnect = None


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

@Given('user "|any|" has set up a client with these settings and password |any|:')
def step(context, username, password):
    configContent = "\n".join(context.multiLineText)
    configContent = substituteInLineCodes(context, configContent)
    configFile = open(confFilePath, "w")
    configFile.write(configContent)
    configFile.close()

    startApplication("owncloud -s --logfile - --confdir " + confdir)
    snooze(1)
    try:
        waitForObject(names.enter_Password_Field, 10000)
        type(waitForObject(names.enter_Password_Field), password)
        clickButton(waitForObject(names.enter_Password_OK_QPushButton))
    except LookupError:
        pass

@Given('the user has started the client')
def step(context):
    startApplication("owncloud -s --logfile - --confdir " + confdir)
    snooze(1)

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

def executeStepThroughMiddleware(context, step, errorMessage=''):
    body = {
    "step": step}
    params = json.dumps(body).encode('utf8')
        
    req = urllib.request.Request(
        context.userData['middlewareUrl'] + 'execute',
        data=params,
        headers={"Content-Type": "application/json"}, method='POST'
    )
    res = urllib.request.urlopen(req)
    if res.getcode() != 200:
        raise Exception(errorMessage + '\nexpected status 200 found {}'.format(res.getcode()))
    
    
@When('the user adds "|any|" as collaborator of resource "|any|" with permissions "|any|" using the client-UI')
def step(context, receiver, resource, permissions):
    resource = substituteInLineCodes(context, resource).replace('//','/')
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

@When('the user waits for file "|any|" to get synced')
def step(context, fileName):
    waitForFileToBeSynced(context, fileName)

@Given('the user has waited for file "|any|" to get synced')
def step(context, fileName):
    waitForFileToBeSynced(context, fileName)

@When('the user creates a file "|any|" with following content on the file system')
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


@Then('the file "|any|" should exist on the file system with following content')
def step(context, filePath):
    expected = "\n".join(context.multiLineText)
    filePath = context.userData['clientSyncPath'] + filePath
    f = open(filePath, 'r')
    contents = f.read()
    test.compare(expected, contents, "file expected to exist with content " + expected + " but does not")


@Given('the user has paused the file sync')
def step(context):
    mouseClick(waitForObjectItem(names.stack_folderList_QTreeView, "_1"), 718, 39, Qt.NoModifier, Qt.LeftButton)
    activateItem(waitForObjectItem(names.settings_QMenu, "Pause sync"))


@Given('the user has changed the content of local file "|any|" to:')
def step(context, filename):
    fileContent = "\n".join(context.multiLineText)
    f = open(context.userData['clientSyncPath'] + filename, "w")
    f.write(fileContent)
    f.close()


@When('the user resumes the file sync on the client')
def step(context):
    mouseClick(waitForObjectItem(names.stack_folderList_QTreeView, "_1"), 719, 38, Qt.NoModifier, Qt.LeftButton)
    activateItem(waitForObjectItem(names.settings_QMenu, "Resume sync"))


@When('the user triggers force sync on the client')
def step(context):
    mouseClick(waitForObjectItem(names.stack_folderList_QTreeView, "_1"), 720, 36, Qt.NoModifier, Qt.LeftButton)
    activateItem(waitForObjectItem(names.settings_QMenu, "Force sync now"))


@Then('a conflict file for "|any|" should exist on the file system with following content')
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


@Then('an conflict warning should be shown for |integer| files')
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


@Then('the table for conflict warning should include file "|any|"')
def step(context, filename):
    waitForObject(names.settings_OCC_SettingsDialog)
    waitForObjectExists({
            "column": 1,
            "container": names.oCC_IssuesWidget_treeWidget_QTreeWidget,
            "text": RegularExpression(buildConflictedRegex(filename)),
            "type": "QModelIndex"
    })


@When('user selects the unsynced files tab with |integer| unsynced files')
def step(context, number):
    # TODO: find some way to dynamically select the tab name
    # It might take some time for all files to sync except the expected number of unsynced files
    snooze(10)
    clickTab(waitForObject(names.stack_QTabWidget), "Not Synced ({})".format(number))
