import json
from os import path
from types import MappingProxyType
from helpers.ConfigHelper import get_config
import helpers.api.HttpHelper as request
import helpers.api.Provisioning as provisioning


share_types = MappingProxyType(
    {"user": 0, "group": 1, "public_link": 3, "federated_cloud_share": 6}
)


def get_share_url():
    return provisioning.format_json(
        path.join(
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
    return path.join(get_config('localBackendUrl'), 'remote.php', 'dav', 'public-files')


def get_public_link_shares(user):
    public_shares_list = []
    response = request.get(get_share_url(), user=user)
    provisioning.checkSuccessOcsStatus(response)
    shares = json.loads(response.text)['ocs']['data']

    for share in shares:
        if share["share_type"] == share_types["public_link"]:
            public_shares_list.append(share)
    return public_shares_list


def has_public_link_share(user, resource_name):
    public_shares = get_public_link_shares(user)
    if public_shares:
        for share in public_shares:
            if share["file_target"].strip('\/') == resource_name:
                return True
    return False


def download_last_public_link_resource(user, resource, public_link_password=None):
    last_stime = 0
    share = False

    public_link_shares = get_public_link_shares(user)

    for share_link in public_link_shares:
        if last_stime < share_link["stime"]:
            share = share_link
            last_stime = share_link["stime"]
    if not share:
        raise Exception(
            f"Expected public link for {resource} shared by {user} could not be found"
        )

    api_url = provisioning.format_json(
        path.join(get_public_endpoint(), share["token"], resource)
    )

    response = request.get(api_url, user='public', password=public_link_password)

    if response.status_code == 200:
        return True
    elif response.status_code == 404:
        return False
    else:
        raise Exception(f"Server returned status code: {response.status_code}")
