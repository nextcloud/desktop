# -*- coding: utf-8 -*-
import os
import re
import builtins
import shutil
import zipfile
from os.path import isfile, join, isdir
import squish

from helpers.SetupClientHelper import get_resource_path, get_temp_resource_path
from helpers.SyncHelper import wait_for_client_to_be_ready
from helpers.ConfigHelper import get_config
from helpers.FilesHelper import (
    build_conflicted_regex,
    sanitize_path,
    can_read,
    can_write,
    read_file_content,
    get_size_in_bytes,
    prefix_path_namespace,
    remember_path,
)


def folder_exists(folder_path, timeout=1000):
    return squish.waitFor(
        lambda: isdir(sanitize_path(folder_path)),
        timeout,
    )


def file_exists(file_path, timeout=1000):
    return squish.waitFor(
        lambda: isfile(sanitize_path(file_path)),
        timeout,
    )


# To create folders in a temporary directory, we set is_temp_folder True
# And if is_temp_folder is True, the create_folder function create folders in tempFolderPath
def create_folder(foldername, username=None, is_temp_folder=False):
    if is_temp_folder:
        folder_path = join(get_config('tempFolderPath'), foldername)
    else:
        folder_path = get_resource_path(foldername, username)
    os.makedirs(prefix_path_namespace(folder_path))


def rename_file_folder(source, destination):
    source = get_resource_path(source)
    destination = get_resource_path(destination)
    os.rename(source, destination)


def create_file_with_size(filename, filesize, is_temp_folder=False):
    if is_temp_folder:
        file = join(get_config('tempFolderPath'), filename)
    else:
        file = get_resource_path(filename)
    with open(prefix_path_namespace(file), 'wb') as f:
        f.seek(get_size_in_bytes(filesize) - 1)
        f.write(b'\0')


def write_file(resource, content):
    with open(prefix_path_namespace(resource), 'w', encoding='utf-8') as f:
        f.write(content)


def wait_and_write_file(path, content):
    wait_for_client_to_be_ready()
    write_file(path, content)


def wait_and_try_to_write_file(resource, content):
    wait_for_client_to_be_ready()
    try:
        write_file(resource, content)
    except:
        pass


def create_zip(resources, zip_file_name, cwd=''):
    os.chdir(cwd)
    with zipfile.ZipFile(zip_file_name, 'w') as zipped_file:
        for resource in resources:
            zipped_file.write(resource)


def extract_zip(zip_file_path, destination_dir):
    with zipfile.ZipFile(zip_file_path, 'r') as zip_file:
        zip_file.extractall(destination_dir)


def add_copy_suffix(resource_path, resource_type):
    if resource_type == 'file':
        source_dir = resource_path.rsplit('.', 1)
        return source_dir[0] + ' - Copy.' + source_dir[-1]
    return resource_path + ' - Copy'


@When(
    'user "|any|" creates a file "|any|" with the following content inside the sync folder'
)
def step(context, username, filename):
    file_content = '\n'.join(context.multiLineText)
    file = get_resource_path(filename, username)
    wait_and_write_file(file, file_content)


@When('user "|any|" creates a folder "|any|" inside the sync folder')
def step(context, username, foldername):
    wait_for_client_to_be_ready()
    create_folder(foldername, username)


@Given('user "|any|" has created a folder "|any|" inside the sync folder')
def step(context, username, foldername):
    create_folder(foldername, username)


@When('user "|any|" creates a file "|any|" with size "|any|" inside the sync folder')
def step(context, _, filename, filesize):
    create_file_with_size(filename, filesize)


@When(r'the user copies the (file|folder) "([^"]*)" to "([^"]*)"', regexp=True)
def step(context, resource_type, source_dir, destination_dir):
    source = get_resource_path(source_dir)
    destination = get_resource_path(destination_dir)
    if source == destination and destination_dir != '/':
        destination = add_copy_suffix(source, resource_type)
    if resource_type == 'folder':
        return shutil.copytree(source, destination)
    return shutil.copy2(source, destination)


@When(r'the user renames a (?:file|folder) "([^"]*)" to "([^"]*)"', regexp=True)
def step(context, source, destination):
    wait_for_client_to_be_ready()
    rename_file_folder(source, destination)


@Then('the file "|any|" should exist on the file system with the following content')
def step(context, file_path):
    expected = '\n'.join(context.multiLineText)
    file_path = get_resource_path(file_path)
    with open(file_path, 'r', encoding='utf-8') as f:
        contents = f.read()
    test.compare(
        expected,
        contents,
        'file expected to exist with content '
        + expected
        + ' but does not have the expected content',
    )


@Then(r'^the (file|folder) "([^"]*)" should exist on the file system$', regexp=True)
def step(context, resource_type, resource):
    resource_path = get_resource_path(resource)
    resource_exists = False
    if resource_type == 'file':
        resource_exists = file_exists(
            resource_path, get_config('maxSyncTimeout') * 1000
        )
    else:
        resource_exists = folder_exists(
            resource_path, get_config('maxSyncTimeout') * 1000
        )

    test.compare(
        True,
        resource_exists,
        f'Assert {resource_type} "{resource}" exists on the system',
    )


@Then(r'^the (file|folder) "([^"]*)" should not exist on the file system$', regexp=True)
def step(context, resource_type, resource):
    resource_path = get_resource_path(resource)
    resource_exists = False
    if resource_type == 'file':
        resource_exists = file_exists(resource_path, 1000)
    else:
        resource_exists = folder_exists(resource_path, 1000)

    test.compare(
        False,
        resource_exists,
        f'Assert {resource_type} "{resource}" doesn\'t exist on the system',
    )


