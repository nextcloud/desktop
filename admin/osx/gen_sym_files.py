#!/usr/bin/env python

import logging, os, re, subprocess, sys
import os.path
import pdb, pprint

if len(sys.argv) < 4:
    print("Usage:")
    print("\tgen_sym_files.py <path to breakpad's dump_syms> <path to owncloud.app> <symbol output dir>")
    print("")
    print("Symbols will be created in './symbols'")
    sys.exit(1)

dump_symsPath = sys.argv[1]
bundlePath = sys.argv[2]
outPath = sys.argv[3]
macOsDir = os.path.join(bundlePath, 'Contents', 'MacOS')
pluginsDir = os.path.join(bundlePath, 'Contents', 'PlugIns')

def resolvePath(input):
    resolved = re.sub(r'@\w+', macOsDir, input)
    return os.path.normpath(resolved)

def extractDeps(macho):
    deps = [macho]
    otool = subprocess.Popen(['otool', '-L', macho], stdout=subprocess.PIPE)
    for l in otool.communicate()[0].splitlines():
        m = re.search(r'@[^\s]+', l)
        if m:
            path = resolvePath(m.group(0))
            if not os.path.exists(path):
                logging.warning("Non-existant file found in dependencies, ignoring: [%s]", path)
                continue
            deps.append(path)
    return deps

def findDeps():
    deps = []
    for f in os.listdir(macOsDir):
        path = os.path.join(macOsDir, f)
        if not os.path.islink(path):
            deps += extractDeps(path)
    for root, dirs, files in os.walk(pluginsDir):
        for f in files:
            path = os.path.join(root, f)
            deps += extractDeps(path)
    return sorted(set(deps))

def dumpSyms(deps):
    for dep in deps:
        print("Generating symbols for [%s]" % dep)
        with open('temp.sym', 'w') as temp:
            subprocess.check_call([dump_symsPath, dep], stdout=temp)
        with open('temp.sym', 'r') as temp:
            header = temp.readline()
            fields = header.split()
            key, name = fields[3:5]
        destDir = '%s/%s/%s/' % (outPath, name, key)
        destPath = destDir + name + '.sym'
        if os.path.exists(destPath):
            logging.warning("Symbols already exist: [%s]", destPath)
            continue
        if not os.path.exists(destDir):
            os.makedirs(destDir)
        os.rename('temp.sym', destPath)

def strip(deps):
    for dep in deps:
        print("Stripping symbols off [%s]" % dep)
        subprocess.check_call(['strip', '-S', dep])

print('=== Generating symbols for [%s] in [%s]' % (bundlePath, outPath))
deps = findDeps()
dumpSyms(deps)
strip(deps)
