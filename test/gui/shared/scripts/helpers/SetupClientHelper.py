import uuid
import os
import subprocess
from urllib.parse import urlparse
from os import makedirs
from os.path import exists, join
import test
import psutil
import squish

from helpers.SpaceHelper import get_space_id
from helpers.ConfigHelper import get_config, set_config, is_windows
from helpers.SyncHelper import listen_sync_status_for_item
from helpers.api.utils import url_join
from helpers.UserHelper import get_displayname_for_user


def substitute_inline_codes(value):
    value = value.replace('%local_server%', get_config('localBackendUrl'))
    value = value.replace('%secure_local_server%', get_config('secureLocalBackendUrl'))
    value = value.replace('%client_root_sync_path%', get_config('clientRootSyncPath'))
    value = value.replace('%current_user_sync_path%', get_config('currentUserSyncPath'))
    value = value.replace(
        '%local_server_hostname%', urlparse(get_config('localBackendUrl')).netloc
    )
    value = value.replace('%home%', get_config('home_dir'))

    return value


def get_client_details(context):
    client_details = {
        'server': '',
        'user': '',
        'password': '',
        'sync_folder': '',
        'oauth': False,
    }
    for row in context.table[0:]:
        row[1] = substitute_inline_codes(row[1])
        if row[0] == 'server':
            client_details.update({'server': row[1]})
        elif row[0] == 'user':
            client_details.update({'user': row[1]})
        elif row[0] == 'password':
            client_details.update({'password': row[1]})
        elif row[0] == 'sync_folder':
            client_details.update({'sync_folder': row[1]})
    return client_details


def create_user_sync_path(username):
    # '' at the end adds '/' to the path
    user_sync_path = join(get_config('clientRootSyncPath'), username, '')

    if not exists(user_sync_path):
        makedirs(user_sync_path)

    set_current_user_sync_path(user_sync_path)
    return user_sync_path.replace('\\', '/')


def create_space_path(space='Personal'):
    space_path = join(get_config('currentUserSyncPath'), space, '')
    if not exists(space_path):
        makedirs(space_path)
    return space_path.replace('\\', '/')


def set_current_user_sync_path(sync_path):
    set_config('currentUserSyncPath', sync_path)


def get_resource_path(resource='', user='', space=''):
    sync_path = get_config('currentUserSyncPath')
    if user:
        sync_path = user
    if get_config('ocis'):
        space = space or get_config('syncConnectionName')
        sync_path = join(sync_path, space)
    sync_path = join(get_config('clientRootSyncPath'), sync_path)
    resource = resource.replace(sync_path, '').strip('/').strip('\\')
    if is_windows():
        resource = resource.replace('/', '\\')
    return join(
        sync_path,
        resource,
    )


def get_temp_resource_path(resource_name):
    return join(get_config('tempFolderPath'), resource_name)


def get_current_user_sync_path():
    return get_config('currentUserSyncPath')


def start_client():
    squish.startApplication(
        'owncloud -s'
        + f' --logfile {get_config("clientLogFile")}'
        + ' --logdebug'
        + ' --logflush'
    )
    if get_config('screenRecordOnFailure'):
        test.startVideoCapture()


def get_polling_interval():
    polling_interval = '''
[ownCloud]
remotePollInterval={polling_interval}
'''
    args = {'polling_interval': 5000}
    polling_interval = polling_interval.format(**args)
    return polling_interval


