import os
import re
import ctypes
import shutil

from helpers.ConfigHelper import is_windows


def build_conflicted_regex(filename):
    if "." in filename:
        # TODO: improve this for complex filenames
        namepart = filename.split(".")[0]
        extpart = filename.split(".")[1]
        # pylint: disable=anomalous-backslash-in-string
        return "%s \(conflicted copy \d{4}-\d{2}-\d{2} \d{6}\)\.%s" % (
            namepart,
            extpart,
        )
    # pylint: disable=anomalous-backslash-in-string
    return "%s \(conflicted copy \d{4}-\d{2}-\d{2} \d{6}\)" % filename


def sanitize_path(path):
    return path.replace("//", "/")


def prefix_path_namespace(path):
    if is_windows():
        # https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file?redirectedfrom=MSDN#win32-file-namespaces
        # disable string parsing
        #  - long path
        #  - trailing whitespaces
        return f"\\\\?\\{path}"
    return path


def can_read(resource):
    read = False
    try:
        with open(resource, encoding="utf-8") as f:
            read = True
    except:
        pass
    return read and os.access(resource, os.R_OK)


def can_write(resource):
    write = False
    try:
        with open(resource, "w", encoding="utf-8") as f:
            write = True
    except:
        pass
    return write and os.access(resource, os.W_OK)


def read_file_content(file):
    with open(file, "r", encoding="utf-8") as f:
        content = f.read()
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
        size_unit = match.group(2)

        if not (size_unit := size_unit.lower()):
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

    raise ValueError("Invalid size: " + size)


def get_file_size_on_disk(resource_path):
    file_size_high = ctypes.c_ulonglong(0)
    if is_windows():
        return ctypes.windll.kernel32.GetCompressedFileSizeW(
            ctypes.c_wchar_p(resource_path), ctypes.pointer(file_size_high)
        )
    raise OSError("'get_file_size_on_disk' function is only supported for Windows OS.")


def get_file_size(resource_path):
    return os.stat(resource_path).st_size


# temp paths created outside of the temporary directory during the test
CREATED_PATHS = []


def remember_path(path):
    CREATED_PATHS.append(path)


def cleanup_created_paths():
    global CREATED_PATHS
    for path in CREATED_PATHS:
        if os.path.exists(path):
            if os.path.isdir(path):
                shutil.rmtree(prefix_path_namespace(path))
            else:
                os.unlink(prefix_path_namespace(path))
    CREATED_PATHS = []
