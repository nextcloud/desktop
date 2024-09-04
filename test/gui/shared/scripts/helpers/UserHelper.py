from base64 import b64encode

test_users = {
    "admin": {
        "username": "admin",
        "password": "admin",
        "displayname": "adminUsername",
    },
    "Alice": {
        "username": "Alice",
        "password": "1234",
        "displayname": "Alice Hansen",
        "email": "alice@example.org",
    },
    "Brian": {
        "username": "Brian",
        "password": "AaBb2Cc3Dd4",
        "displayname": "Brian Murphy",
        "email": "brian@example.org",
    },
    "Carol": {
        "username": "Carol",
        "password": "1234",
        "displayname": "Carol King",
        "email": "carol@example.org",
    },
    "David": {
        "username": "David",
        "password": "1234",
        "displayname": "David Lopez",
        "email": "david@example.org",
    },
    "regularuser": {
        "password": "1234",
    },
}


def basic_auth_header(user=None, password=None):
    if not user and not password:
        user = 'admin'
        password = 'admin'
    elif not user == 'public' and not password:
        password = getPasswordForUser(user)

    token = b64encode(("%s:%s" % (user, password)).encode()).decode()
    return {"Authorization": "Basic " + token}


def getUserInfo(username, attribute):
    if username in test_users:
        return test_users[username][attribute]
    else:
        return test_users["regularuser"][attribute]


def getDisplaynameForUser(username):
    return getUserInfo(username, 'displayname')


def getPasswordForUser(username):
    return getUserInfo(username, 'password')
