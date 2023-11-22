from pageObjects.EnterPassword import EnterPassword

from helpers.UserHelper import getDisplaynameForUser, getPasswordForUser
from helpers.SetupClientHelper import setUpClient, getResourcePath
from helpers.SyncHelper import waitForInitialSyncToComplete
from helpers.SpaceHelper import (
    create_space,
    create_space_folder,
    create_space_file,
    add_user_to_space,
    get_file_content,
    resource_exists,
)


@Given('the administrator has created a space "|any|"')
def step(context, space_name):
    create_space(space_name)


@Given('the administrator has created a folder "|any|" in space "|any|"')
def step(context, folder_name, space_name):
    create_space_folder(space_name, folder_name)


@Given(
    'the administrator has uploaded a file "|any|" with content "|any|" inside space "|any|"'
)
def step(context, file_name, content, space_name):
    create_space_file(space_name, file_name, content)


@Given('the administrator has added user "|any|" to space "|any|" with role "|any|"')
def step(context, user, space_name, role):
    add_user_to_space(user, space_name, role)


@Given('user "|any|" has set up a client with space "|any|"')
def step(context, user, space_name):
    password = getPasswordForUser(user)
    displayName = getDisplaynameForUser(user)
    setUpClient(user, displayName, space_name)
    EnterPassword.loginAfterSetup(user, password)
    # wait for files to sync
    waitForInitialSyncToComplete(getResourcePath('/', user, space_name))


@Then(
    'as "|any|" the file "|any|" in the space "|any|" should have content "|any|" in the server'
)
def step(context, user, file_name, space_name, content):
    downloaded_content = get_file_content(space_name, file_name, user)
    test.compare(downloaded_content, content, "Comparing file content")


@Then(
    r'as "([^"]*)" the space "([^"]*)" should have (?:folder|file) "([^"]*)" in the server',
    regexp=True,
)
def step(context, user, space_name, resource_name):
    exists = resource_exists(space_name, resource_name, user)
    test.compare(exists, True, "Resource exists")
