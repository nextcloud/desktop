import json
from urllib import parse

from helpers.ConfigHelper import get_config
from helpers.api.utils import url_join
import helpers.api.http_helper as request

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
    body = json.dumps({'name': space_name})
    response = request.post(get_space_endpint(), body)
    request.assert_http_status(response, 201, f'Failed to create space {space_name}')
    # save created space
    resp_object = response.json()
    created_spaces[space_name] = resp_object['id']


def fetch_spaces(user=None, query=''):
    if query:
        query = '?' + query
    url = get_space_endpint() + query
    response = request.get(url=url, user=user)
    request.assert_http_status(response, 200, 'Failed to get spaces')
    return response.json()['value']


def get_project_spaces(user=None):
    search_query = '$filter=driveType eq \'project\''
    return fetch_spaces(query=search_query, user=user)


def get_space_id(space_name, user=None):
    spaces = {**created_spaces, **user_spaces}
    if not space_name in spaces.keys():
        return fetch_space_id(space_name, user)
    return spaces.get(space_name)


def fetch_space_id(space_name, user=None):
    spaces = fetch_spaces(user=user)
    space_id = None
    for space in spaces:
        if space['name'] == space_name:
            user_spaces[space_name] = space['id']
            space_id = space['id']
            break
    return space_id


def delete_project_spaces():
    global created_spaces, user_spaces
    for _, space_id in created_spaces.items():
        disable_project_space(space_id)
        delete_project_space(space_id)
    created_spaces = {}
    user_spaces = {}


def disable_project_space(space_id):
    url = url_join(get_space_endpint(), space_id)
    response = request.delete(url)
    request.assert_http_status(response, 204, f'Failed to disable space {space_id}')


def delete_project_space(space_id):
    url = url_join(get_space_endpint(), space_id)
    response = request.delete(url, {'Purge': 'T'})
    request.assert_http_status(response, 204, f'Failed to delete space {space_id}')


def create_space_folder(space_name, folder_name):
    space_id = get_space_id(space_name)
    url = url_join(get_dav_endpint(), space_id, folder_name)
    response = request.mkcol(url)
    request.assert_http_status(
        response,
        201,
        f'Failed to create folder "{folder_name}" in space "{space_name}"',
    )


def create_space_file(space_name, file_name, content):
    space_id = get_space_id(space_name)
    url = url_join(get_dav_endpint(), space_id, file_name)
    response = request.put(url, content)
    if response.status_code not in (201, 204):
        raise AssertionError(
            f"Creating file '{file_name}' in space '{space_name}' failed with {response.status_code}\n"
            + response.text
        )


def add_user_to_space(user, space_name, role):
    role = role.lower()
    if not role in space_role:
        raise ValueError(f"Cannot set the role '{role}' to a space")

    space_id = get_space_id(space_name)
    url = get_share_endpint()
    body = parse.urlencode(
        {
            'space_ref': space_id,
            'shareType': 7,
            'shareWith': user,
            'role': role,
        }
    )
    headers = {'Content-Type': 'application/x-www-form-urlencoded'}
    response = request.post(url, body, headers)
    request.assert_http_status(
        response, 200, f'Failed to add user "{user}" to space "{space_name}"'
    )


def get_file_content(space_name, file_name, user=None):
    space_id = get_space_id(space_name, user)
    url = url_join(get_dav_endpint(), space_id, file_name)
    response = request.get(url=url, user=user)
    request.assert_http_status(response, 200, f'Failed to get file "{file_name}"')
    return response.text


def resource_exists(space_name, resource, user=None):
    space_id = get_space_id(space_name, user)
    url = url_join(get_dav_endpint(), space_id, resource)
    response = request.get(url=url, user=user)
    if response.status_code == 200:
        return True
    return False
