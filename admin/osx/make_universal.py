#!/usr/bin/env python

import sys
import os
import subprocess


# A general note: We first produce a x86_64 and a arm64 app package
# and then merge them together instead of compiling the desktop client
# with the CMake option CMAKE_OSX_ARCHITECTURES="x86_64;arm64" because
# macdeployqt can not handle universal binaries well. In the future
# with Qt6 this might change and this script will become obsolete.


def usage(program_name):
    print("Creates a universal app package from a x86_64 and a arm64 app package.")
    print("Usage: {} x86_64_app_file arm64_app_file output_directory".format(program_name))
    print("Example: {} some_dir/Nextcloud.app some_other_dir/Nextcloud.app output_dir".format(program_name))


def execute(command):
    return subprocess.check_output(command)


def path_relative_to_package(app_package_file_path, file_path):
    if file_path.startswith(app_package_file_path):
        relative_path = file_path[len(app_package_file_path):]
        if relative_path.startswith("/"):
            return relative_path[1:]
        return relative_path
    return file_path


def is_executable(file_path):
    output = str(execute(["file", file_path]))
    if (("Mach-O 64-bit dynamically linked shared library" in output)
        or ("Mach-O 64-bit executable" in output)
            or ("Mach-O 64-bit bundle" in output)):
        return True
    return False


if __name__ == "__main__":
    if len(sys.argv) != 4:
        usage(sys.argv[0])
        sys.exit(1)

    x86_64_app_file = sys.argv[1]
    if not os.path.exists(x86_64_app_file):
        print("Can't create universal: Path {} does not exist".format(x86_64_app_file))
        sys.exit(1)
    arm64_app_file = sys.argv[2]
    if not os.path.exists(arm64_app_file):
        print("Can't create universal: Path {} does not exist".format(arm64_app_file))
        sys.exit(1)
    output_dir = sys.argv[3]

    # Copy the Arm64 variant to the output location if possible
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    app_file_name = os.path.basename(arm64_app_file)
    universal_app_file = os.path.join(output_dir, app_file_name)
    if os.path.exists(universal_app_file):
        print("Can't create universal: Path {} already exists".format(universal_app_file))
        sys.exit(1)

    execute(["cp", "-a", arm64_app_file, output_dir])

    # Now walk through the copied arm64 version and replace the binaries
    for root, dirs, files in os.walk(universal_app_file):
        for f in files:
            absolute_file_path = os.path.join(root, f)
            root_relative = path_relative_to_package(universal_app_file, root)
            x86_64_absolute_path = os.path.join(x86_64_app_file, root_relative, f)
            arm64_absolute_path = os.path.join(arm64_app_file, root_relative, f)
            if os.path.islink(absolute_file_path) or not is_executable(absolute_file_path):
                continue
            try:
                print(f"Going to merge {arm64_absolute_path} with {x86_64_absolute_path} into {absolute_file_path}")
                execute(["lipo", "-create", "-output", absolute_file_path, arm64_absolute_path, x86_64_absolute_path])
                print(execute(["lipo", "-info", absolute_file_path]))
            except:
                print(f"Could not merge {arm64_absolute_path} with {x86_64_absolute_path} into {absolute_file_path}!")

    print("Finished :)")
