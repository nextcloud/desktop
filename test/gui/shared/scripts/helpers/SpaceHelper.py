import requests
import json
from urllib import parse
from base64 import b64encode
from os import path

from helpers.UserHelper import getPasswordForUser
from helpers.ConfigHelper import get_config

requests.packages.urllib3.disable_warnings()

created_spaces = {}
user_spaces = {}
space_role = ['manager', 'editor', 'viewer']


def auth_header(user=None):
    # default admin auth
    token = b64encode(b"admin:admin").decode()
    if user:
        password = getPasswordForUser(user)
        token = b64encode(("%s:%s" % (user, password)).encode()).decode()
    return {"Authorization": "Basic " + token}


def send_request(url, method, body=None, headers={}, user=None):
    auth = auth_header(user)
    headers.update(auth)
    return requests.request(method, url, data=body, headers=headers, verify=False)


def get_space_endpint(context):
    return path.join(get_config('localBackendUrl'), 'graph', 'v1.0', 'drives')


def get_dav_endpint(context):
    return path.join(get_config('localBackendUrl'), 'dav', 'spaces')


def get_share_endpint(context):
    return path.join(
        get_config('localBackendUrl'),
        'ocs/v2.php/apps',
        'files_sharing/api/v1/shares',
    )


def create_space(context, space_name):
    body = json.dumps({"Name": space_name})
    response = send_request(get_space_endpint(context), "POST", body)
    if response.status_code != 201:
        raise Exception(
            "Creating space '%s' failed with %s\n" % (space_name, response.status_code)
            + response.text
        )
    # save created space
    resp_object = response.json()
    created_spaces[space_name] = resp_object['id']


def fetch_spaces(context, user=None, query=''):
    if query:
        query = '?' + query
    url = get_space_endpint(context) + query
    response = send_request(url, "GET", user=user)
    if response.status_code != 200:
        raise Exception(
            "Getting spaces failed with %s\n" % response.status_code + response.text
        )
    return response.json()['value']


def get_project_spaces(context, user=None):
    search_query = '$filter=driveType eq \'project\''
    return fetch_spaces(context, query=search_query, user=user)


def get_space_id(context, space_name, user=None):
    global created_spaces, user_spaces
    spaces = {**created_spaces, **user_spaces}
    if not space_name in spaces.keys():
        return fetch_space_id(context, space_name, user)
    for space, id in spaces.items():
        if space == space_name:
            return id


def fetch_space_id(context, space_name, user=None):
    global user_spaces
    spaces = fetch_spaces(context, user=user)
    for space in spaces:
        if space['name'] == space_name:
            user_spaces[space_name] = space['id']
            return space['id']


def delete_project_spaces(context):
    global created_spaces, user_spaces
    for _, id in created_spaces.items():
        disable_project_space(context, id)
        delete_project_space(context, id)
    created_spaces = {}
    user_spaces = {}


def disable_project_space(context, space_id):
    url = path.join(get_space_endpint(context), space_id)
    response = send_request(url, "DELETE")
    if response.status_code != 204:
        raise Exception(
            "Disabling space '%s' failed with %s\n" % (space_id, response.status_code)
            + response.text
        )


def delete_project_space(context, space_id):
    url = path.join(get_space_endpint(context), space_id)
    response = send_request(url, "DELETE", headers={"Purge": "T"})
    if response.status_code != 204:
        raise Exception(
            "Deleting space '%s' failed with %s\n" % (space_id, response.status_code)
            + response.text
        )


def create_space_folder(context, space_name, folder_name):
    space_id = get_space_id(context, space_name)
    url = path.join(get_dav_endpint(context), space_id, folder_name)
    response = send_request(url, "MKCOL")
    if response.status_code != 201:
        raise Exception(
            "Creating folder '%s' in space '%s' failed with %s\n"
            % (folder_name, space_name, response.status_code)
            + response.text
        )


def create_space_file(context, space_name, file_name, content):
    space_id = get_space_id(context, space_name)
    url = path.join(get_dav_endpint(context), space_id, file_name)
    response = send_request(url, "PUT", content)
    if response.status_code != 201 and response.status_code != 204:
        raise Exception(
            "Creating file '%s' in space '%s' failed with %s\n"
            % (file_name, space_name, response.status_code)
            + response.text
        )


def add_user_to_space(context, user, space_name, role):
    global space_role
    role = role.lower()
    if not role in space_role:
        raise Exception("Cannot set the role '%s' to a space" % role)

    space_id = get_space_id(context, space_name)
    url = get_share_endpint(context)
    body = parse.urlencode(
        {
            "space_ref": space_id,
            "shareType": 7,
            "shareWith": user,
            "role": role,
        }
    )
    headers = {'Content-Type': 'application/x-www-form-urlencoded'}
    response = send_request(url, "POST", body, headers=headers)
    if response.status_code != 200:
        raise Exception(
            "Adding user '%s' to space '%s' failed with %s\n"
            % (user, space_name, response.status_code)
            + response.text
        )
