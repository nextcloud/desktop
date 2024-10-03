from pageObjects.EnterPassword import EnterPassword

from helpers.UserHelper import get_password_for_user
from helpers.SetupClientHelper import setup_client, get_resource_path
from helpers.SyncHelper import wait_for_initial_sync_to_complete
from helpers.SpaceHelper import (
    create_space,
    create_space_folder,
    create_space_file,
    add_user_to_space,
    get_file_content,
    resource_exists,
)
from helpers.ConfigHelper import get_config


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
    password = get_password_for_user(user)
    setup_client(user, space_name)
    enter_password = EnterPassword()
    if get_config('ocis'):
        enter_password.accept_certificate()
    enter_password.login_after_setup(user, password)
    # wait for files to sync
    wait_for_initial_sync_to_complete(get_resource_path('/', user, space_name))


@Then(
    'as "|any|" the file "|any|" in the space "|any|" should have content "|any|" in the server'
)
def step(context, user, file_name, space_name, content):
    downloaded_content = get_file_content(space_name, file_name, user)
    test.compare(downloaded_content, content, 'Comparing file content')


@Then(
    r'as "([^"]*)" the space "([^"]*)" should have (?:folder|file) "([^"]*)" in the server',
    regexp=True,
)
def step(context, user, space_name, resource_name):
    exists = resource_exists(space_name, resource_name, user)
    test.compare(exists, True, 'Resource exists')
