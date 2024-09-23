import json
from types import MappingProxyType

from helpers.ConfigHelper import get_config
from helpers.api.utils import url_join
import helpers.api.http_helper as request
import helpers.api.oc10 as oc
from helpers.api.oc10 import checkSuccessOcsStatus


share_types = MappingProxyType(
    {'user': 0, 'group': 1, 'public_link': 3, 'federated_cloud_share': 6}
)

PERMISSIONS = MappingProxyType(
    {'read': 1, 'update': 2, 'create': 4, 'delete': 8, 'share': 16}
)


def get_permission_value(permissions):
    permission_list = [perm.strip() for perm in permissions.split(',')]
    combinedPermission = 0
    if 'all' in permission_list:
        return sum(PERMISSIONS.values())

    for permission in permission_list:
        if (permission_value := PERMISSIONS.get(permission)) is None:
            raise ValueError(f"Permission '{permission}' is not valid.")
        combinedPermission += permission_value

    return combinedPermission


def get_share_url():
    return oc.format_json(
        url_join(
            get_config('localBackendUrl'),
            'ocs',
            'v2.php',
            'apps',
            'files_sharing',
            'api',
            'v1',
            'shares',
        )
    )


def get_public_endpoint():
    return url_join(get_config('localBackendUrl'), 'remote.php', 'dav', 'public-files')


def get_public_link_shares(user):
    public_shares_list = []
    response = request.get(get_share_url(), user=user)
    checkSuccessOcsStatus(response)
    shares = json.loads(response.text)['ocs']['data']

    for share in shares:
        if share['share_type'] == share_types['public_link']:
            public_shares_list.append(share)
    return public_shares_list


def has_public_link_share(user, resource_name):
    if public_shares := get_public_link_shares(user):
        for share in public_shares:
            if share['file_target'].strip('\\/') == resource_name:
                return True
    return False


def download_last_public_link_resource(user, resource, public_link_password=None):
    last_stime = 0
    share = False

    public_link_shares = get_public_link_shares(user)

    for share_link in public_link_shares:
        if last_stime < share_link['stime']:
            share = share_link
            last_stime = share_link['stime']
    if not share:
        raise LookupError(
            f'Expected public link for {resource} shared by {user} could not be found'
        )

    api_url = oc.format_json(url_join(get_public_endpoint(), share['token'], resource))

    response = request.get(api_url, user='public', password=public_link_password)

    if response.status_code == 200:
        return True
    if response.status_code == 404:
        return False
    raise AssertionError(f'Server returned status code: {response.status_code}')


def share_resource(user, resource, receiver, permissions, receiver_type):
    permissions = get_permission_value(permissions)
    url = get_share_url()
    body = {
        'path': resource,
        'shareType': share_types[receiver_type],
        'shareWith': receiver,
        'permissions': permissions,
    }
    response = request.post(url, body, user=user)
    request.assertHttpStatus(
        response,
        200,
        f"Failed to share resource '{resource}' to {receiver_type} '{receiver}'",
    )
    checkSuccessOcsStatus(response)


def create_link_share(
    user, resource, permissions, name=None, password=None, expire_date=None
):
    url = get_share_url()
    permissions = get_permission_value(permissions)
    body = {
        'path': resource,
        'permissions': permissions,
        'shareType': share_types['public_link'],
    }
    if name is not None:
        body['name'] = name
    if password is not None:
        body['password'] = password
    if expire_date is not None:
        body['expireDate'] = expire_date
    response = request.post(url, body, user=user)
    request.assertHttpStatus(
        response, 200, f"Failed to create public link for resource '{resource}'"
    )
    checkSuccessOcsStatus(response)
