# -*- coding: utf-8 -*-
import os
from os.path import isfile, join, isdir
import re
import builtins
import shutil

from pageObjects.AccountSetting import AccountSetting

from helpers.SetupClientHelper import getResourcePath
from helpers.SyncHelper import waitForClientToBeReady
from helpers.ConfigHelper import get_config
from helpers.FilesHelper import (
    buildConflictedRegex,
    sanitizePath,
    can_read,
    can_write,
    read_file_content,
)


def folderExists(folderPath, timeout=1000):
    return waitFor(
        lambda: isdir(sanitizePath(folderPath)),
        timeout,
    )


def fileExists(filePath, timeout=1000):
    return waitFor(
        lambda: isfile(sanitizePath(filePath)),
        timeout,
    )


# To create folders in a temporary directory, we set isTempFolder True
# And if isTempFolder is True, the createFolder function create folders in tempFolderPath
def createFolder(foldername, username=None, isTempFolder=False):
    if isTempFolder:
        folder_path = join(get_config('tempFolderPath'), foldername)
    else:
        folder_path = getResourcePath(foldername, username)
    os.makedirs(folder_path)


def renameFileFolder(source, destination):
    source = getResourcePath(source)
    destination = getResourcePath(destination)
    os.rename(source, destination)


def createFileWithSize(filename, filesize, isTempFolder=False):
    if isTempFolder:
        file = join(get_config('tempFolderPath'), filename)
    else:
        file = getResourcePath(filename)
    cmd = "truncate -s {filesize} {file}".format(filesize=filesize, file=file)
    os.system(cmd)


def writeFile(resource, content):
    f = open(resource, "w")
    f.write(content)
    f.close()


def waitAndWriteFile(path, content):
    waitForClientToBeReady()
    writeFile(path, content)


def waitAndTryToWriteFile(resource, content):
    waitForClientToBeReady()
    try:
        writeFile(resource, content)
    except:
        pass


@When(
    'user "|any|" creates a file "|any|" with the following content inside the sync folder'
)
def step(context, username, filename):
    fileContent = "\n".join(context.multiLineText)
    file = getResourcePath(filename, username)
    waitAndWriteFile(file, fileContent)


@When('user "|any|" creates a folder "|any|" inside the sync folder')
def step(context, username, foldername):
    createFolder(foldername, username)


@Given('user "|any|" has created a folder "|any|" inside the sync folder')
def step(context, username, foldername):
    createFolder(foldername, username)


@When('user "|any|" creates a file "|any|" with size "|any|" inside the sync folder')
def step(context, username, filename, filesize):
    createFileWithSize(filename, filesize)


@When('the user copies the folder "|any|" to "|any|"')
def step(context, sourceFolder, destinationFolder):
    source_dir = getResourcePath(sourceFolder)
    destination_dir = getResourcePath(destinationFolder)
    shutil.copytree(source_dir, destination_dir)


@When(r'the user renames a (file|folder) "([^"]*)" to "([^"]*)"', regexp=True)
def step(context, type, source, destination):
    renameFileFolder(source, destination)


@Then('the file "|any|" should exist on the file system with the following content')
def step(context, filePath):
    expected = "\n".join(context.multiLineText)
    filePath = getResourcePath(filePath)
    f = open(filePath, 'r')
    contents = f.read()
    test.compare(
        expected,
        contents,
        "file expected to exist with content "
        + expected
        + " but does not have the expected content",
    )


@Then(r'^the (file|folder) "([^"]*)" should exist on the file system$', regexp=True)
def step(context, resourceType, resource):
    resourcePath = getResourcePath(resource)
    print(resourcePath)
    resourceExists = False
    if resourceType == 'file':
        resourceExists = fileExists(resourcePath, get_config('maxSyncTimeout') * 1000)
    elif resourceType == 'folder':
        resourceExists = folderExists(resourcePath, get_config('maxSyncTimeout') * 1000)
    else:
        raise Exception("Unsupported resource type '" + resourceType + "'")

    test.compare(
        True,
        resourceExists,
        "Assert " + resourceType + " '" + resource + "' exists on the system",
    )


@Then(r'^the (file|folder) "([^"]*)" should not exist on the file system$', regexp=True)
def step(context, resourceType, resource):
    resourcePath = getResourcePath(resource)
    resourceExists = False
    if resourceType == 'file':
        resourceExists = fileExists(resourcePath, 1000)
    elif resourceType == 'folder':
        resourceExists = folderExists(resourcePath, 1000)
    else:
        raise Exception("Unsupported resource type '" + resourceType + "'")

    test.compare(
        False,
        resourceExists,
        "Assert " + resourceType + " '" + resource + "' doesn't exist on the system",
    )


