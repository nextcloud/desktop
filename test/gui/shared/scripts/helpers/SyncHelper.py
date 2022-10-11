import re


# File syncing in client has the following status
SYNC_STATUS = {
    'SYNC': 'STATUS:SYNC',  # sync in process
    'OK': 'STATUS:OK',  # sync completed
    'ERROR': 'STATUS:ERROR',  # file sync has error
    'IGNORE': 'STATUS:IGNORE',  # file is igored
    'NOP': 'STATUS:NOP',  # file yet to be synced
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
            pattern.append(message[:end])
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


def matchPatterns(p1, p2):
    if p1 == p2:
        return True
    return False
