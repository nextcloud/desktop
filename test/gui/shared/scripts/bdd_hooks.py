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
import urllib.request
import os


@OnScenarioStart
def hook(context):
    from configparser import ConfigParser

    cfg = ConfigParser()
    cfg.read('../config.ini')
    context.userData = {
        'localBackendUrl': os.environ.get(
            'BACKEND_HOST', cfg.get('DEFAULT', 'BACKEND_HOST')
        ),
        'clientSyncPathUser1': os.environ.get(
            'CLIENT_SYNC_PATH_USER1', cfg.get('DEFAULT', 'CLIENT_SYNC_PATH_USER1')
        ),
        'clientSyncPathUser2': os.environ.get(
            'CLIENT_SYNC_PATH_USER2', cfg.get('DEFAULT', 'CLIENT_SYNC_PATH_USER2')
        ),
        'clientSyncTimeout': os.environ.get(
            'CLIENT_SYNC_TIMEOUT', cfg.get('DEFAULT', 'CLIENT_SYNC_TIMEOUT')
        ),
        'middlewareUrl': os.environ.get(
            'MIDDLEWARE_URL', cfg.get('DEFAULT', 'MIDDLEWARE_URL')
        ),
        'clientConfigFile': os.environ.get(
            'CLIENT_LOG_FILE', cfg.get('DEFAULT', 'CLIENT_LOG_FILE')
        ),
    }

    if context.userData['localBackendUrl'] == '':
        context.userData['localBackendUrl'] = 'https://localhost:9200'
    if context.userData['clientSyncPathUser1'] == '':
        context.userData['clientSyncPathUser1'] = '/tmp/client-bdd-user1/'
    else:
        context.userData['clientSyncPathUser1'] = (
            context.userData['clientSyncPathUser1'].rstrip("/") + "/"
        )  # make sure there is always one trailing slash
    if context.userData['clientSyncPathUser2'] == '':
        context.userData['clientSyncPathUser2'] = '/tmp/client-bdd-user2/'
    else:
        context.userData['clientSyncPathUser2'] = (
            context.userData['clientSyncPathUser2'].rstrip("/") + "/"
        )  # make sure there is always one trailing slash
    if context.userData['clientSyncTimeout'] == '':
        context.userData['clientSyncTimeout'] = 60
    else:
        context.userData['clientSyncTimeout'] = int(
            context.userData['clientSyncTimeout']
        )

    if not os.path.exists(context.userData['clientSyncPathUser1']):
        os.makedirs(context.userData['clientSyncPathUser1'])

    if not os.path.exists(context.userData['clientSyncPathUser2']):
        os.makedirs(context.userData['clientSyncPathUser2'])

    if context.userData['middlewareUrl'] == '':
        context.userData['middlewareUrl'] = 'http://localhost:3000/'

    if context.userData['clientConfigFile'] == '':
        context.userData['clientConfigFile'] = '-'

    req = urllib.request.Request(
        os.path.join(context.userData['middlewareUrl'], 'init'),
        headers={"Content-Type": "application/json"},
        method='POST',
    )
    try:
        urllib.request.urlopen(req)
    except urllib.error.HTTPError as e:
        raise Exception(
            "Step execution through test middleware failed. Error: " + e.read().decode()
        )


@OnScenarioEnd
def hook(context):
    # Detach (i.e. potentially terminate) all AUTs at the end of a scenario
    for ctx in applicationContextList():
        ctx.detach()
        snooze(5)  # ToDo wait smarter till the app died

    # delete local files/folders
    for filename in os.listdir(context.userData['clientSyncPathUser1']):
        test.log("Deleting :" + filename)
        file_path = os.path.join(context.userData['clientSyncPathUser1'], filename)
        try:
            if os.path.isfile(file_path) or os.path.islink(file_path):
                os.unlink(file_path)
            elif os.path.isdir(file_path):
                shutil.rmtree(file_path)
        except Exception as e:
            print('Failed to delete %s. Reason: %s' % (file_path, e))

    for filename in os.listdir(context.userData['clientSyncPathUser2']):
        test.log("Deleting :" + filename)
        file_path = os.path.join(context.userData['clientSyncPathUser2'], filename)
        try:
            if os.path.isfile(file_path) or os.path.islink(file_path):
                os.unlink(file_path)
            elif os.path.isdir(file_path):
                shutil.rmtree(file_path)
        except Exception as e:
            print('Failed to delete %s. Reason: %s' % (file_path, e))
