from urllib.parse import urlparse


def substituteInLineCodes(context, value):
    value = value.replace('%local_server%', context.userData['localBackendUrl'])
    value = value.replace('%client_sync_path%', context.userData['clientSyncPath'])
    value = value.replace(
        '%local_server_hostname%', urlparse(context.userData['localBackendUrl']).netloc
    )

    return value


def getClientDetails(context):
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
    return server, user, password, localfolder
