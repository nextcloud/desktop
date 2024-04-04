# -*- coding: utf-8 -*-
import os
from os.path import isfile, join, isdir
import re
import builtins
import shutil
import zipfile

from pageObjects.AccountSetting import AccountSetting

from helpers.SetupClientHelper import getResourcePath, getTempResourcePath
from helpers.SyncHelper import waitForClientToBeReady
from helpers.ConfigHelper import get_config
from helpers.FilesHelper import (
    buildConflictedRegex,
    sanitizePath,
    can_read,
    can_write,
    read_file_content,
    is_empty_sync_folder,
    get_size_in_bytes,
    prefix_path_namespace,
)
from helpers.SetupClientHelper import (
    getTempResourcePath,
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
    os.makedirs(prefix_path_namespace(folder_path))


def renameFileFolder(source, destination):
    source = getResourcePath(source)
    destination = getResourcePath(destination)
    os.rename(source, destination)


def createFileWithSize(filename, filesize, isTempFolder=False):
    if isTempFolder:
        file = join(get_config('tempFolderPath'), filename)
    else:
        file = getResourcePath(filename)
    with open(prefix_path_namespace(file), "wb") as f:
        f.seek(get_size_in_bytes(filesize) - 1)
        f.write(b'\0')


def writeFile(resource, content):
    f = open(prefix_path_namespace(resource), "w")
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


def createZip(resources, zip_file_name, cwd=""):
    os.chdir(cwd)
    with zipfile.ZipFile(zip_file_name, 'w') as zippedFile:
        for resource in resources:
            zippedFile.write(resource)


def extractZip(zip_file_path, destination_dir):
    with zipfile.ZipFile(zip_file_path, 'r') as zipFile:
        zipFile.extractall(destination_dir)


def addCopySuffix(resource_path, resource_type):
    if resource_type == "file":
        source_dir = resource_path.rsplit('.', 1)
        return source_dir[0] + " - Copy." + source_dir[-1]
    return resource_path + " - Copy"


@When(
    'user "|any|" creates a file "|any|" with the following content inside the sync folder'
)
def step(context, username, filename):
    fileContent = "\n".join(context.multiLineText)
    file = getResourcePath(filename, username)
    waitAndWriteFile(file, fileContent)


@When('user "|any|" creates a folder "|any|" inside the sync folder')
def step(context, username, foldername):
    waitForClientToBeReady()
    createFolder(foldername, username)


@Given('user "|any|" has created a folder "|any|" inside the sync folder')
def step(context, username, foldername):
    createFolder(foldername, username)


@When('user "|any|" creates a file "|any|" with size "|any|" inside the sync folder')
def step(context, username, filename, filesize):
    createFileWithSize(filename, filesize)


@When(r'the user copies the (file|folder) "([^"]*)" to "([^"]*)"', regexp=True)
def step(context, resource_type, source_dir, destination_dir):
    source = getResourcePath(source_dir)
    destination = getResourcePath(destination_dir)
    if source == destination and destination_dir != '/':
        destination = addCopySuffix(source, resource_type)
    if resource_type == 'folder':
        return shutil.copytree(source, destination)
    else:
        return shutil.copy2(source, destination)


@When(r'the user renames a (?:file|folder) "([^"]*)" to "([^"]*)"', regexp=True)
def step(context, source, destination):
    waitForClientToBeReady()
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

    # if the sync folder is empty after deleting file,
    # a dialog will popup asking to confirm "Remove all files"
    if is_empty_sync_folder(getResourcePath()):
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


@Given('the user has created a folder "|any|" in temp folder')
def step(context, folderName):
    createFolder(folderName, isTempFolder=True)


@Given(
    'the user has created "|any|" files each of size "|any|" bytes inside folder "|any|" in temp folder'
)
def step(context, fileNumber, fileSize, folderName):
    currentSyncPath = getTempResourcePath(folderName)
    if folderExists(currentSyncPath):
        fileSize = builtins.int(fileSize)
        for i in range(0, builtins.int(fileNumber)):
            fileName = f"file{i}.txt"
            createFileWithSize(join(currentSyncPath, fileName), fileSize, True)
    else:
        raise Exception(f"Folder '{folderName}' does not exist in the temp folder")


@When(
    r'user "([^"]*)" moves (folder|file) "([^"]*)" from the temp folder into the sync folder',
    regexp=True,
)
def step(context, username, resource_type, resource_name):
    source_dir = join(get_config('tempFolderPath'), resource_name)
    destination_dir = getResourcePath('/', username)
    shutil.move(source_dir, destination_dir)


@When(
    r'user "([^"]*)" moves (folder|file) "([^"]*)" to the temp folder',
    regexp=True,
)
def step(context, username, resource_type, resource_name):
    source_dir = getResourcePath(resource_name)
    destination_dir = getTempResourcePath(resource_name)
    shutil.move(source_dir, destination_dir)


@When(
    r'user "([^"]*)" moves (?:file|folder) "([^"]*)" to "([^"]*)" in the sync folder',
    regexp=True,
)
def step(context, username, source, destination):
    waitForClientToBeReady()
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


@Given(
    'the user has created a zip file "|any|" with the following resources in the temp folder'
)
def step(context, zip_file_name):
    resource_list = []

    for row in context.table[1:]:
        resource_list.append(row[0])
        resource = join(get_config('tempFolderPath'), row[0])
        if row[1] == 'folder':
            os.makedirs(resource)
        elif row[1] == 'file':
            content = ""
            if len(row) > 2 and row[2]:
                content = row[2]
            writeFile(resource, content)
    createZip(resource_list, zip_file_name, get_config('tempFolderPath'))


@When('user "|any|" unzips the zip file "|any|" inside the sync root')
def step(context, username, zip_file_name):
    destination_dir = getResourcePath('/', username)
    zip_file_path = join(destination_dir, zip_file_name)
    extractZip(zip_file_path, destination_dir)


@When('user "|any|" copies file "|any|" to temp folder')
def step(context, username, source):
    waitForClientToBeReady()
    source_dir = getResourcePath(source, username)
    destination_dir = getTempResourcePath(source)
    shutil.copy2(source_dir, destination_dir)
