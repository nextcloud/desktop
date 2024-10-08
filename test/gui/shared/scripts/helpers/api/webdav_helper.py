from urllib.parse import quote
import xml.etree.ElementTree as ET

import helpers.api.http_helper as request
from helpers.api.utils import url_join
from helpers.ConfigHelper import get_config
from helpers.FilesHelper import get_file_for_upload


def get_webdav_url():
    return url_join(get_config('localBackendUrl'), 'remote.php/dav/files')


def get_resource_path(user, resource):
    resource = resource.strip('/').replace('\\', '/')
    encoded_resource_path = [quote(path, safe='') for path in resource.split('/')]
    encoded_resource_path = '/'.join(encoded_resource_path)
    url = url_join(get_webdav_url(), user, encoded_resource_path)
    return url


def resource_exists(user, resource):
    response = request.propfind(get_resource_path(user, resource), user=user)
    if response.status_code == 207:
        return True
    if response.status_code == 404:
        return False
    raise AssertionError(f'Server returned status code: {response.status_code}')


def get_file_content(user, resource):
    response = request.get(get_resource_path(user, resource), user=user)
    return response.text


def get_folder_items_count(user, folder_name):
    folder_name = folder_name.strip('/')
    path = get_resource_path(user, folder_name)
    xml_response = request.propfind(path, user=user)
    total_items = 0
    root_element = ET.fromstring(xml_response.content)
    for response_element in root_element:
        for href_element in response_element:
            # The first item is folder itself so excluding it
            if href_element.tag == '{DAV:}href' and not href_element.text.endswith(
                f'{user}/{folder_name}/'
            ):
                total_items += 1
    return str(total_items)


def create_folder(user, folder_name):
    url = get_resource_path(user, folder_name)
    response = request.mkcol(url, user=user)
    assert (
        response.status_code == 201
    ), f'Could not create the folder: {folder_name} for user {user}'


def create_file(user, file_name, contents):
    url = get_resource_path(user, file_name)
    response = request.put(url, body=contents, user=user)
    assert response.status_code in [
        201,
        204,
    ], f"Could not create file '{file_name}' for user {user}"


def upload_file(user, file_name, destination):
    file_path = get_file_for_upload(file_name)
    with open(file_path, 'rb') as file:
        contents = file.read()
    create_file(user, destination, contents)


def delete_resource(user, resource):
    url = get_resource_path(user, resource)
    response = request.delete(url, user=user)
    assert response.status_code == 204, f"Could not delete folder '{resource}'"
