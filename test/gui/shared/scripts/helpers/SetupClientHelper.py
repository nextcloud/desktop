from urllib.parse import urlparse
import squish


confdir = '/tmp/bdd-tests-owncloud-client/'
confFilePath = confdir + 'owncloud.cfg'


def substituteInLineCodes(context, value):
    value = value.replace('%local_server%', context.userData['localBackendUrl'])
    value = value.replace(
        '%secure_local_server%', context.userData['secureLocalBackendUrl']
    )
    value = value.replace(
        '%client_sync_path_user1%', context.userData['clientSyncPathUser1']
    )
    value = value.replace(
        '%client_sync_path_user2%', context.userData['clientSyncPathUser2']
    )
    value = value.replace(
        '%local_server_hostname%', urlparse(context.userData['localBackendUrl']).netloc
    )

    return value


def getClientDetails(context):
    clientDetails = {'server': '', 'user': '', 'password': '', 'localfolder': ''}
    for row in context.table[0:]:
        row[1] = substituteInLineCodes(context, row[1])
        if row[0] == 'server':
            clientDetails.update({'server': row[1]})
        elif row[0] == 'user':
            clientDetails.update({'user': row[1]})
        elif row[0] == 'password':
            clientDetails.update({'password': row[1]})
        elif row[0] == 'localfolder':
            clientDetails.update({'localfolder': row[1]})
        try:
            os.makedirs(localfolder, 0o0755)
        except:
            pass
    return clientDetails


def startClient(context):
    squish.startApplication(
        "owncloud -s"
        + " --logfile "
        + context.userData['clientConfigFile']
        + " --logdebug"
        + " --logflush"
        + " --confdir "
        + confdir
    )
    squish.snooze(1)


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
    userSetting = userSetting + getPollingInterval()
    args = {
        'displayUserName': displayName,
        'davUserName': userFirstName[0].lower(),
        'displayUserFirstName': userFirstName[0],
        'client_sync_path': context.userData['clientSyncPathUser1'],
        'local_server': context.userData['localBackendUrl'],
    }
    userSetting = userSetting.format(**args)
    configFile = open(confFilePath, "w")
    configFile.write(userSetting)
    configFile.close()

    startClient(context)
