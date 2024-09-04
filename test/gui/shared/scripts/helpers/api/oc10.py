import json

import helpers.api.HttpHelper as request
from helpers.api.utils import url_join
from helpers.ConfigHelper import get_config


def get_ocs_url():
    return url_join(get_config('localBackendUrl'), 'ocs', "v2.php", 'cloud')


def checkSuccessOcsStatus(response):
    if response.text:
        ocs_code = json.loads(response.text)['ocs']['meta']['statuscode']
        if ocs_code not in [100, 200]:
            raise Exception("Request failed." + response.text)
    else:
        raise Exception(
            "No OCS response body. HTTP status was " + str(response.status_code)
        )


def create_group(group_name):
    url = url_join(get_ocs_url(), 'groups')
    body = {"groupid": group_name}
    response = request.post(url, body)
    request.assertHttpStatus(response, 200, f"Failed to create group '{group_name}'")
    return {"id": group_name}


def delete_group(group_id):
    url = url_join(get_ocs_url(), 'groups', group_id)
    response = request.delete(url)
    request.assertHttpStatus(response, 200, f"Failed to delete group '{group_id}'")


def add_user_to_group(user, group_name):
    url = url_join(get_ocs_url(), "users", user, "groups")
    body = {"groupid": group_name}
    response = request.post(url, body)
    request.assertHttpStatus(
        response, 200, f"Failed to add user '{user}' to group '{group_name}'"
    )


def create_user(username, password, displayname, email):
    url = url_join(get_ocs_url(), "users")
    body = {
        "userid": username,
        "password": password,
        "displayname": displayname,
        "email": email,
    }
    response = request.post(url, body)
    request.assertHttpStatus(response, 200, f"Failed to create user '{username}'")

    # oc10 does not set display name while creating user,
    # so we need update the user info
    user_url = url_join(get_ocs_url(), "users", username)
    display_name_body = {"key": "displayname", "value": displayname}
    display_name_response = request.put(user_url, display_name_body)
    request.assertHttpStatus(
        display_name_response, 200, f"Failed to update user '{username}' display name"
    )

    return {
        "id": username,
        "username": username,
        "password": password,
        "displayname": displayname,
        "email": email,
    }


def delete_user(userid):
    url = url_join(get_ocs_url(), 'users', userid)
    response = request.delete(url)
    request.assertHttpStatus(response, 200, f"Failed to delete user '{userid}'")
