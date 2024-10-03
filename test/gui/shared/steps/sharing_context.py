import os
import squish

from pageObjects.PublicLinkDialog import PublicLinkDialog
from pageObjects.SharingDialog import SharingDialog

from helpers.SetupClientHelper import get_resource_path, substitute_inline_codes
from helpers.FilesHelper import sanitize_path
from helpers.SyncHelper import (
    get_socket_connection,
    wait_for_resource_to_have_sync_status,
    SYNC_STATUS,
)
from helpers.ConfigHelper import get_config


def send_share_command(resource):
    socket_connect = get_socket_connection()
    socket_connect.sendCommand(f'SHARE:{resource}\n')
    if not socket_connect.read_socket_data_with_timeout(0.1):
        return False
    for line in socket_connect.get_available_responses():
        if line.startswith('SHARE:OK') and line.endswith(resource):
            return True
    return False


def open_sharing_dialog(resource):
    resource = get_resource_path(resource)
    resource_exist = squish.waitFor(
        lambda: os.path.exists(resource), get_config('maxSyncTimeout') * 1000
    )
    if not resource_exist:
        raise FileNotFoundError(f"{resource} doesn't exists")
    share_dialog = squish.waitFor(
        lambda: send_share_command(resource),
        get_config('maxSyncTimeout') * 1000,
    )
    if not share_dialog:
        raise LookupError(f"Sharing dialog didn't open for {resource}")


def create_public_link_share(resource, password='', permissions='', expire_date=''):
    open_sharing_dialog(resource)
    PublicLinkDialog.open_public_link_tab()
    PublicLinkDialog.create_public_link(password, permissions, expire_date)


def create_public_share_with_role(resource, role):
    resource = sanitize_path(substitute_inline_codes(resource))
    open_sharing_dialog(resource)
    PublicLinkDialog.open_public_link_tab()
    PublicLinkDialog.create_public_link_with_role(role)


def collaborator_should_be_listed(receiver, resource, permissions, receiver_count=0):
    # wait for client to the view
    wait_for_resource_to_have_sync_status(
        get_resource_path(), 'FOLDER', SYNC_STATUS['UPDATE']
    )
    open_sharing_dialog(resource)
    check_collaborator_and_premissions(receiver, permissions, receiver_count)
    SharingDialog.close_sharing_dialog()


def check_collaborator_and_premissions(receiver, permissions, receiver_count=0):
    permissions_list = permissions.split(',')

    shared_with_obj = SharingDialog.get_collaborators()

    #     we use sharedWithObj list from above while verifying if users are listed or not.
    #     For this we need an index value i.e receiver_count
    #     For 1st user in the list the index will be 0 which is receiver_count default value
    #     For 2nd user in the list the index will be 1 and so on

    test.compare(str(shared_with_obj[receiver_count].text), receiver)
    test.compare(
        SharingDialog.has_edit_permission(),
        ('edit' in permissions_list),
    )
    test.compare(
        SharingDialog.has_share_permission(),
        ('share' in permissions_list),
    )


@When(
    'the user adds "|any|" as collaborator of resource "|any|" with permissions "|any|" using the client-UI'
)
def step(context, receiver, resource, permissions):
    open_sharing_dialog(resource)
    SharingDialog.add_collaborator(receiver, permissions)
    SharingDialog.close_sharing_dialog()


@When('the user adds following collaborators of resource "|any|" using the client-UI')
def step(context, resource):
    open_sharing_dialog(resource)

    # In the following loop we are trying to share resource with given permission
    # to one user at a timegiven from the data table in the feature file
    for count, row in enumerate(context.table[1:]):
        receiver = row[0]
        permissions = row[1]
        SharingDialog.add_collaborator(receiver, permissions, False, count + 1)

    SharingDialog.close_sharing_dialog()


@When(
    'the user selects "|any|" as collaborator of resource "|any|" using the client-UI'
)
def step(context, receiver, resource):
    open_sharing_dialog(resource)
    SharingDialog.select_collaborator(receiver)


