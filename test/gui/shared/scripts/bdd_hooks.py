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
import builtins


@OnScenarioStart
def hook(context):
    from configparser import ConfigParser

    CONFIG_ENV_MAP = {
        'localBackendUrl': 'BACKEND_HOST',
        'secureLocalBackendUrl': 'SECURE_BACKEND_HOST',
        'clientSyncTimeout': 'CLIENT_SYNC_TIMEOUT',
        'middlewareUrl': 'MIDDLEWARE_URL',
        'clientConfigFile': 'CLIENT_LOG_FILE',
        'clientSyncPathUser1': 'CLIENT_SYNC_PATH_USER1',
        'clientSyncPathUser2': 'CLIENT_SYNC_PATH_USER2',
    }

    DEFAULT_CONFIG = {
        'localBackendUrl': 'https://localhost:9200/',
        'secureLocalBackendUrl': 'https://localhost:9200/',
        'clientSyncTimeout': 60,
        'middlewareUrl': 'http://localhost:3000/',
        'clientConfigFile': '-',
        'clientSyncPathUser1': '/tmp/client-bdd-user1/',
        'clientSyncPathUser2': '/tmp/client-bdd-user2/',
    }

    # read configs from environment variables
    context.userData = {}
    for key, value in CONFIG_ENV_MAP.items():
        context.userData[key] = os.environ.get(value, '')

    # try reading configs from config.ini
    cfg = ConfigParser()
    try:
        cfg.read('../config.ini')
        for key, value in context.userData.items():
            if value == '':
                context.userData[key] = cfg.get('DEFAULT', CONFIG_ENV_MAP[key])
    except:
        print("Error reading config.ini file!")

    # Set the default values if empty
    for key, value in context.userData.items():
        if value == '':
            context.userData[key] = DEFAULT_CONFIG[key]
        elif key == 'clientSyncTimeout':
            context.userData[key] = builtins.int(value)
        elif key == 'clientSyncPathUser1' or key == 'clientSyncPathUser2':
            # make sure there is always one trailing slash
            context.userData[key] = value.rstrip('/') + '/'

    if not os.path.exists(context.userData['clientSyncPathUser1']):
        os.makedirs(context.userData['clientSyncPathUser1'])

    if not os.path.exists(context.userData['clientSyncPathUser2']):
        os.makedirs(context.userData['clientSyncPathUser2'])

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
