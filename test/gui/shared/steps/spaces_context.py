from pageObjects.EnterPassword import EnterPassword

from helpers.UserHelper import getDisplaynameForUser, getPasswordForUser
from helpers.SetupClientHelper import setUpClient
from helpers.SyncHelper import waitForInitialSyncToComplete
from helpers.SetupClientHelper import getResourcePath
from helpers.SpaceHelper import (
    create_space,
    create_space_folder,
    create_space_file,
    add_user_to_space,
)


@Given('the administrator has created a space "|any|"')
def step(context, space_name):
    create_space(context, space_name)


@Given('the administrator has created a folder "|any|" in space "|any|"')
def step(context, folder_name, space_name):
    create_space_folder(context, space_name, folder_name)


@Given(
    'the administrator has uploaded a file "|any|" with content "|any|" inside space "|any|"'
)
def step(context, file_name, content, space_name):
    create_space_file(context, space_name, file_name, content)


@Given('the administrator has added user "|any|" to space "|any|" with role "|any|"')
def step(context, user, space_name, role):
    add_user_to_space(context, user, space_name, role)


@Given('user "|any|" has set up a client with space "|any|"')
def step(context, user, space_name):
    password = getPasswordForUser(context, user)
    displayName = getDisplaynameForUser(context, user)
    setUpClient(
        context, user, displayName, context.userData['clientConfigFile'], space_name
    )
    EnterPassword.loginAfterSetup(context, user, password)
    # wait for files to sync
    sync_path = getResourcePath(context, '/', user, space_name)
    waitForInitialSyncToComplete(context, sync_path)
