import os
import subprocess
import glob
import re
from datetime import datetime
from helpers.ConfigHelper import is_windows


def get_core_dumps():
    # TODO: find a way to use coredump in windows
    if is_windows():
        return False
    # read coredump location
    with open('/proc/sys/kernel/core_pattern', 'r', encoding='utf-8') as f:
        coredump_path = f.read().strip('\n')

    # yields something like: /tmp/core-*-*-*-*
    coredump_file_pattern = re.sub(r'%[a-zA-Z]{1}', '*', coredump_path)
    return glob.glob(coredump_file_pattern)


def generate_stacktrace(scenario_title, coredumps):
    message = ['###########################################']
    message.append(f'Scenario: {scenario_title}')

    for coredump_file in coredumps:
        message.append(parse_stacktrace(coredump_file))

    message.append('###########################################')
    message.append('')
    stacktrace = '\n'.join(message)

    stacktrace_file = os.environ.get('STACKTRACE_FILE', '../stacktrace.log')
    # save stacktrace to a file
    with open(stacktrace_file, 'a', encoding='utf-8') as f:
        f.write(stacktrace)


def parse_stacktrace(coredump_file):
    message = []
    if coredump_file:
        coredump_filename = os.path.basename(coredump_file)
        # example coredump file: core-1648445754-1001-11-!drone!src!build-GUI-tests!bin!owncloud
        patterns = coredump_filename.split('-')
        app_binary = '-'.join(patterns[4:]).replace('!', '/')

        message.append('-------------------------------------------')
        message.append(f'Executable: {app_binary}')
        message.append(f'Timestamp: {str(datetime.fromtimestamp(float(patterns[1])))}')
        message.append(f'Process ID: {patterns[2]}')
        message.append(f'Signal Number: {patterns[3]}')
        message.append('-------------------------------------------')
        message.append('<<<<< STACKTRACE START >>>>>')
        message.append(
            subprocess.run(
                [
                    'gdb',
                    app_binary,
                    coredump_file,
                    '-batch',
                    '-ex',
                    'bt full',
                ],
                stdout=subprocess.PIPE,
                check=False,
            ).stdout.decode('utf-8')
        )
        message.append('<<<<< STACKTRACE END >>>>>')

        # remove coredump file
        os.unlink(coredump_file)

    return '\n'.join(message)