@When(
    'the user adds group "|any|" as collaborator of resource "|any|" with permissions "|any|" using the client-UI'
)
def step(context, receiver, resource, permissions):
    open_sharing_dialog(resource)
    SharingDialog.add_collaborator(receiver, permissions, True)
    SharingDialog.close_sharing_dialog()


@Then(
    'user "|any|" should be listed in the collaborators list for file "|any|" with permissions "|any|" on the client-UI'
)
def step(context, receiver, resource, permissions):
    collaborator_should_be_listed(receiver, resource, permissions)


@Then(
    # pylint: disable=line-too-long
    'group "|any|" should be listed in the collaborators list for file "|any|" with permissions "|any|" on the client-UI'
)
def step(context, receiver, resource, permissions):
    receiver += ' (group)'
    collaborator_should_be_listed(receiver, resource, permissions)


@When('the user opens the public links dialog of "|any|" using the client-UI')
def step(context, resource):
    open_sharing_dialog(resource)
    PublicLinkDialog.open_public_link_tab()


@When('the user toggles the password protection using the client-UI')
def step(context):
    PublicLinkDialog.toggle_password()


@Then('the password progress indicator should not be visible in the client-UI')
def step(context):
    squish.waitFor(lambda: (test.vp('publicLinkPasswordProgressIndicatorInvisible')))


@Then(
    'the password progress indicator should not be visible in the client-UI - expected to fail'
)
def step(context):
    squish.waitFor(lambda: (test.xvp('publicLinkPasswordProgressIndicatorInvisible')))


@When('the user opens the sharing dialog of "|any|" using the client-UI')
def step(context, resource):
    open_sharing_dialog(resource)


@Then('the text "|any|" should be displayed in the sharing dialog')
def step(context, message):
    test.compare(
        SharingDialog.get_sharing_dialog_message(),
        message,
    )


@Then('the error text "|any|" should be displayed in the sharing dialog')
def step(context, error_message):
    test.compare(
        SharingDialog.get_sharing_dialog_message(),
        error_message,
    )


@When(
    'the user creates a new public link for file "|any|" without password using the client-UI'
)
def step(context, resource):
    create_public_link_share(resource)


@When(
    'the user creates a new public link for file "|any|" with password "|any|" using the client-UI'
)
def step(context, resource, password):
    create_public_link_share(resource, password)


@Then('the expiration date of the last public link of file "|any|" should be "|any|"')
def step(context, resource, expiry_date):
    # wait for client to the view
    wait_for_resource_to_have_sync_status(
        get_resource_path(), 'FOLDER', SYNC_STATUS['UPDATE']
    )
    open_sharing_dialog(resource)
    PublicLinkDialog.open_public_link_tab()

    if expiry_date.strip('%') == 'default':
        expiry_date = PublicLinkDialog.get_default_expiry_date()
    actual_expiry_date = PublicLinkDialog.get_expiration_date()
    test.compare(expiry_date, actual_expiry_date)

    SharingDialog.close_sharing_dialog()


@When('the user edits the public link named "|any|" of file "|any|" changing following')
def step(context, _, __):
    expire_date = ''
    for row in context.table:
        if row[0] == 'expireDate':
            expire_date = row[1]
            break
    PublicLinkDialog.set_expiration_date(expire_date)


@When(
    # pylint: disable=line-too-long
    'the user creates a new public link with permissions "|any|" for folder "|any|" without password using the client-UI'
)
def step(context, permissions, resource):
    create_public_link_share(resource, '', permissions)


@When(
    # pylint: disable=line-too-long
    'the user creates a new public link with permissions "|any|" for folder "|any|" with password "|any|" using the client-UI'
)
def step(context, permissions, resource, password):
    create_public_link_share(resource, password, permissions)


