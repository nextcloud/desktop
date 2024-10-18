import os
import re
import sys
import test
import urllib.request
import squish

from helpers.ConfigHelper import get_config, is_linux, is_windows
from helpers.FilesHelper import sanitize_path

if is_windows():
    from helpers.WinPipeHelper import WinPipeConnect as SocketConnect
else:
    # NOTE: 'syncstate.py' was removed from client
    # and is now available at https://github.com/owncloud/client-desktop-shell-integration-nautilus
    # check if 'syncstate.py' is available, if not, download it
    custom_lib = get_config('custom_lib')
    syncstate_lib_file = os.path.join(custom_lib, 'syncstate.py')
    os.makedirs(custom_lib, exist_ok=True)

    if not os.path.exists(syncstate_lib_file):
        urllib.request.urlretrieve(
            'https://raw.github.com/owncloud/client-desktop-shell-integration-nautilus/master/src/syncstate.py',
            os.path.join(custom_lib, 'syncstate.py'),
        )

    # the script needs to use the system wide python
    # to switch from the built-in interpreter
    #   see https://kb.froglogic.com/squish/howto/using-external-python-interpreter-squish-6-6/
    # if the IDE fails to reference the script,
    # add the folder in Edit->Preferences->PyDev->Interpreters->Libraries
    sys.path.append(custom_lib)
    from custom_lib.syncstate import SocketConnect

# socket messages
socket_messages = []
SOCKET_CONNECT = None
# Whether wait has been made or not after account is set up
# This is useful for waiting only for the first time
WAITED_AFTER_SYNC = False

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
        # when adding account via New Account wizard
        [
            SYNC_STATUS['REGISTER'],
            SYNC_STATUS['UPDATE'],
            SYNC_STATUS['UPDATE'],
            SYNC_STATUS['UPDATE'],
        ],
        # when syncing empty account (hidden files are ignored)
        [SYNC_STATUS['UPDATE'], SYNC_STATUS['OK']],
        # when syncing an account that has some files/folders
        [SYNC_STATUS['SYNC'], SYNC_STATUS['OK']],
    ],
    'root_synced': [
        [
            SYNC_STATUS['SYNC'],
            SYNC_STATUS['OK'],
            SYNC_STATUS['OK'],
            SYNC_STATUS['OK'],
            SYNC_STATUS['UPDATE'],
        ],
        [
            SYNC_STATUS['SYNC'],
            SYNC_STATUS['UPDATE'],
            SYNC_STATUS['OK'],
            SYNC_STATUS['OK'],
            SYNC_STATUS['OK'],
            SYNC_STATUS['UPDATE'],
        ],
    ],
    'single_synced': [SYNC_STATUS['SYNC'], SYNC_STATUS['OK']],
    'error': [SYNC_STATUS['ERROR']],
}


def get_socket_connection():
    global SOCKET_CONNECT
    if not SOCKET_CONNECT or not SOCKET_CONNECT.connected:
        SOCKET_CONNECT = SocketConnect()
    return SOCKET_CONNECT


def read_socket_messages():
    messages = []
    socket_connect = get_socket_connection()
    socket_connect.read_socket_data_with_timeout(0.1)
    for line in socket_connect.get_available_responses():
        messages.append(line)
    return messages


def read_and_update_socket_messages():
    messages = read_socket_messages()
    return update_socket_messages(messages)


def update_socket_messages(messages):
    socket_messages.extend(filter_sync_messages(messages))
    return socket_messages


def clear_socket_messages(resource=''):
    global socket_messages
    if resource:
        resource_messages = set(filter_messages_for_item(socket_messages, resource))
        socket_messages = [
            msg for msg in socket_messages if msg not in resource_messages
        ]
    else:
        socket_messages.clear()


def close_socket_connection():
    socket_messages.clear()
    if SOCKET_CONNECT:
        SOCKET_CONNECT.connected = False
        if is_windows():
            SOCKET_CONNECT.close_conn()
        elif is_linux():
            SOCKET_CONNECT._sock.close()  # pylint: disable=protected-access


def get_initial_sync_patterns():
    return SYNC_PATTERNS['initial']


def get_synced_pattern(resource=''):
    # get only the resource path
    sync_path = get_config('currentUserSyncPath')
    if get_config('ocis'):
        sync_path = os.path.join(sync_path, get_config('syncConnectionName'))

    if resource := resource.replace(sync_path, '').strip('\\').strip('/'):
        return SYNC_PATTERNS['single_synced']
    return SYNC_PATTERNS['root_synced']


# generate sync pattern from the socket messages
#
# returns List
# e.g: ['UPDATE_VIEW', 'STATUS:OK']
def generate_sync_pattern_from_messages(messages):
    pattern = []
    if not messages:
        return pattern

    sync_messages = filter_sync_messages(messages)
    for message in sync_messages:
        # E.g; from "STATUS:OK:/tmp/client-bdd/Alice/"
        # excludes ":/tmp/client-bdd/Alice/"
        # adds only "STATUS:OK" to the pattern list
        if match := re.search(':(/|[A-Z]{1}:\\\\).*', message):
            (end, _) = match.span()
            # shared resources will have status like "STATUS:OK+SWM"
            status = message[:end].replace('+SWM', '')
            pattern.append(status)
    return pattern


