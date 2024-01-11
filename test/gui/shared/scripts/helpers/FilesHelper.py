import os
import re
import ctypes
from helpers.ConfigHelper import isWindows


def buildConflictedRegex(filename):
    if '.' in filename:
        # TODO: improve this for complex filenames
        namepart = filename.split('.')[0]
        extpart = filename.split('.')[1]
        return '%s \(conflicted copy \d{4}-\d{2}-\d{2} \d{6}\)\.%s' % (
            namepart,
            extpart,
        )
    else:
        return '%s \(conflicted copy \d{4}-\d{2}-\d{2} \d{6}\)' % (filename)


def sanitizePath(path):
    return path.replace('//', '/')


def can_read(resource):
    can_read = False
    try:
        f = open(resource)
        f.close()
        can_read = True
    except:
        pass
    return can_read and os.access(resource, os.R_OK)


def can_write(resource):
    can_write = False
    try:
        f = open(resource, 'w')
        f.close()
        can_write = True
    except:
        pass
    return can_write and os.access(resource, os.W_OK)


def read_file_content(file):
    f = open(file, "r")
    content = f.read()
    f.close()
    return content


def is_empty_sync_folder(folder):
    ignore_files = ["Desktop.ini"]
    for item in os.listdir(folder):
        # do not count the hidden files as they are ignored by the client
        if not item.startswith(".") and not item in ignore_files:
            return False
    return True


def get_size_in_bytes(size):
    match = re.match(r"(\d+)((?: )?[KkMmGgBb]{0,2})?", str(size))
    units = ["b", "kb", "mb", "gb"]
    multiplier = 1024
    if match:
        size_num = int(match.group(1))
        size_unit = match.group(2).lower()

        if not size_unit:
            return size_num
        if size_unit in units:
            if size_unit == "b":
                return size_num
            if size_unit == "kb":
                return size_num * multiplier
            if size_unit == "mb":
                return size_num * (multiplier**2)
            if size_unit == "gb":
                return size_num * (multiplier**3)

    raise Exception("Invalid size: " + size)


def get_file_size_on_disk(resource_path):
    file_size_high = ctypes.c_ulonglong(0)
    if isWindows():
        return ctypes.windll.kernel32.GetCompressedFileSizeW(
            ctypes.c_wchar_p(resource_path), ctypes.pointer(file_size_high)
        )
    raise Exception(
        "'get_file_size_on_disk' function is only supported for Windows OS."
    )


def get_file_size(resource_path):
    return os.stat(resource_path).st_size