def generate_account_config(users, space='Personal'):
    sync_paths = {}
    user_setting = ''
    for idx, username in enumerate(users):
        user_setting += '''
{user_index}/Folders/{uuid_v4}/davUrl={url}
{user_index}/Folders/{uuid_v4}/ignoreHiddenFiles=true
{user_index}/Folders/{uuid_v4}/localPath={client_sync_path}
{user_index}/Folders/{uuid_v4}/displayString={displayString}
{user_index}/Folders/{uuid_v4}/paused=false
{user_index}/Folders/{uuid_v4}/targetPath=/
{user_index}/Folders/{uuid_v4}/version=13
{user_index}/Folders/{uuid_v4}/virtualFilesMode=off
{user_index}/dav_user={davUserName}
{user_index}/display-name={displayUserName}
{user_index}/http_CredentialVersion=1
{user_index}/http_oauth={oauth}
{user_index}/http_user={davUserName}
{user_index}/url={local_server}
{user_index}/user={displayUserFirstName}
{user_index}/supportsSpaces={supportsSpaces}
{user_index}/version=13
'''
        if not idx:
            user_setting = '[Accounts]' + user_setting

        sync_path = create_user_sync_path(username)
        dav_endpoint = url_join('remote.php/dav/files', username)

        server_url = get_config('localBackendUrl')

        if is_ocis := get_config('ocis'):
            set_config('syncConnectionName', space)
            sync_path = create_space_path(space)
            space_name = space
            if space == 'Personal':
                space_name = get_displayname_for_user(username)
            dav_endpoint = url_join('dav/spaces', get_space_id(space_name, username))

        args = {
            'url': url_join(server_url, dav_endpoint, ''),
            'displayString': get_config('syncConnectionName'),
            'displayUserName': get_displayname_for_user(username),
            'davUserName': username if is_ocis else username.lower(),
            'displayUserFirstName': get_displayname_for_user(username).split()[0],
            'client_sync_path': sync_path,
            'local_server': server_url,
            'oauth': 'true' if is_ocis else 'false',
            'vfs': 'wincfapi' if is_windows() else 'off',
            'supportsSpaces': 'true' if is_ocis else 'false',
            'user_index': idx,
            'uuid_v4': generate_uuidv4(),
        }
        user_setting = user_setting.format(**args)
        sync_paths.update({username: sync_path})
    # append extra configs
    user_setting += 'version=13'
    user_setting = user_setting + get_polling_interval()

    with open(get_config('clientConfigFile'), 'a+', encoding='utf-8') as config_file:
        config_file.write(user_setting)

    return sync_paths


def setup_client(username, space='Personal'):
    sync_paths = generate_account_config([username], space)
    start_client()
    for _, sync_path in sync_paths.items():
        listen_sync_status_for_item(sync_path)


def is_app_killed(pid):
    try:
        psutil.Process(pid)
        return False
    except psutil.NoSuchProcess:
        return True


def wait_until_app_killed(pid=0):
    timeout = 5 * 1000
    killed = squish.waitFor(
        lambda: is_app_killed(pid),
        timeout,
    )
    if not killed:
        test.log(f'Application was not terminated within {timeout} milliseconds')


def generate_uuidv4():
    return str(uuid.uuid4())


# sometimes the keyring is locked during the test execution
# and we need to unlock it
def unlock_keyring():
    if is_windows():
        return

    stdout, stderr, _ = run_sys_command(
        [
            'busctl',
            '--user',
            'get-property',
            'org.freedesktop.secrets',
            '/org/freedesktop/secrets/collection/login',
            'org.freedesktop.Secret.Collection',
            'Locked',
        ]
    )
    output = ''
    if stdout:
        output = stdout.decode('utf-8')
    if stderr:
        output = stderr.decode('utf-8')

    if not output.strip().endswith('false'):
        test.log('Unlocking keyring...')
        password = os.getenv('VNC_PW')
        command = f'echo -n "{password}" | gnome-keyring-daemon -r -d --unlock'
        stdout, stderr, returncode = run_sys_command(command, True)
        if stdout:
            output = stdout.decode('utf-8')
        if stderr:
            output = stderr.decode('utf-8')
        if returncode:
            test.log(f'Failed to unlock keyring:\n{output}')


def run_sys_command(command=None, shell=False):
    cmd = subprocess.run(
        command,
        shell=shell,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    return cmd.stdout, cmd.stderr, cmd.returncode
