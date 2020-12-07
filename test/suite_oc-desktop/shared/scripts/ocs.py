# -*- coding: utf-8 -*-
import os
import urllib.request
import base64

class Ocs:
    def __init__(self, base_url, username, password, version='v2.php'):
        self.base_url = base_url
        self.version = version
        self.username = username
        self.password = password
        
    def getFullUrl(self, path):
        return os.path.join(self.base_url, 'ocs', self.version, path)
    
    def getAuthHeader(self):
        header_encode = '{}:{}'.format(self.username, self.password).encode('ascii')
        authheader = base64.b64encode(header_encode)
        return {'Authorization': 'Basic ' + authheader.decode('ascii')}
        
    def sendRequest(self, req):
        return urllib.request.urlopen(req)
        
    def get(self, path, header):
        req = urllib.request.Request(
            self.getFullUrl(path), headers=self.getAuthHeader(), method='GET'
        )
        res = self.sendRequest(req)
        return res
        
    def post(self, path, body, header={}, user=""):
        params = urllib.parse.urlencode(body)
        params = params.encode('utf-8')
        
        req = urllib.request.Request(
            self.getFullUrl(path), data=params, headers=self.getAuthHeader(), method='POST'
        )
        res = self.sendRequest(req)
        return res
        
    def delete(self, path, header={}, user=""):
        req = urllib.request.Request(
            self.getFullUrl(path), headers=self.getAuthHeader(), method='DELETE'
        )
        res = self.sendRequest(req)
        return res