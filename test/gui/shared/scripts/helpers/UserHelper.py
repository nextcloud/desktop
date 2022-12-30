import os
import requests

createdUsers = {}

# gets all users information created in a test scenario
def getCreatedUsersFromMiddleware(context):
    createdUsers = {}
    try:
        res = requests.get(
            os.path.join(context.userData['middlewareUrl'], 'state'),
            headers={"Content-Type": "application/json"},
        )
        createdUsers = res.json()['created_users']
    except ValueError:
        raise Exception("Could not get created users information from middleware")

    return createdUsers


def getUserInfo(context, username, attribute):
    # add and update users to the global createdUsers dict if not already there
    # so that we don't have to request for user information in every scenario
    # but instead get user information from the global dict
    global createdUsers
    if username in createdUsers:
        return createdUsers[username][attribute]
    else:
        createdUsers = {**createdUsers, **getCreatedUsersFromMiddleware(context)}
        return createdUsers[username][attribute]


def getDisplaynameForUser(context, username):
    return getUserInfo(context, username, 'displayname')


def getPasswordForUser(context, username):
    return getUserInfo(context, username, 'password')
