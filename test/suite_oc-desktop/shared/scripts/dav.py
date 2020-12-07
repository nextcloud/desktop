# -*- coding: utf-8 -*-\
import urllib.request
import base64
import requests
import xml.etree.ElementTree as ET
from urllib.parse import unquote

class Dav:
    def __init__(self, username, password, context):
        self.username = username
        self.password = password
        self.context = context
        
    def getBasicAuthHeader(self):
        header_encode = '{}:{}'.format(self.username, self.password).encode('ascii')
        authheader = base64.b64encode(header_encode)
        return 'Basic ' + authheader.decode('ascii')
    
    def getDavPath(self, path=""):
        return self.context.userData['localBackendUrl'] + '/remote.php/dav/files/' + self.username + '/' + path
        
    def downloadFile(self, fileName):
        res = requests.get(self.getDavPath(fileName), headers={'Authorization': self.getBasicAuthHeader()})
        if res.status_code != 200:
            error = self.parse_error_response(res.text)
            raise Exception(error['message'])
        return res.text

    def uploadFile(self, fileName, content):
        data = content.encode('utf8')
        res = requests.put(self.getDavPath(fileName), data=data, headers={'Authorization': self.getBasicAuthHeader()})
        if not (res.status_code == 201 or res.status_code == 204):
            error = self.parse_error_response(res.text)
            raise Exception(error['message'])

    def listFiles(self, basePath='/'):
        res = self.propfind(basePath)
        tree = ET.fromstring(res.text)
        files = []
        for child in tree:
            if child.tag == '{DAV:}response':
                href = child[0].text
                filename = href.split('dav/files/')[1].split('/')[1:]
                filename = "/".join(filename)
                files.append(unquote('/' + filename))
        return files

    def move(self, source, destination):
        res = requests.request(
                'MOVE',
                self.getDavPath(source),
                headers={'Authorization': self.getBasicAuthHeader(), 'Destination': self.getDavPath(destination)},
            )

        if res.status_code != 201:
            error = self.parse_error_response(res.text)
            raise Exception(error['message'])

    def copy(self, source, destination):
        res = requests.request(
                'COPY',
                self.getDavPath(source),
                headers={'Authorization': self.getBasicAuthHeader(), 'Destination': self.getDavPath(destination)},
            )

        if res.status_code != 201:
            error = self.parse_error_response(res.text)
            raise Exception(error['message'])

    def delete(self, filepath):
        res = requests.delete(self.getDavPath(filepath), headers={'Authorization': self.getBasicAuthHeader()})
        if res.status_code != 204:
            raise Exception("failed to delete")

    def makeDir(self, dirPath):
        res = requests.request(
                'MKCOL',
                self.getDavPath(dirPath),
                headers={'Authorization': self.getBasicAuthHeader()},
            )
        if res.status_code != 201:
            error = self.parse_error_response(res.text)
            raise Exception(error['message'])

    def propfind(self, path, properties=None):
        url = self.getDavPath(path)
        res = requests.request('PROPFIND', url, data=self.get_propfind_body(properties),  headers={'Authorization': self.getBasicAuthHeader()})
        return res

    def get_propfind_body(self, properties=None):
        if not properties:
          properties=''
        body = '<?xml version="1.0" encoding="utf-8" ?>'
        body += '<D:propfind xmlns:D="DAV:">'
        if properties:
            body += '<D:prop>'
            for prop in properties:
                body += '<D:' + prop + '/>'
            body += '</D:prop>'
        else:
            body += '<D:allprop/>'
        body += '</D:propfind>'
        return body

    def parse_error_response(self, response):
        tree = ET.fromstring(response)
        error = {}
        for child in tree:
            if 'exception' in child.tag:
                error['exception'] = child.text
            if 'message' in child.tag:
                error['message'] = child.text
        if 'message' not in error:
            error['message'] = "Error performing the action"
        return error

    def checkFile(self, path):
        res = self.propfind(path)
        if 200 <= res.status_code < 300:
            return True
        else:
            return False
