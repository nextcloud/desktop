import requests
from helpers.UserHelper import basic_auth_header

requests.packages.urllib3.disable_warnings()


def send_request(url, method, body=None, headers={}, user=None):
    auth_header = basic_auth_header(user)
    headers.update(auth_header)
    return requests.request(method, url, data=body, headers=headers, verify=False)


def get(url, headers={}, user=None):
    return send_request(url=url, method="GET", headers=headers, user=user)


def post(url, body=None, headers={}, user=None):
    return send_request(url, "POST", body, headers, user)


def put(url, body=None, headers={}, user=None):
    return send_request(url, "PUT", body, headers, user)


def delete(url, headers={}, user=None):
    return send_request(url=url, method="DELETE", headers=headers, user=user)


def mkcol(url, headers={}, user=None):
    return send_request(url=url, method="MKCOL", headers=headers, user=user)


def propfind(url, body=None, headers={}, user=None):
    return send_request(url, "PROPFIND", body, headers, user)
