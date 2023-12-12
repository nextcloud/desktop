import json

import helpers.api.HttpHelper as request
from helpers.api.utils import url_join
from helpers.ConfigHelper import get_config


def get_ocs_url():
    return url_join(get_config('localBackendUrl'), 'ocs', "v2.php", 'cloud')


def format_json(url):
    return url + "?format=json"


def assertHttpStatus(response, expected_code):
    assert (
        response.status_code == expected_code
    ), f"Request failed with status code '{response.status_code}'"


def checkSuccessOcsStatus(response):
    if response.text:
        ocs_code = json.loads(response.text)['ocs']['meta']['statuscode']
        if ocs_code not in [100, 200]:
            raise Exception("Request failed." + response.text)
    else:
        raise Exception(
            "No OCS response body. HTTP status was " + str(response.status_code)
        )


def enable_app(app_name):
    url = format_json(url_join(get_ocs_url(), "apps", app_name))
    response = request.post(url)
    assertHttpStatus(response, 200)
    checkSuccessOcsStatus(response)


def disable_app(app_name):
    url = format_json(url_join(get_ocs_url(), "apps", app_name))
    response = request.delete(url)
    assertHttpStatus(response, 200)
    checkSuccessOcsStatus(response)


def setup_app(app_name, action):
    if action.startswith('enable'):
        enable_app(app_name)
    elif action.startswith('disable'):
        disable_app(app_name)
    else:
        raise Exception("Unknown action: " + action)
