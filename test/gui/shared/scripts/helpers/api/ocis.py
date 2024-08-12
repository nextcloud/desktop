import json

import helpers.api.HttpHelper as request
from helpers.api.utils import url_join
from helpers.ConfigHelper import get_config


def get_graph_url():
    return url_join(get_config('localBackendUrl'), 'graph', 'v1.0')


def create_group(group_name):
    url = url_join(get_graph_url(), 'groups')
    body = json.dumps({"displayName": group_name})
    response = request.post(url, body)
    request.assertHttpStatus(response, 201)
    resp_object = response.json()
    return {
        "id": resp_object['id'],
        "displayName": resp_object['displayName'],
    }


def delete_group(group_id):
    url = url_join(get_ocis_url(), 'groups', group_id)
    response = request.delete(url)
    request.assertHttpStatus(response, 200)
