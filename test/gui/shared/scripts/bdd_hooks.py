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
from urllib.parse import urlparse
from helpers.StacktraceHelper import getCoredumps, generateStacktrace
from helpers.SyncHelper import closeSocketConnection, clearWaitedAfterSync
from helpers.SpaceHelper import delete_project_spaces
from helpers.SetupClientHelper import wait_until_app_killed
from helpers.ConfigHelper import (
    init_config,
    get_config,
    set_config,
    clear_scenario_config,
    isWindows,
    isLinux,
)
from helpers.api.utils import url_join
from datetime import datetime
from pageObjects.Toolbar import Toolbar
from pageObjects.AccountSetting import AccountSetting
from pageObjects.AccountConnectionWizard import AccountConnectionWizard

# this will reset in every test suite
previousFailResultCount = 0
previousErrorResultCount = 0


# runs before a feature
# Order: 1
@OnFeatureStart
def hook(context):
    init_config()


# runs before every scenario
# Order: 1
@OnScenarioStart
def hook(context):
    clear_scenario_config()


# runs before every scenario
# Order: 2
@OnScenarioStart
def hook(context):
    # set owncloud config file path
    config_dir = get_config('clientConfigDir')
    if os.path.exists(config_dir):
        if len(os.listdir(config_dir)) != 0 and isWindows():
            raise Exception(
                "Looks like you have previous client config in '"
                + config_dir
                + "'\n[DANGER] Delete it and try again.\n[DANGER] Removing config file will make client to lost the previously added accounts."
            )
        # clean previous configs
        shutil.rmtree(config_dir)
    os.makedirs(config_dir, 0o0755)
    set_config('clientConfigFile', os.path.join(config_dir, 'owncloud.cfg'))

    # create reports dir if not exists
    test_report_dir = get_config('guiTestReportDir')
    if not os.path.exists(test_report_dir):
        os.makedirs(test_report_dir)

    # log tests scenario title on serverlog file
    if os.getenv('CI'):
        f = open(test_report_dir + "/serverlog.log", "a")
        f.write(
            str((datetime.now()).strftime("%H:%M:%S:%f"))
            + "\tBDD Scenario: "
            + context._data["title"]
            + "\n"
        )
        f.close()

    # this path will be changed according to the user added to the client
    # e.g.: /tmp/client-bdd/Alice
    set_config('currentUserSyncPath', '')

    root_sync_dir = get_config('clientRootSyncPath')
    if not os.path.exists(root_sync_dir):
        os.makedirs(root_sync_dir)

    tmp_dir = get_config('tempFolderPath')
    if not os.path.exists(tmp_dir):
        os.makedirs(tmp_dir)

    req = urllib.request.Request(
        url_join(get_config('middlewareUrl'), 'init'),
        headers={"Content-Type": "application/json"},
        method='POST',
    )
    try:
        urllib.request.urlopen(req)
    except urllib.error.HTTPError as e:
        raise Exception(
            "Step execution through test middleware failed. Error: " + e.read().decode()
        )

    # sync connection folder display name
    set_config('syncConnectionName', "Personal" if get_config("ocis") else "ownCloud")


# determines if the test scenario failed or not
# Currently, this workaround is needed because we cannot find out a way to determine the pass/fail status of currently running test scenario.
# And, resultCount("errors")  and resultCount("fails") return the total number of error/failed test scenarios of a test suite.
def scenarioFailed():
    global previousFailResultCount
    global previousErrorResultCount
    return (
        test.resultCount("fails") - previousFailResultCount > 0
        or test.resultCount("errors") - previousErrorResultCount > 0
    )


# runs after every scenario
# Order: 1
# cleanup spaces
@OnScenarioEnd
def hook(context):
    if get_config('ocis'):
        delete_project_spaces()


