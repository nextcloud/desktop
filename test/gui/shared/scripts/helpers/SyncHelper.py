import re
import sys
import test
from os.path import realpath
from squish import waitFor, snooze

# the script needs to use the system wide python
# to switch from the built-in interpreter see https://kb.froglogic.com/squish/howto/using-external-python-interpreter-squish-6-6/
# if the IDE fails to reference the script, add the folder in Edit->Preferences->PyDev->Interpreters->Libraries
sys.path.append(realpath('../../../shell_integration/nautilus/'))
from syncstate import SocketConnect

from helpers.FilesHelper import sanitizePath
from helpers.ConfigHelper import get_config

# socket messages
socket_messages = []
socketConnect = None
# Whether wait has been made or not after account is set up
# This is useful for waiting only for the first time
waitedAfterSync = False

# File syncing in client has the following status
SYNC_STATUS = {
    'SYNC': 'STATUS:SYNC',  # sync in progress
    'OK': 'STATUS:OK',  # sync completed
    'ERROR': 'STATUS:ERROR',  # sync error
    'IGNORE': 'STATUS:IGNORE',  # sync ignored
    'NOP': 'STATUS:NOP',  # not in sync yet
    'REGISTER': 'REGISTER_PATH',
    'UNREGISTER': 'UNREGISTER_PATH',
    'UPDATE': 'UPDATE_VIEW',
}

SYNC_PATTERNS = {
    # default sync patterns for the initial sync (after adding account)
    # the pattern can be of TWO types depending on the available resources (files/folders)
    'initial': [
        # when syncing empty account (hidden files are ignored)
        [SYNC_STATUS['UPDATE'], SYNC_STATUS['OK']],
        # when syncing an account that has some files/folders
        [SYNC_STATUS['SYNC'], SYNC_STATUS['OK']],
    ],
    'synced': [SYNC_STATUS['SYNC'], SYNC_STATUS['OK']],
    'error': [SYNC_STATUS['ERROR']],
}


def getSocketConnection():
    global socketConnect
    if not socketConnect or not socketConnect.connected:
        socketConnect = SocketConnect()
    return socketConnect


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


def closeSocketConnection():
    global socketConnect, socket_messages
    socket_messages.clear()
    if socketConnect:
        socketConnect.connected = False
        socketConnect._sock.close()


def getInitialSyncPatterns():
    return SYNC_PATTERNS['initial']


def getSyncedPattern():
    return SYNC_PATTERNS['synced']


# generate sync pattern from the socket messages
#
# returns List
# e.g: ['UPDATE_VIEW', 'STATUS:OK']
def generateSyncPatternFromMessages(messages):
    pattern = []
    if not messages:
        return pattern

    sync_messages = filterSyncMessages(messages)
    for message in sync_messages:
        # E.g; from "STATUS:OK:/tmp/client-bdd/Alice/"
        # excludes ":/tmp/client-bdd/Alice/"
        # adds only "STATUS:OK" to the pattern list
        match = re.search(":/.*", message)
        if match:
            (end, _) = match.span()
            # shared resources will have status like "STATUS:OK+SWM"
            status = message[:end].replace('+SWM', '')
            pattern.append(status)
    return pattern


# strip out the messages that are not related to sync
def filterSyncMessages(messages):
    start_idx = 0
    if 'GET_STRINGS:END' in messages:
        start_idx = messages.index('GET_STRINGS:END') + 1
    return messages[start_idx:]


def filterMessagesForItem(messages, item):
    filteredMsg = []
    for msg in messages:
        if msg.rstrip('/').endswith(item.rstrip('/')):
            filteredMsg.append(msg)
    return filteredMsg


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


def waitForFileOrFolderToSync(context, resource, resourceType='FOLDER', patterns=None):
    listenSyncStatusForItem(resource, resourceType)

    timeout = get_config('maxSyncTimeout') * 1000

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


def waitForInitialSyncToComplete(context, path):
    waitForFileOrFolderToSync(
        context,
        path,
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
            if pattern_len == len(actual_pattern) and pattern == actual_pattern:
                return True
    # 100 milliseconds polling interval
    snooze(0.1)
    return False


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
    resource = sanitizePath(resource)

    listenSyncStatusForItem(resource, resourceType)

    if not timeout:
        timeout = get_config('maxSyncTimeout') * 1000

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


# performing actions immediately after completing the sync from the server does not work
# The test should wait for a while before performing the action
# issue: https://github.com/owncloud/client/issues/8832
def waitForClientToBeReady(context):
    global waitedAfterSync
    if not waitedAfterSync:
        snooze(get_config('minSyncTimeout'))
        waitedAfterSync = True


def clearWaitedAfterSync():
    global waitedAfterSync
    waitedAfterSync = False
