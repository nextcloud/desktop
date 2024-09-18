import requests
import urllib3

from helpers.UserHelper import basic_auth_header
from helpers.ConfigHelper import get_config

urllib3.disable_warnings()


def send_request(url, method, body=None, headers=None, user=None, password=None):
    auth_header = basic_auth_header(user, password)
    if not headers:
        headers = {}
    headers.update(auth_header)
    return requests.request(
        method,
        url,
        data=body,
        headers=headers,
        verify=False,
        timeout=get_config("maxSyncTimeout"),
    )


def get(url, headers=None, user=None, password=None):
    return send_request(
        url=url, method="GET", headers=headers, user=user, password=password
    )


def post(url, body=None, headers=None, user=None):
    return send_request(url, "POST", body, headers, user)


def put(url, body=None, headers=None, user=None):
    return send_request(url, "PUT", body, headers, user)


def delete(url, headers=None, user=None):
    return send_request(url=url, method="DELETE", headers=headers, user=user)


def mkcol(url, headers=None, user=None):
    return send_request(url=url, method="MKCOL", headers=headers, user=user)


def propfind(url, body=None, headers=None, user=None, password=None):
    return send_request(url, "PROPFIND", body, headers, user, password)


def assertHttpStatus(response, expected_code, message=""):
    response_body = ""
    if response.text:
        response_body = response.text

    assert (
        response.status_code == expected_code
    ), f"{message}\nRequest failed with status code '{response.status_code}'\n{response_body}"