@Given('the user has changed the content of local file "|any|" to:')
def step(context, filename):
    file_content = '\n'.join(context.multiLineText)
    wait_and_write_file(get_resource_path(filename), file_content)


@Then(
    'a conflict file for "|any|" should exist on the file system with the following content'
)
def step(context, filename):
    expected = '\n'.join(context.multiLineText)

    onlyfiles = [
        f for f in os.listdir(get_resource_path()) if isfile(get_resource_path(f))
    ]
    found = False
    pattern = re.compile(build_conflicted_regex(filename))
    for file in onlyfiles:
        if pattern.match(file):
            with open(get_resource_path(file), 'r', encoding='utf-8') as f:
                if f.read() == expected:
                    found = True
                    break

    if not found:
        raise AssertionError('Conflict file not found with given name')


@When('the user overwrites the file "|any|" with content "|any|"')
def step(context, resource, content):
    resource = get_resource_path(resource)
    wait_and_write_file(resource, content)


@When('the user tries to overwrite the file "|any|" with content "|any|"')
def step(context, resource, content):
    resource = get_resource_path(resource)
    wait_and_try_to_write_file(resource, content)


@When('user "|any|" tries to overwrite the file "|any|" with content "|any|"')
def step(context, user, resource, content):
    resource = get_resource_path(resource, user)
    wait_and_try_to_write_file(resource, content)


@When(r'the user deletes the (file|folder) "([^"]*)"', regexp=True)
def step(context, item_type, resource):
    wait_for_client_to_be_ready()

    resource_path = sanitize_path(get_resource_path(resource))
    if item_type == 'file':
        os.remove(resource_path)
    else:
        shutil.rmtree(resource_path)


@When('user "|any|" creates the following files inside the sync folder:')
def step(context, username):
    wait_for_client_to_be_ready()

    for row in context.table[1:]:
        file = get_resource_path(row[0], username)
        write_file(file, '')


@Given('the user has created a folder "|any|" in temp folder')
def step(context, folder_name):
    create_folder(folder_name, is_temp_folder=True)


@Given(
    'the user has created "|any|" files each of size "|any|" bytes inside folder "|any|" in temp folder'
)
def step(context, file_number, file_size, folder_name):
    current_sync_path = get_temp_resource_path(folder_name)
    if folder_exists(current_sync_path):
        file_size = builtins.int(file_size)
        for i in range(0, builtins.int(file_number)):
            file_name = f'file{i}.txt'
            create_file_with_size(join(current_sync_path, file_name), file_size, True)
    else:
        raise FileNotFoundError(
            f"Folder '{folder_name}' does not exist in the temp folder"
        )


@When(
    r'user "([^"]*)" moves (folder|file) "([^"]*)" from the temp folder into the sync folder',
    regexp=True,
)
def step(context, username, _, resource_name):
    source_dir = join(get_config('tempFolderPath'), resource_name)
    destination_dir = get_resource_path('/', username)
    shutil.move(source_dir, destination_dir)


@When(
    r'user "([^"]*)" moves (?:folder|file) "([^"]*)" to the temp folder',
    regexp=True,
)
def step(context, _, resource_name):
    source_dir = get_resource_path(resource_name)
    destination_dir = get_temp_resource_path(resource_name)
    shutil.move(source_dir, destination_dir)


@When(
    r'user "([^"]*)" moves (?:file|folder) "([^"]*)" to "([^"]*)" in the sync folder',
    regexp=True,
)
def step(context, username, source, destination):
    wait_for_client_to_be_ready()
    source_dir = get_resource_path(source, username)
    if destination in (None, '/'):
        destination = ''
    destination_dir = get_resource_path(destination, username)
    shutil.move(source_dir, destination_dir)


@Then('user "|any|" should be able to open the file "|any|" on the file system')
def step(context, user, file_name):
    file_path = get_resource_path(file_name, user)
    test.compare(can_read(file_path), True, 'File should be readable')


@Then('as "|any|" the file "|any|" should have content "|any|" on the file system')
def step(context, user, file_name, content):
    file_path = get_resource_path(file_name, user)
    file_content = read_file_content(file_path)
    test.compare(file_content, content, 'Comparing file content')


@Then('user "|any|" should not be able to edit the file "|any|" on the file system')
def step(context, user, file_name):
    file_path = get_resource_path(file_name, user)
    test.compare(not can_write(file_path), True, 'File should not be writable')


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
            content = ''
            if len(row) > 2 and row[2]:
                content = row[2]
            write_file(resource, content)
    create_zip(resource_list, zip_file_name, get_config('tempFolderPath'))


@When('user "|any|" unzips the zip file "|any|" inside the sync root')
def step(context, username, zip_file_name):
    destination_dir = get_resource_path('/', username)
    zip_file_path = join(destination_dir, zip_file_name)
    extract_zip(zip_file_path, destination_dir)


@When('user "|any|" copies file "|any|" to temp folder')
def step(context, username, source):
    wait_for_client_to_be_ready()
    source_dir = get_resource_path(source, username)
    destination_dir = get_temp_resource_path(source)
    shutil.copy2(source_dir, destination_dir)


@Given('the user has created folder "|any|" in the default home path')
def step(context, folder_name):
    folder_path = join(get_config('home_dir'), folder_name)
    os.makedirs(prefix_path_namespace(folder_path))
    remember_path(folder_path)
    # when account is added, folder with suffix will be created
    remember_path(f'{folder_path} (2)')
