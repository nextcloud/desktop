import helpers.api.HttpHelper as request
import helpers.api.oc10 as oc
import helpers.api.ocis as ocis
from helpers.api.utils import url_join
from helpers.ConfigHelper import get_config
import helpers.UserHelper as UserHelper

created_groups = {}


def format_json(url):
    return url + "?format=json"


def enable_app(app_name):
    url = format_json(url_join(oc.get_ocs_url(), "apps", app_name))
    response = request.post(url)
    request.assertHttpStatus(response, 200, f"Failed to enable app '{app_name}'")
    oc.checkSuccessOcsStatus(response)


def disable_app(app_name):
    url = format_json(url_join(oc.get_ocs_url(), "apps", app_name))
    response = request.delete(url)
    request.assertHttpStatus(response, 200, f"Failed to disable app '{app_name}'")
    oc.checkSuccessOcsStatus(response)


def setup_app(app_name, action):
    if action.startswith('enable'):
        enable_app(app_name)
    elif action.startswith('disable'):
        disable_app(app_name)
    else:
        raise Exception("Unknown action: " + action)


def create_group(group_name):
    global created_groups
    if get_config('ocis'):
        group_info = ocis.create_group(group_name)
    else:
        group_info = oc.create_group(group_name)
    created_groups[group_name] = group_info


def delete_created_groups():
    global created_groups
    for group_name, group_info in list(created_groups.items()):
        if get_config('ocis'):
            ocis.delete_group(group_info["id"])
        else:
            oc.delete_group(group_info["id"])
        del created_groups[group_name]


def add_user_to_group(user, group_name):
    if get_config('ocis'):
        ocis.add_user_to_group(user, group_name)
    else:
        oc.add_user_to_group(user, group_name)


def create_user(username):
    user = {}
    if username in UserHelper.test_users:
        user = UserHelper.test_users[username]
    else:
        user = {
            "username": username,
            "displayname": username,
            "email": f'{username}@mail.com',
            "password": UserHelper.test_users["regularuser"]["password"],
        }

    if get_config('ocis'):
        user_info = ocis.create_user(
            user['username'], user['password'], user['displayname'], user['email']
        )
    else:
        user_info = oc.create_user(
            user['username'], user['password'], user['displayname'], user['email']
        )
    UserHelper.createdUsers[username] = user_info


def delete_created_users():
    for username, user_info in list(UserHelper.createdUsers.items()):
        if get_config('ocis'):
            ocis.delete_user(user_info["id"])
        else:
            oc.delete_user(user_info["id"])
        del UserHelper.createdUsers[username]
