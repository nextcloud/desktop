import os
import requests
from base64 import b64encode
from helpers.ConfigHelper import get_config

createdUsers = {}


def basic_auth_header(user=None):
    # default admin auth
    token = b64encode(b"admin:admin").decode()
    if user:
        password = getPasswordForUser(user)
        token = b64encode(("%s:%s" % (user, password)).encode()).decode()
    return {"Authorization": "Basic " + token}


# gets all users information created in a test scenario
def getCreatedUsersFromMiddleware():
    createdUsers = {}
    try:
        res = requests.get(
            os.path.join(get_config('middlewareUrl'), 'state'),
            headers={"Content-Type": "application/json"},
        )
        createdUsers = res.json()['created_users']
    except ValueError:
        raise Exception("Could not get created users information from middleware")

    return createdUsers


def getUserInfo(username, attribute):
    # add and update users to the global createdUsers dict if not already there
    # so that we don't have to request for user information in every scenario
    # but instead get user information from the global dict
    global createdUsers
    if username in createdUsers:
        return createdUsers[username][attribute]
    else:
        createdUsers = {**createdUsers, **getCreatedUsersFromMiddleware()}
        return createdUsers[username][attribute]


def getDisplaynameForUser(username):
    return getUserInfo(username, 'displayname')


def getPasswordForUser(username):
    return getUserInfo(username, 'password')
