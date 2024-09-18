from helpers.api import oc10 as oc
from helpers.api import ocis
from helpers.ConfigHelper import get_config
from helpers import UserHelper

created_groups = {}
created_users = {}


def create_group(group_name):
    if get_config('ocis'):
        group_info = ocis.create_group(group_name)
    else:
        group_info = oc.create_group(group_name)
    created_groups[group_name] = group_info


def delete_created_groups():
    for group_name, group_info in list(created_groups.items()):
        if get_config('ocis'):
            ocis.delete_group(group_info['id'])
        else:
            oc.delete_group(group_info['id'])
        del created_groups[group_name]


def add_user_to_group(user, group_name):
    if get_config('ocis'):
        ocis.add_user_to_group(user, group_name)
    else:
        oc.add_user_to_group(user, group_name)


def create_user(username):
    if username in UserHelper.test_users:
        user = UserHelper.test_users[username]
    else:
        user = UserHelper.User(
            username=username,
            displayname=username,
            email=f'{username}@mail.com',
            password=UserHelper.get_default_password(),
        )

    if get_config('ocis'):
        user_info = ocis.create_user(
            user.username,
            user.password,
            user.displayname,
            user.email,
        )
    else:
        user_info = oc.create_user(
            user.username,
            user.password,
            user.displayname,
            user.email,
        )
    created_users[username] = user_info


def delete_created_users():
    for username, user_info in list(created_users.items()):
        if get_config('ocis'):
            ocis.delete_user(user_info['id'])
        else:
            oc.delete_user(user_info['id'])
        del created_users[username]