@Given('the user has changed the content of local file "|any|" to:')
def step(context, filename):
    fileContent = "\n".join(context.multiLineText)
    waitAndWriteFile(getResourcePath(filename), fileContent)


@Then(
    'a conflict file for "|any|" should exist on the file system with the following content'
)
def step(context, filename):
    expected = "\n".join(context.multiLineText)

    onlyfiles = [f for f in os.listdir(getResourcePath()) if isfile(getResourcePath(f))]
    found = False
    pattern = re.compile(buildConflictedRegex(filename))
    for file in onlyfiles:
        if pattern.match(file):
            f = open(getResourcePath(file), 'r')
            contents = f.read()
            if contents == expected:
                found = True
                break

    if not found:
        raise Exception("Conflict file not found with given name")


@When('the user overwrites the file "|any|" with content "|any|"')
def step(context, resource, content):
    print("starting file overwrite")
    resource = getResourcePath(resource)
    waitAndWriteFile(resource, content)
    print("file has been overwritten")


@When('the user tries to overwrite the file "|any|" with content "|any|"')
def step(context, resource, content):
    resource = getResourcePath(resource)
    waitAndTryToWriteFile(resource, content)


@When('user "|any|" tries to overwrite the file "|any|" with content "|any|"')
def step(context, user, resource, content):
    resource = getResourcePath(resource, user)
    waitAndTryToWriteFile(resource, content)


@When(r'the user deletes the (file|folder) "([^"]*)"', regexp=True)
def step(context, itemType, resource):
    waitForClientToBeReady()

    resourcePath = sanitizePath(getResourcePath(resource))
    if itemType == 'file':
        os.remove(resourcePath)
    elif itemType == 'folder':
        shutil.rmtree(resourcePath)
    else:
        raise Exception("No such item type for resource")

    isSyncFolderEmpty = True
    for item in os.listdir(getResourcePath()):
        # do not count the hidden files as they are ignored by the client
        if not item.startswith("."):
            isSyncFolderEmpty = False
            break

    # if the sync folder is empty after deleting file,
    # a dialog will popup asking to confirm "Remove all files"
    if isSyncFolderEmpty:
        try:
            AccountSetting.confirmRemoveAllFiles()
        except:
            pass


@When('user "|any|" creates the following files inside the sync folder:')
def step(context, username):
    waitForClientToBeReady()

    for row in context.table[1:]:
        file = getResourcePath(row[0], username)
        writeFile(file, '')


@Given(
    'the user has created a folder "|any|" with "|any|" files each of size "|any|" bytes in temp folder'
)
def step(context, foldername, filenumber, filesize):
    createFolder(foldername, isTempFolder=True)
    filesize = builtins.int(filesize)
    for i in range(0, builtins.int(filenumber)):
        filename = f"file{i}.txt"
        createFileWithSize(join(foldername, filename), filesize, True)


@When('user "|any|" moves folder "|any|" from the temp folder into the sync folder')
def step(context, username, foldername):
    source_dir = join(get_config('tempFolderPath'), foldername)
    destination_dir = getResourcePath('/', username)
    shutil.move(source_dir, destination_dir)


@When(
    r'user "([^"]*)" moves (file|folder) "([^"]*)" to "([^"]*)" in the sync folder',
    regexp=True,
)
def step(context, username, resourceType, source, destination):
    source_dir = getResourcePath(source, username)
    if destination == None or destination == "/":
        destination = ""
    destination_dir = getResourcePath(destination, username)
    shutil.move(source_dir, destination_dir)


@Then('user "|any|" should be able to open the file "|any|" on the file system')
def step(context, user, file_name):
    file_path = getResourcePath(file_name, user)
    test.compare(can_read(file_path), True, "File should be readable")


@Then('as "|any|" the file "|any|" should have content "|any|" on the file system')
def step(context, user, file_name, content):
    file_path = getResourcePath(file_name, user)
    file_content = read_file_content(file_path)
    test.compare(file_content, content, "Comparing file content")


@Then('user "|any|" should not be able to edit the file "|any|" on the file system')
def step(context, user, file_name):
    file_path = getResourcePath(file_name, user)
    test.compare(not can_write(file_path), True, "File should not be writable")
