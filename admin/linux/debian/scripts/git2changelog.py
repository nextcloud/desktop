#!//usr/bin/env python2.7

from __future__ import print_function
import subprocess
import re
import sys
import datetime
import os
import ConfigParser

try:
    long
except NameError:
    long = int

distribution="yakkety"

versionTagRE = re.compile("^v([0-9]+((\.[0-9]+)+))(-(.+))?$")

def processVersionTag(tag):
    m = versionTagRE.match(tag)
    if m:
        return (m.group(1), "release" if m.group(4) is None else "beta")
    else:
        return None

def getCommitVersion(commit):
    major=None
    minor=None
    patch=None
    try:
        for line in subprocess.check_output(["git", "show",
                                             commit + ":VERSION.cmake"]).splitlines():
            m = re.match("set\( MIRALL_VERSION_([A-Z]+) +([0-9]+) *\)", line)
            if m is not None:
                kind=m.group(1)
                version=m.group(2)
                if kind=="MAJOR":
                    major=version
                elif kind=="MINOR":
                    minor=version
                elif kind=="PATCH":
                    patch=version
        if major and minor and patch:
            return major + "." + minor + "." + patch
        else:
            return None
    except:
        return None

def collectEntries(baseCommit, baseVersion, kind, finalBaseVersion,
                   finalRevDate, config, finalCommit):

    newVersionCommit = None
    newVersionTag = None
    newVersionOrigTag = None

    if config is not None and config.has_section("versionhack"):
        if config.has_option("versionhack", "commit") and \
           config.has_option("versionhack", "tag"):
            newVersionCommit = config.get("versionhack", "commit")
            newVersionTag = config.get("versionhack", "tag")

    entries = []

    args = ["git", "log",
            "--format=%h%x09%an%x09%ae%x09%aD%x09%ad%x09%s",
            "--date=unix", "--author-date-order", "--reverse"]
    try:
        output = subprocess.check_output(args + [baseCommit + ".."])
    except:
        output = subprocess.check_output(args)


    lastVersionTag = None
    lastCMAKEVersion = None
    for line in output.splitlines():
        words = line.split("\t")
        (commit, name, email, date, revdate) = words[0:5]
        subject = "\t".join(words[5:])

        revdate = datetime.datetime.utcfromtimestamp(long(revdate)).strftime("%Y%m%d.%H%M%S")
        revdate += "." + commit

        kind = "alpha"

        if commit==newVersionCommit:
            result = processVersionTag(newVersionTag)
            if result:
                newVersionOrigTag = lastVersionTag
                (baseVersion, _kind) = result

        version=getCommitVersion(commit)
        if version and version!=lastCMAKEVersion:
            tag = "v" + version
            if tag!=newVersionOrigTag:
                result = processVersionTag(tag)
                if result:
                    lastVersionTag = tag
                    lastCMAKEVersion = version
                    (baseVersion, _kind) = result

        commitTags = []
        for tag in subprocess.check_output(["git", "tag",
                                            "--points-at",
                                            commit]).splitlines():
            if tag!=newVersionOrigTag:
                result = processVersionTag(tag)
                if result:
                    lastVersionTag = tag
                    commitTags.append(tag)
                    (baseVersion, kind1) = result
                    if kind1!=kind and kind!="release":
                        kind = kind1


        entries.append((commit, name, email, date, revdate, subject,
                        baseVersion, kind))

        if commit==finalCommit or finalCommit in commitTags:
            break

    if entries:
        (commit, name, email, date, revdate, subject, baseVersion, kind) = entries[-1]
        if finalRevDate is None:
            revdate = datetime.datetime.now().strftime("%Y%m%d.%H%M%S")+ "." + commit
        else:
            revdate = finalRevDate
        if finalBaseVersion is not None:
            baseVersion = finalBaseVersion
        entries[-1] = (commit, name, email, date, revdate, subject, baseVersion, kind)

    entries.reverse()

    return entries

def genChangeLogEntries(f, entries, distribution):
    latestBaseVersion = None
    latestRevDate = None
    latestKind = None
    for (commit, name, email, date, revdate, subject, baseVersion, kind) in entries:
        if latestBaseVersion is None:
            latestBaseVersion = baseVersion
            latestRevDate = revdate
            latestKind = kind
        upstreamVersion = baseVersion + "-" + revdate
        if distribution=="stable":
            version = upstreamVersion
        else:
            version = upstreamVersion + "-1.0~" + distribution + "1"
        print("nextcloud-desktop (%s) %s; urgency=medium" % (version, distribution), file=f)
        print(file=f)
        print("  * " + subject, file=f)
        print(file=f)
        print(" -- %s <%s>  %s" % (name, email, date), file=f)
        print(file=f)
    return (latestBaseVersion, latestRevDate, latestKind)

if __name__ == "__main__":
    scriptdir = os.path.dirname(__file__)
    configPath = os.path.join(scriptdir, "git2changelog.cfg")

    baseCommit = "f9b1c724d6ab5431e0cd56b7cd834f2dd48cebb1"
    baseVersion = "2.4.0"

    config = None
    if os.path.exists(configPath):
        config = ConfigParser.SafeConfigParser()
        config.read(configPath)

        if config.has_section("base"):
            if config.has_option("base", "commit") and \
               config.has_option("base", "version"):
                baseCommit = config.get("base", "commit")
                baseVersion = config.get("base", "version")

    distribution = sys.argv[2]
    finalRevDate = sys.argv[3] if len(sys.argv)>3 and sys.argv[3] else None
    finalBaseVersion = sys.argv[4] if len(sys.argv)>4 and sys.argv[4] else None
    finalCommit = sys.argv[5] if len(sys.argv)>5 and sys.argv[5] else None

    entries = collectEntries(baseCommit, baseVersion, "alpha",
                             finalBaseVersion, finalRevDate, config,
                             finalCommit)

    with open(sys.argv[1], "wt") as f:
        (baseVersion, revdate, kind) = genChangeLogEntries(f, entries, distribution)
        print(baseVersion, revdate, kind)
