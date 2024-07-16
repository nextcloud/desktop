import json
from urllib import parse

from helpers.ConfigHelper import get_config
from helpers.api.utils import url_join
import helpers.api.HttpHelper as request

created_spaces = {}
user_spaces = {}
space_role = ['manager', 'editor', 'viewer']


def get_space_endpint():
    return url_join(get_config('localBackendUrl'), 'graph', 'v1.0', 'drives')


def get_dav_endpint():
    return url_join(get_config('localBackendUrl'), 'dav', 'spaces')


def get_share_endpint():
    return url_join(
        get_config('localBackendUrl'),
        'ocs/v2.php/apps',
        'files_sharing/api/v1/shares',
    )


def create_space(space_name):
    body = json.dumps({"name": space_name})
    response = request.post(get_space_endpint(), body)
    if response.status_code != 201:
        raise Exception(
            "Creating space '%s' failed with %s\n" % (space_name, response.status_code)
            + response.text
        )
    # save created space
    resp_object = response.json()
    created_spaces[space_name] = resp_object['id']


def fetch_spaces(user=None, query=''):
    if query:
        query = '?' + query
    url = get_space_endpint() + query
    response = request.get(url=url, user=user)
    if response.status_code != 200:
        raise Exception(
            "Getting spaces failed with %s\n" % response.status_code + response.text
        )
    return response.json()['value']


def get_project_spaces(user=None):
    search_query = '$filter=driveType eq \'project\''
    return fetch_spaces(query=search_query, user=user)


def get_space_id(space_name, user=None):
    global created_spaces, user_spaces
    spaces = {**created_spaces, **user_spaces}
    if not space_name in spaces.keys():
        return fetch_space_id(space_name, user)
    for space, id in spaces.items():
        if space == space_name:
            return id


def fetch_space_id(space_name, user=None):
    global user_spaces
    spaces = fetch_spaces(user=user)
    for space in spaces:
        if space['name'] == space_name:
            user_spaces[space_name] = space['id']
            return space['id']


def delete_project_spaces():
    global created_spaces, user_spaces
    for _, id in created_spaces.items():
        disable_project_space(id)
        delete_project_space(id)
    created_spaces = {}
    user_spaces = {}


def disable_project_space(space_id):
    url = url_join(get_space_endpint(), space_id)
    response = request.delete(url)
    if response.status_code != 204:
        raise Exception(
            "Disabling space '%s' failed with %s\n" % (space_id, response.status_code)
            + response.text
        )


def delete_project_space(space_id):
    url = url_join(get_space_endpint(), space_id)
    response = request.delete(url, {"Purge": "T"})
    if response.status_code != 204:
        raise Exception(
            "Deleting space '%s' failed with %s\n" % (space_id, response.status_code)
            + response.text
        )


def create_space_folder(space_name, folder_name):
    space_id = get_space_id(space_name)
    url = url_join(get_dav_endpint(), space_id, folder_name)
    response = request.mkcol(url)
    if response.status_code != 201:
        raise Exception(
            "Creating folder '%s' in space '%s' failed with %s\n"
            % (folder_name, space_name, response.status_code)
            + response.text
        )


def create_space_file(space_name, file_name, content):
    space_id = get_space_id(space_name)
    url = url_join(get_dav_endpint(), space_id, file_name)
    response = request.put(url, content)
    if response.status_code != 201 and response.status_code != 204:
        raise Exception(
            "Creating file '%s' in space '%s' failed with %s\n"
            % (file_name, space_name, response.status_code)
            + response.text
        )


def add_user_to_space(user, space_name, role):
    global space_role
    role = role.lower()
    if not role in space_role:
        raise Exception("Cannot set the role '%s' to a space" % role)

    space_id = get_space_id(space_name)
    url = get_share_endpint()
    body = parse.urlencode(
        {
            "space_ref": space_id,
            "shareType": 7,
            "shareWith": user,
            "role": role,
        }
    )
    headers = {'Content-Type': 'application/x-www-form-urlencoded'}
    response = request.post(url, body, headers)
    if response.status_code != 200:
        raise Exception(
            "Adding user '%s' to space '%s' failed with %s\n"
            % (user, space_name, response.status_code)
            + response.text
        )


def get_file_content(space_name, file_name, user=None):
    space_id = get_space_id(space_name, user)
    url = url_join(get_dav_endpint(), space_id, file_name)
    response = request.get(url=url, user=user)
    if response.status_code != 200:
        raise Exception(
            "Getting file '%s' from space '%s' failed with %s\n"
            % (file_name, space_name, response.status_code)
            + response.text
        )
    return response.text


def resource_exists(space_name, resource, user=None):
    space_id = get_space_id(space_name, user)
    url = url_join(get_dav_endpint(), space_id, resource)
    response = request.get(url=url, user=user)
    if response.status_code == 200:
        return True
    return False
