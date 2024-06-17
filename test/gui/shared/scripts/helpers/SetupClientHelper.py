import squish, test
import psutil
from urllib.parse import urlparse
from os import makedirs
from os.path import exists, join
from helpers.SpaceHelper import get_space_id
from helpers.ConfigHelper import get_config, set_config, isWindows
from helpers.SyncHelper import listenSyncStatusForItem
from helpers.api.utils import url_join


def substituteInLineCodes(value):
    value = value.replace('%local_server%', get_config('localBackendUrl'))
    value = value.replace('%secure_local_server%', get_config('secureLocalBackendUrl'))
    value = value.replace('%client_root_sync_path%', get_config('clientRootSyncPath'))
    value = value.replace('%current_user_sync_path%', get_config('currentUserSyncPath'))
    value = value.replace(
        '%local_server_hostname%', urlparse(get_config('localBackendUrl')).netloc
    )

    return value


def getClientDetails(context):
    clientDetails = {
        'server': '',
        'user': '',
        'password': '',
        'sync_folder': '',
        'oauth': False,
    }
    for row in context.table[0:]:
        row[1] = substituteInLineCodes(row[1])
        if row[0] == 'server':
            clientDetails.update({'server': row[1]})
        elif row[0] == 'user':
            clientDetails.update({'user': row[1]})
        elif row[0] == 'password':
            clientDetails.update({'password': row[1]})
        elif row[0] == 'sync_folder':
            clientDetails.update({'sync_folder': row[1]})
    return clientDetails


def createUserSyncPath(username):
    # '' at the end adds '/' to the path
    userSyncPath = join(get_config('clientRootSyncPath'), username, '')

    if not exists(userSyncPath):
        makedirs(userSyncPath)

    setCurrentUserSyncPath(userSyncPath)
    return userSyncPath.replace('\\', '/')


def createSpacePath(space='Personal'):
    spacePath = join(get_config('currentUserSyncPath'), space, '')
    if not exists(spacePath):
        makedirs(spacePath)
    return spacePath.replace('\\', '/')


def setCurrentUserSyncPath(syncPath):
    set_config('currentUserSyncPath', syncPath)


def getResourcePath(resource='', user='', space=''):
    sync_path = get_config('currentUserSyncPath')
    if user:
        sync_path = user
    if get_config('ocis'):
        space = space or get_config('syncConnectionName')
        sync_path = join(sync_path, space)
    sync_path = join(get_config('clientRootSyncPath'), sync_path)
    resource = resource.replace(sync_path, '').strip('/').strip('\\')
    if isWindows():
        resource = resource.replace('/', '\\')
    return join(
        sync_path,
        resource,
    )


def getTempResourcePath(resourceName):
    return join(get_config('tempFolderPath'), resourceName)


def getCurrentUserSyncPath():
    return get_config('currentUserSyncPath')


def startClient():
    squish.startApplication(
        "owncloud -s"
        + " --logfile "
        + get_config('clientLogFile')
        + " --logdebug"
        + " --logflush"
    )


def getPollingInterval():
    pollingInterval = '''[ownCloud]
    remotePollInterval={pollingInterval}
    '''
    args = {'pollingInterval': 5000}
    pollingInterval = pollingInterval.format(**args)
    return pollingInterval


def setUpClient(username, displayName, space="Personal"):
    userSetting = '''
    [Accounts]
    0/Folders/1/davUrl={url}
    0/Folders/1/ignoreHiddenFiles=true
    0/Folders/1/localPath={client_sync_path}
    0/Folders/1/displayString={displayString}
    0/Folders/1/paused=false
    0/Folders/1/targetPath=/
    0/Folders/1/version=2
    0/Folders/1/virtualFilesMode={vfs}
    0/dav_user={davUserName}
    0/display-name={displayUserName}
    0/http_CredentialVersion=1
    0/http_oauth={oauth}
    0/http_user={davUserName}
    0/url={local_server}
    0/user={displayUserFirstName}
    0/version=1
    version=2
    '''

    userSetting = userSetting + getPollingInterval()

    syncPath = createUserSyncPath(username)
    dav_endpoint = url_join("remote.php/dav/files", username)

    server_url = get_config('localBackendUrl')
    is_ocis = get_config('ocis')
    if is_ocis:
        set_config('syncConnectionName', space)
        syncPath = createSpacePath(space)
        if space == "Personal":
            space = displayName
        dav_endpoint = url_join("dav/spaces", get_space_id(space, username))

    args = {
        'url': url_join(server_url, dav_endpoint, ''),
        'displayString': get_config('syncConnectionName'),
        'displayUserName': displayName,
        'davUserName': username if is_ocis else username.lower(),
        'displayUserFirstName': displayName.split()[0],
        'client_sync_path': syncPath,
        'local_server': server_url,
        'oauth': 'true' if is_ocis else 'false',
        'vfs': 'wincfapi' if isWindows() else 'off',
    }
    userSetting = userSetting.format(**args)

    configFile = open(get_config('clientConfigFile'), "w")
    configFile.write(userSetting)
    configFile.close()

    startClient()
    listenSyncStatusForItem(syncPath)


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
        test.log(
            "Application was not terminated within {} milliseconds".format(timeout)
        )
