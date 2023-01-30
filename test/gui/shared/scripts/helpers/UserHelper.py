import os
import requests
from helpers.ConfigHelper import get_config

createdUsers = {}

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