@When('the user creates a new public link with following settings using the client-UI:')
def step(context):
    link_settings = {}
    for row in context.table:
        link_settings[row[0]] = row[1]

    if 'path' not in link_settings:
        raise ValueError("'path' is required but not given.")

    if 'expireDate' in link_settings and link_settings['expireDate'] == '%default%':
        link_settings['expireDate'] = link_settings['expireDate'].strip('%')

    create_public_link_share(
        resource=link_settings['path'],
        password=link_settings['password'] if 'password' in link_settings else None,
        expire_date=(
            link_settings['expireDate'] if 'expireDate' in link_settings else None
        ),
    )


@When(
    'the user creates a new public link for folder "|any|" using the client-UI with these details:'
)
def step(context, resource):
    role = ''
    for row in context.table:
        if row[0] == 'role':
            role = row[1]
            break

    if not role:
        raise ValueError('No role has been found')

    create_public_share_with_role(resource, role)


@When(
    'the user removes permissions "|any|" for user "|any|" of resource "|any|" using the client-UI'
)
def step(context, permissions, _, resource):
    open_sharing_dialog(resource)
    SharingDialog.remove_permissions(permissions)


@Step('the user closes the sharing dialog')
def step(context):
    SharingDialog.close_sharing_dialog()


@Then(
    '"|any|" permissions should not be displayed for user "|any|" for resource "|any|" on the client-UI'
)
def step(context, permissions, _, __):
    permissions_list = permissions.split(',')

    edit_checked, share_checked = SharingDialog.get_available_permission()

    if 'edit' in permissions_list:
        test.compare(edit_checked, False)

    if 'share' in permissions_list:
        test.compare(share_checked, False)


@Then('the error "|any|" should be displayed in the sharing dialog')
def step(context, error_message):
    test.compare(SharingDialog.get_error_text(), error_message)


@When(
    'the user tires to share resource "|any|" with the group "|any|" using the client-UI'
)
def step(context, resource, group):
    open_sharing_dialog(resource)

    SharingDialog.select_collaborator(group, True)


@When(
    'the user unshares the resource "|any|" for collaborator "|any|" using the client-UI'
)
def step(context, resource, _):
    open_sharing_dialog(resource)
    SharingDialog.unshare_with()


@When('the user deletes the public link for file "|any|"')
def step(context, resource):
    open_sharing_dialog(resource)
    PublicLinkDialog.open_public_link_tab()
    PublicLinkDialog.delete_public_link()


@When(
    'the user changes the password of public link "|any|" to "|any|" using the client-UI'
)
def step(context, _, password):
    PublicLinkDialog.change_password(password)


@Then(
    'the following users should be listed in as collaborators for file "|any|" on the client-UI'
)
def step(context, resource):
    # wait for client to the view
    wait_for_resource_to_have_sync_status(
        get_resource_path(), 'FOLDER', SYNC_STATUS['UPDATE']
    )
    open_sharing_dialog(resource)
    #     Here we are trying to verify if the user added in when step are listed in the client-UI or not
    #     We now have a variable name receiverCount which is used in collaborator_should_be_listed function call
    receiver_count = 0
    for row in context.table[1:]:
        receiver = row[0]
        permissions = row[1]

        check_collaborator_and_premissions(receiver, permissions, receiver_count)
        receiver_count += 1

    SharingDialog.close_sharing_dialog()


@When('the user searches for collaborator "|any|" using the client-UI')
def step(context, collaborator):
    SharingDialog.search_collaborator(collaborator)


@When(
    'the user searches for collaborator with autocomplete characters "|any|" using the client-UI'
)
def step(context, collaborator):
    SharingDialog.search_collaborator(collaborator)


@Then('the following users should be listed as suggested collaborators:')
def step(context):
    for collaborator in context.table[1:]:
        test.compare(
            SharingDialog.is_user_in_suggestion_list(collaborator[0]),
            True,
            "Assert user '" + collaborator[0] + "' is listed",
        )


@Then('the collaborators should be listed in the following order:')
def step(context):
    for index, collaborator in enumerate(context.table[1:], start=1):
        test.compare(
            SharingDialog.get_collaborator_name(index),
            collaborator[0],
        )
