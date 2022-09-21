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

# default sync patterns for the initial sync (after adding account)
SYNC_PATTERNS = {
    'root_sync': [
        {
            'length': 2,
            'pattern': {
                SYNC_STATUS['UPDATE']: [0],
                SYNC_STATUS['OK']: [1],
            },
        },
        {
            'length': 2,
            'pattern': {
                SYNC_STATUS['SYNC']: [0],
                SYNC_STATUS['OK']: [1],
            },
        },
    ],
}


# generate sync pattern from pattern meta data
#
# returns List
# e.g: ['UPDATE_VIEW', 'STATUS:OK']
def generateSyncPattern(pattern_meta):
    pattern = [None] * pattern_meta['length']
    for status in pattern_meta['pattern']:
        for idx in pattern_meta['pattern'][status]:
            pattern[idx] = status
    return pattern


def getRootSyncPatterns():
    patterns = []
    for pattern_meta in SYNC_PATTERNS['root_sync']:
        patterns.append(generateSyncPattern(pattern_meta))

    return patterns


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


def matchPatterns(p1, p2):
    if p1 == p2:
        return True
    return False
