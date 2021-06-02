#!/usr/bin/env python3

# Small script that counts the warnings which the compiler emits
# and takes care that not more warnings are added.

import sys
import os
import re
import requests


from pathlib import Path

upload_branch = "feature/enable-clazy"

if len(sys.argv) != 2:
    print(f"Usage: {sys.argv[0]} REPOSITORY_PATH")
    sys.exit(1)

repository_path = sys.argv[1]

warning_regex = re.compile(r'warning:', re.M)

max_allowed_warnings_count_response = requests.get(f"https://nextclouddesktopwarningscount.felixweilbach.de")

if max_allowed_warnings_count_response.status_code != 200:
    print('Can not get maximum number of allowed warnings')
    sys.exit(1)

max_allowed_warnings_count = int(max_allowed_warnings_count_response.content)

print("Max number of allowed warnings:", max_allowed_warnings_count)

warnings_count = 0

for line in sys.stdin:
    if warning_regex.findall(line):
        warnings_count += 1

    print(line, end="")

    if warnings_count > max_allowed_warnings_count:
        print("Error: You probably introduced a new warning!")
        sys.exit(1)

print("Total number of warnings:", warnings_count)


def get_active_branch_name(path):
    head_dir = Path(path) / ".git" / "HEAD"
    with head_dir.open("r") as f: content = f.read().splitlines()

    for line in content:
        if line[0:4] == "ref:":
            return line.partition("refs/heads/")[2]


active_branch = get_active_branch_name(repository_path)

if active_branch == upload_branch:
    print(f"Running on {upload_branch} branch. Upload warnings count.")
    sshcmd = f"ssh -o StrictHostKeyChecking=no nextclouddesktop@felixweilbach.de 'set_warnings_count {warnings_count}'"
    os.system(sshcmd)
