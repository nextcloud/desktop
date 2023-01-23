from urllib.parse import urlparse
import squish
from os import makedirs
from os.path import exists, join


def substituteInLineCodes(context, value):
    value = value.replace('%local_server%', context.userData['localBackendUrl'])
    value = value.replace(
        '%secure_local_server%', context.userData['secureLocalBackendUrl']
    )
    value = value.replace(
        '%client_root_sync_path%', context.userData['clientRootSyncPath']
    )
    value = value.replace(
        '%current_user_sync_path%', context.userData['currentUserSyncPath']
    )
    value = value.replace(
        '%local_server_hostname%', urlparse(context.userData['localBackendUrl']).netloc
    )

    return value


def getClientDetails(context):
    clientDetails = {'server': '', 'user': '', 'password': ''}
    for row in context.table[0:]:
        row[1] = substituteInLineCodes(context, row[1])
        if row[0] == 'server':
            clientDetails.update({'server': row[1]})
        elif row[0] == 'user':
            clientDetails.update({'user': row[1]})
        elif row[0] == 'password':
            clientDetails.update({'password': row[1]})
    return clientDetails


def createUserSyncPath(context, username):
    # '' at the end adds '/' to the path
    userSyncPath = join(context.userData['clientRootSyncPath'], username, '')

    if not exists(userSyncPath):
        makedirs(userSyncPath)

    setCurrentUserSyncPath(context, userSyncPath)
    return userSyncPath


def createSpacePath(context, space='Personal'):
    spacePath = join(context.userData['currentUserSyncPath'], space, '')
    if not exists(spacePath):
        makedirs(spacePath)
    return spacePath


def setCurrentUserSyncPath(context, syncPath):
    context.userData['currentUserSyncPath'] = syncPath


def getResourcePath(context, resource='', user='', space=''):
    sync_path = context.userData['currentUserSyncPath']
    if user:
        sync_path = user
    if context.userData['ocis']:
        # default space for oCIS is "Personal"
        space = space or "Personal"
        sync_path = join(sync_path, space)
    sync_path = join(context.userData['clientRootSyncPath'], sync_path)
    resource = resource.replace(sync_path, '').strip('/')
    return join(
        sync_path,
        resource,
    )


def getCurrentUserSyncPath(context):
    return context.userData['currentUserSyncPath']


def startClient(context):
    squish.startApplication(
        "owncloud -s"
        + " --logfile "
        + context.userData['clientLogFile']
        + " --logdebug"
        + " --logflush"
        + " --confdir "
        + context.userData['clientConfigDir']
    )


def getPollingInterval():
    pollingInterval = '''[ownCloud]
    remotePollInterval={pollingInterval}
    '''
    args = {'pollingInterval': 5000}
    pollingInterval = pollingInterval.format(**args)
    return pollingInterval


def setUpClient(context, username, displayName, confFilePath):
    userSetting = '''
    [Accounts]
    0/Folders/1/ignoreHiddenFiles=true
    0/Folders/1/localPath={client_sync_path}
    0/Folders/1/displayString={displayString}
    0/Folders/1/paused=false
    0/Folders/1/targetPath=/
    0/Folders/1/version=2
    0/Folders/1/virtualFilesMode=off
    0/dav_user={davUserName}
    0/display-name={displayUserName}
    0/http_oauth={oauth}
    0/http_user={davUserName}
    0/url={local_server}
    0/user={displayUserFirstName}
    0/version=1
    version=2
    '''

    userSetting = userSetting + getPollingInterval()

    syncPath = createUserSyncPath(context, username)
    if context.userData['ocis']:
        syncPath = createSpacePath(context)

    args = {
        'displayString': context.userData['syncConnectionName'],
        'displayUserName': displayName,
        'davUserName': username if context.userData['ocis'] else username.lower(),
        'displayUserFirstName': displayName.split()[0],
        'client_sync_path': syncPath,
        'local_server': context.userData['localBackendUrl'],
        'oauth': 'true' if context.userData['ocis'] else 'false',
    }
    userSetting = userSetting.format(**args)

    configFile = open(confFilePath, "w")
    configFile.write(userSetting)
    configFile.close()

    startClient(context)