# strip out the messages that are not related to sync
def filter_sync_messages(messages):
    start_idx = 0
    if 'GET_STRINGS:END' in messages:
        start_idx = messages.index('GET_STRINGS:END') + 1
    return messages[start_idx:]


def filter_messages_for_item(messages, item):
    filtered_messages = []
    for msg in messages:
        msg = msg.rstrip('/').rstrip('\\')
        item = item.rstrip('/').rstrip('\\')
        if msg.endswith(item):
            filtered_messages.append(msg)
    return filtered_messages


def listen_sync_status_for_item(item, resource_type='FOLDER'):
    if (resource_type := resource_type.upper()) not in ('FILE', 'FOLDER'):
        raise ValueError('resource_type must be "FILE" or "FOLDER"')
    socket_connect = get_socket_connection()
    item = item.rstrip('\\')
    socket_connect.sendCommand(f'RETRIEVE_{resource_type}_STATUS:{item}\n')


def get_current_sync_status(resource, resource_type):
    listen_sync_status_for_item(resource, resource_type)
    messages = filter_messages_for_item(read_socket_messages(), resource)
    # return the last message from the list
    return messages[-1]


def wait_for_resource_to_sync(resource, resource_type='FOLDER', patterns=None):
    listen_sync_status_for_item(resource, resource_type)

    timeout = get_config('maxSyncTimeout') * 1000

    if patterns is None:
        patterns = get_synced_pattern(resource)

    synced = squish.waitFor(
        lambda: has_sync_pattern(patterns, resource),
        timeout,
    )
    clear_socket_messages(resource)
    if not synced:
        # if the sync pattern doesn't match then check the last sync status
        # and pass the step if the last sync status is STATUS:OK
        status = get_current_sync_status(resource, resource_type)
        if status.startswith(SYNC_STATUS['OK']):
            test.log(
                '[WARN] Failed to match sync pattern for resource: '
                + resource
                + f'\nBut its last status is "{SYNC_STATUS["OK"]}"'
                + '. So passing the step.'
            )
            return
        raise TimeoutError(
            'Timeout while waiting for sync to complete for '
            + str(timeout)
            + ' milliseconds'
        )


def wait_for_initial_sync_to_complete(path):
    wait_for_resource_to_sync(
        path,
        'FOLDER',
        get_initial_sync_patterns(),
    )


def has_sync_pattern(patterns, resource=None):
    if isinstance(patterns[0], str):
        patterns = [patterns]
    messages = read_and_update_socket_messages()
    if resource:
        messages = filter_messages_for_item(messages, resource)
    for pattern in patterns:
        pattern_len = len(pattern)
        for idx, _ in enumerate(messages):
            actual_pattern = generate_sync_pattern_from_messages(
                messages[idx : idx + pattern_len]
            )
            if len(actual_pattern) < pattern_len:
                break
            if pattern_len == len(actual_pattern) and pattern == actual_pattern:
                return True
    # 100 milliseconds polling interval
    squish.snooze(0.1)
    return False


# Using socket API to check file sync status
def has_sync_status(item_name, status):
    sync_messages = read_and_update_socket_messages()
    sync_messages = filter_messages_for_item(sync_messages, item_name)
    for line in sync_messages:
        line = line.rstrip('/').rstrip('\\')
        item_name = item_name.rstrip('/').rstrip('\\')
        if line.startswith(status) and line.endswith(item_name):
            return True
    return False


# useful for checking sync status such as 'error', 'ignore'
# but not quite so reliable for checking 'ok' sync status
def wait_for_resource_to_have_sync_status(
    resource, resource_type, status=SYNC_STATUS['OK'], timeout=None
):
    resource = sanitize_path(resource)

    listen_sync_status_for_item(resource, resource_type)

    if not timeout:
        timeout = get_config('maxSyncTimeout') * 1000

    result = squish.waitFor(
        lambda: has_sync_status(resource, status),
        timeout,
    )

    if not result:
        if status == SYNC_STATUS['ERROR']:
            expected = 'have sync error'
        elif status == SYNC_STATUS['IGNORE']:
            expected = 'be sync ignored'
        else:
            expected = 'be synced'
        raise ValueError(
            f'Expected {resource_type} "{resource}" to {expected}, but not.'
        )


def wait_for_resource_to_have_sync_error(resource, resource_type):
    wait_for_resource_to_have_sync_status(resource, resource_type, SYNC_STATUS['ERROR'])


# performing actions immediately after completing the sync from the server does not work
# The test should wait for a while before performing the action
# issue: https://github.com/owncloud/client/issues/8832
def wait_for_client_to_be_ready():
    global WAITED_AFTER_SYNC
    if not WAITED_AFTER_SYNC:
        squish.snooze(get_config('minSyncTimeout'))
        WAITED_AFTER_SYNC = True


def clear_waited_after_sync():
    global WAITED_AFTER_SYNC
    WAITED_AFTER_SYNC = False
