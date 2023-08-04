import urllib.parse
import helpers.api.HttpHelper as request
from helpers.ConfigHelper import get_config
from os import path
import xml.etree.ElementTree as ET


def get_webdav_url():
    return path.join(get_config('localBackendUrl'), "remote.php", "dav", 'files')


def get_resource_path(user, resource):
    resource = resource.strip('/')
    encoded_resource_path = [
        urllib.parse.quote(path, safe='') for path in resource.split('/')
    ]
    encoded_resource_path = '/'.join(encoded_resource_path)
    url = path.join(get_webdav_url(), user, encoded_resource_path)
    return url


def resource_exists(user, resource):
    response = request.propfind(get_resource_path(user, resource), user=user)
    if response.status_code == 207:
        return True
    elif response.status_code == 404:
        return False
    else:
        raise Exception(f"Server returned status code: {response.status_code}")


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
    ), f"Could not create the folder: {folder_name} for user {user}"


def create_file(user, file_name, contents):
    url = get_resource_path(user, file_name)
    response = request.put(url, body=contents, user=user)
    assert (
        response.status_code == 201
    ), f"Could not create file '{file_name}' for user {user}"
