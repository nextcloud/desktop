import os
import subprocess
import glob
import re
from datetime import datetime


def getCoredumps():
    # read coredump location
    with open("/proc/sys/kernel/core_pattern", "r") as f:
        coredumpPath = f.read().strip("\n")

    # yields something like: /tmp/core-*-*-*-*
    coredumpFilePattern = re.sub(r'%[a-zA-Z]{1}', '*', coredumpPath)
    return glob.glob(coredumpFilePattern)


def generateStacktrace(scenario_title, coredumps):
    message = ["###########################################"]
    message.append("Scenario: " + scenario_title)

    for coredumpFile in coredumps:
        message.append(parseStacktrace(coredumpFile))

    message.append("###########################################")
    message.append("")
    message = "\n".join(message)

    stacktrace_file = os.environ.get("STACKTRACE_FILE", "../stacktrace.log")
    # save stacktrace to a file
    open(stacktrace_file, "a").write(message)


def parseStacktrace(coredumpFile):
    message = []
    if coredumpFile:
        coredumpFilename = os.path.basename(coredumpFile)
        # example coredump file: core-1648445754-1001-11-!drone!src!build-GUI-tests!bin!owncloud
        patterns = coredumpFilename.split('-')
        appBinary = "-".join(patterns[4:]).replace('!', '/')

        message.append("-------------------------------------------")
        message.append("Executable: " + appBinary)
        message.append("Timestamp: " + str(datetime.fromtimestamp(float(patterns[1]))))
        message.append("Process ID: " + patterns[2])
        message.append("Signal Number: " + patterns[3])
        message.append("-------------------------------------------")
        message.append("<<<<< STACKTRACE START >>>>>")
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
            ).stdout.decode('utf-8')
        )
        message.append("<<<<< STACKTRACE END >>>>>")

        # remove coredump file
        os.unlink(coredumpFile)

    return "\n".join(message)
