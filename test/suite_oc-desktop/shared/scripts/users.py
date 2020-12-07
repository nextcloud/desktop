# -*- coding: utf-8 -*-

import ocs
import time
import dav

defaultUsers = {
    'admin': {
        'userid': 'admin',
        'password': 'admin',
    },
    'Alice': {
        'userid': 'Alice',
        'password': '1234',
        'email': 'alice@owncloud.com',
        'displayName': 'Alice Hansen'
    },
    'Brian': {
        'userid': 'Brian',
        'password': '123456',
        'email': 'brian@owncloud.com',
        'displayName': 'Brian Murphy'
    }
}

class Users:
    createdUsers = {}
    context = None

    def __init__(self, context):
        self.context = context
        self.ocsAdmin = ocs.Ocs(self.context.userData['localBackendUrl'], 'admin', 'admin')

    @classmethod
    def getPasswordForUser(cls, user):
        if user in cls.createdUsers:
            return cls.createdUsers[user]['password']
        elif user in defaultUsers:
            return defaultUsers[user]['password']
        else:
            raise Exception("user {} not found, the user may not have been created yet".format(user))     
        
    def createDefaultUser(self, username):
        if username not in defaultUsers:
            raise Exception("Default user {} doesn't exist".format(username))
        user = defaultUsers[username]
        self.createusers(user['userid'], user['password'], user['email'], user['displayName'])
        ocDav = dav.Dav(username, self.getPasswordForUser(username), self.context)
        ocDav.listFiles()

    def createusers(self, userName, password, email="", displayName=""):
        try:
            self.deleteUser(userName)
        except:
            pass

        body = {'userid': userName, 'password': password, 'email': email, 'displayname': displayName}

        res = self.ocsAdmin.post('cloud/users', body)
        if res.getcode() != 200:
            raise Exception('Failed when creating user, expected status 201 found {}'.format(res.getcode()))

        self.createdUsers[userName] = body
        
        # the user's skeleton files/folders will be locked for a while
        time.sleep(5)

    def deleteUser(self, userName):
        res = self.ocsAdmin.delete('cloud/users/{}'.format(userName))
        if res.getcode() != 200:
            raise Exception('Failed when deleting user')

    def deleteAllUsers(self):
        for user in self.createdUsers.keys():
            self.deleteUser(user)
