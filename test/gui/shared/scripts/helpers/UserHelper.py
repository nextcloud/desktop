from base64 import b64encode
from typing import NamedTuple


class User(NamedTuple):
    username: str
    password: str
    displayname: str
    email: str


test_users = {
    "admin": User(
        username="admin",
        password="admin",
        displayname="adminUsername",
        email="admin@example.org",
    ),
    "Alice": User(
        username="Alice",
        password="1234",
        displayname="Alice Hansen",
        email="alice@example.org",
    ),
    "Brian": User(
        username="Brian",
        password="AaBb2Cc3Dd4",
        displayname="Brian Murphy",
        email="brian@example.org",
    ),
    "Carol": User(
        username="Carol",
        password="1234",
        displayname="Carol King",
        email="carol@example.org",
    ),
    "David": User(
        username="David",
        password="1234",
        displayname="David Lopez",
        email="david@example.org",
    ),
}


def get_default_password():
    return "1234"


def basic_auth_header(user=None, password=None):
    if not user and not password:
        user = "admin"
        password = "admin"
    elif not user == "public" and not password:
        password = get_password_for_user(user)

    token = b64encode((f"{user}:{password}").encode()).decode()
    return {"Authorization": "Basic " + token}


def get_user_info(username, attribute):
    if username in test_users:
        return getattr(test_users[username], attribute)
    if attribute == "password":
        return get_default_password()
    raise ValueError(f"Invalid user attribute: {attribute}")


def get_displayname_for_user(username):
    return get_user_info(username, "displayname")


def get_password_for_user(username):
    return get_user_info(username, "password")
