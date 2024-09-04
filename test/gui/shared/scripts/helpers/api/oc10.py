import json

import helpers.api.HttpHelper as request
from helpers.api.utils import url_join
from helpers.ConfigHelper import get_config


def format_json(url):
    return url + "?format=json"


def get_ocs_url():
    return url_join(get_config('localBackendUrl'), 'ocs', "v2.php")


def get_provisioning_url(*paths):
    return format_json(url_join(get_ocs_url(), 'cloud', *paths))


def checkSuccessOcsStatus(response):
    if response.text:
        ocs_code = json.loads(response.text)['ocs']['meta']['statuscode']
        if ocs_code not in [100, 200]:
            raise Exception("Request failed." + response.text)
    else:
        raise Exception(
            "No OCS response body. HTTP status was " + str(response.status_code)
        )


def create_user(username, password, displayname, email):
    url = get_provisioning_url("users")
    body = {
        "userid": username,
        "password": password,
        "displayname": displayname,
        "email": email,
    }
    response = request.post(url, body)
    request.assertHttpStatus(response, 200, f"Failed to create user '{username}'")
    checkSuccessOcsStatus(response)

    # oc10 does not set display name while creating user,
    # so we need update the user info
    user_url = get_provisioning_url("users", username)
    display_name_body = {"key": "displayname", "value": displayname}
    display_name_response = request.put(user_url, display_name_body)
    request.assertHttpStatus(
        display_name_response, 200, f"Failed to update user '{username}' display name"
    )
    checkSuccessOcsStatus(display_name_response)

    return {
        "id": username,
        "username": username,
        "password": password,
        "displayname": displayname,
        "email": email,
    }


def delete_user(user_id):
    url = get_provisioning_url('users', user_id)
    response = request.delete(url)
    request.assertHttpStatus(response, 200, f"Failed to delete user '{user_id}'")
    checkSuccessOcsStatus(response)


def create_group(group_name):
    body = {"groupid": group_name}
    response = request.post(get_provisioning_url("groups"), body)
    request.assertHttpStatus(response, 200, f"Failed to create group '{group_name}'")
    checkSuccessOcsStatus(response)
    return {"id": group_name}


def delete_group(group_id):
    url = get_provisioning_url("groups", group_id)
    response = request.delete(url)
    request.assertHttpStatus(response, 200, f"Failed to delete group '{group_id}'")
    checkSuccessOcsStatus(response)


def add_user_to_group(user, group_name):
    url = get_provisioning_url("users", user, "groups")
    body = {"groupid": group_name}
    response = request.post(url, body)
    request.assertHttpStatus(
        response, 200, f"Failed to add user '{user}' to group '{group_name}'"
    )
    checkSuccessOcsStatus(response)


def enable_app(app_name):
    url = format_json(url_join(get_ocs_url(), "apps", app_name))
    response = request.post(url)
    request.assertHttpStatus(response, 200, f"Failed to enable app '{app_name}'")
    checkSuccessOcsStatus(response)


def disable_app(app_name):
    url = format_json(url_join(get_ocs_url(), "apps", app_name))
    response = request.delete(url)
    request.assertHttpStatus(response, 200, f"Failed to disable app '{app_name}'")
    checkSuccessOcsStatus(response)


def setup_app(app_name, action):
    if action.startswith('enable'):
        enable_app(app_name)
    elif action.startswith('disable'):
        disable_app(app_name)
    else:
        raise Exception("Unknown action: " + action)