# runs after every scenario
# Order: 2
@OnScenarioEnd
def hook(context):
    clearWaitedAfterSync()
    closeSocketConnection()

    global previousFailResultCount, previousErrorResultCount

    # capture a screenshot if there is error or test failure in the current scenario execution
    if scenarioFailed() and os.getenv('CI') and isLinux():
        import gi

        gi.require_version('Gtk', '3.0')
        from gi.repository import Gdk

        window = Gdk.get_default_root_window()
        pb = Gdk.pixbuf_get_from_window(window, *window.get_geometry())

        # scenario name can have "/" which is invalid filename
        filename = (
            context._data["title"].replace(" ", "_").replace("/", "_").strip(".")
            + ".png"
        )
        directory = os.path.join(get_config('guiTestReportDir'), "screenshots")

        if not os.path.exists(directory):
            os.makedirs(directory)

        pb.savev(os.path.join(directory, filename), "png", [], [])

    # teardown accounts and configs
    teardown_client()

    # search coredumps after every test scenario
    # CI pipeline might fail although all tests are passing
    coredumps = getCoredumps()
    if coredumps:
        try:
            generateStacktrace(context._data["title"], coredumps)
            test.log("Stacktrace generated!")
        except Exception as err:
            test.log("Exception occured:" + str(err))
    elif scenarioFailed():
        test.log("No coredump found!")

    # cleanup test server
    req = urllib.request.Request(
        url_join(get_config('middlewareUrl'), 'cleanup'),
        headers={"Content-Type": "application/json"},
        method='POST',
    )
    try:
        urllib.request.urlopen(req)
    except urllib.error.HTTPError as e:
        raise Exception(
            "Step execution through test middleware failed. Error: " + e.read().decode()
        )

    previousFailResultCount = test.resultCount("fails")
    previousErrorResultCount = test.resultCount("errors")


def teardown_client():
    # Cleanup user accounts from UI for Windows platform
    # It is not needed for Linux so skipping it in order to save CI time
    if isWindows():
        # remove account from UI
        # In Windows, removing only config and sync folders won't help
        # so to work around that, remove the account connection
        close_dialogs()
        close_widgets()
        server_host = urlparse(get_config('localBackendUrl')).netloc
        accounts = Toolbar.get_accounts()
        for account in accounts:
            displayname = account.split('\n')[0]
            Toolbar.openAccount(displayname, server_host)
            AccountSetting.removeAccountConnection()
        if accounts:
            waitForObject(AccountConnectionWizard.SERVER_ADDRESS_BOX)

    # Detach (i.e. potentially terminate) all AUTs at the end of a scenario
    for ctx in applicationContextList():
        # get pid before detaching
        pid = ctx.pid
        ctx.detach()
        wait_until_app_killed(pid)

    # clean up config files
    shutil.rmtree(get_config('clientConfigDir'))

    # delete test files/folders
    for entry in os.scandir(get_config('clientRootSyncPath')):
        test.log("Deleting: " + entry.name)
        try:
            if entry.is_file() or entry.is_symlink():
                os.unlink(entry.path)
            elif entry.is_dir():
                shutil.rmtree(entry.path)
        except Exception as e:
            test.log(f'Failed to delete{entry.name}. Reason: {e}.')


def close_dialogs():
    # close the current active dailog if it's not a main client window
    while True:
        active_window = QApplication.activeModalWidget()
        if str(active_window) == "<null>":
            break
        test.log(f"Closing '{active_window.objectName}' window")
        closed = active_window.close()
        if not closed:
            confirm_dialog = QApplication.activeModalWidget()
            if confirm_dialog.visible:
                clickButton(waitForObject(AccountSetting.CONFIRMATION_YES_BUTTON))


def close_widgets():
    try:
        ch = object.children(squish.waitForObject(AccountSetting.DIALOG_STACK, 500))
        for obj in ch:
            if (
                hasattr(obj, "objectName")
                and obj.objectName != ''
                and obj.objectName != "page"
            ):
                obj.close()
                # if the dialog has a confirmation dialog, confirm it
                confirm_dialog = QApplication.activeModalWidget()
                if str(confirm_dialog) != "<null>" and confirm_dialog.visible:
                    clickButton(waitForObject(AccountSetting.CONFIRMATION_YES_BUTTON))
    except LookupError:
        # nothing to close if DIALOG_STACK is not found
        # required for client versions <= 5
        pass
