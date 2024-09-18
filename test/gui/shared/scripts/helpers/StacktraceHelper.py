import os
import subprocess
import glob
import re
from datetime import datetime
from helpers.ConfigHelper import isWindows


def getCoredumps():
    # TODO: find a way to use coredump in windows
    if isWindows():
        return False
    # read coredump location
    with open('/proc/sys/kernel/core_pattern', 'r', encoding='utf-8') as f:
        coredumpPath = f.read().strip('\n')

    # yields something like: /tmp/core-*-*-*-*
    coredumpFilePattern = re.sub(r'%[a-zA-Z]{1}', '*', coredumpPath)
    return glob.glob(coredumpFilePattern)


def generateStacktrace(scenario_title, coredumps):
    message = ['###########################################']
    message.append(f'Scenario: {scenario_title}')

    for coredumpFile in coredumps:
        message.append(parseStacktrace(coredumpFile))

    message.append('###########################################')
    message.append('')
    stacktrace = '\n'.join(message)

    stacktrace_file = os.environ.get('STACKTRACE_FILE', '../stacktrace.log')
    # save stacktrace to a file
    with open(stacktrace_file, 'a', encoding='utf-8') as f:
        f.write(stacktrace)


def parseStacktrace(coredumpFile):
    message = []
    if coredumpFile:
        coredumpFilename = os.path.basename(coredumpFile)
        # example coredump file: core-1648445754-1001-11-!drone!src!build-GUI-tests!bin!owncloud
        patterns = coredumpFilename.split('-')
        appBinary = '-'.join(patterns[4:]).replace('!', '/')

        message.append('-------------------------------------------')
        message.append(f'Executable: {appBinary}')
        message.append(f'Timestamp: {str(datetime.fromtimestamp(float(patterns[1])))}')
        message.append(f'Process ID: {patterns[2]}')
        message.append(f'Signal Number: {patterns[3]}')
        message.append('-------------------------------------------')
        message.append('<<<<< STACKTRACE START >>>>>')
        message.append(
            subprocess.run(
                [
                    'gdb',
                    appBinary,
                    coredumpFile,
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
        os.unlink(coredumpFile)

    return '\n'.join(message)
