# -*- coding: utf-8 -*-

# This file contains hook functions to run as the .feature file is executed.
#
# A common use-case is to use the OnScenarioStart/OnScenarioEnd hooks to
# start and stop an AUT, e.g.
#
# @OnScenarioStart
# def hook(context):
#     startApplication("addressbook")
#
# @OnScenarioEnd
# def hook(context):
#     currentApplicationContext().detach()
#
# See the section 'Performing Actions During Test Execution Via Hooks' in the Squish
# manual for a complete reference of the available API.
import shutil

@OnScenarioStart
def hook(context):
    from configparser import ConfigParser
    cfg = ConfigParser()
    cfg.read('../config.ini')
    context.userData = {
        'localBackendUrl': os.environ.get('BACKEND_HOST', cfg.get('DEFAULT','BACKEND_HOST')),
        'clientSyncPath':  os.environ.get('CLIENT_SYNC_PATH', cfg.get('DEFAULT','CLIENT_SYNC_PATH')),
        'clientSyncTimeout':  os.environ.get('CLIENT_SYNC_TIMEOUT', cfg.get('DEFAULT','CLIENT_SYNC_TIMEOUT')),
        'middlewareUrl': os.environ.get('MIDDLEWARE_URL', cfg.get('DEFAULT','MIDDLEWARE_URL')),
    }

    if context.userData['localBackendUrl'] == '':
        context.userData['localBackendUrl']='https://localhost:9200'
    if context.userData['clientSyncPath'] == '':
        context.userData['clientSyncPath']='/tmp/client-bdd/'
    else:
        context.userData['clientSyncPath'] = context.userData['clientSyncPath'].rstrip("/") + "/" # make sure there is always one trailing slash
    if context.userData['clientSyncTimeout'] == '':
        context.userData['clientSyncTimeout']=60
    else:
        context.userData['clientSyncTimeout']=int(context.userData['clientSyncTimeout'])

    if not os.path.exists(context.userData['clientSyncPath']):
        os.makedirs(context.userData['clientSyncPath'])

    if context.userData['middlewareUrl'] == '':
        context.userData['middlewareUrl']='http://localhost:3000/'

@OnScenarioEnd
def hook(context):
    # Detach (i.e. potentially terminate) all AUTs at the end of a scenario
    for ctx in applicationContextList():
        ctx.detach()
        snooze(5) #ToDo wait smarter till the app died

    # delete local files/folders
    for filename in os.listdir(context.userData['clientSyncPath']):
        test.log("Deleting :" + filename)
        file_path = os.path.join(context.userData['clientSyncPath'], filename)
        try:
            if os.path.isfile(file_path) or os.path.islink(file_path):
                os.unlink(file_path)
            elif os.path.isdir(file_path):
                shutil.rmtree(file_path)
        except Exception as e:
            print('Failed to delete %s. Reason: %s' % (file_path, e))
