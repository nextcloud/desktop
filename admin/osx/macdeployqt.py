#!/usr/bin/python
#  This file is part of ownCloud.
#  It was inspired in large part by the macdeploy script in Clementine
#  and Tomahawk
#
#  ownCloud is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  ownCLoud is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with ownCloud.  If not, see <http://www.gnu.org/licenses/>.

import os
import re
import subprocess
import commands
import sys
from glob import glob
from distutils.version import LooseVersion

def QueryQMake(attrib):
    return subprocess.check_output([qmake_path, '-query', attrib]).rstrip('\n')

FRAMEWORK_SEARCH_PATH=[
    '/Library/Frameworks',
    os.path.join(os.environ['HOME'], 'Library/Frameworks')
]

LIBRARY_SEARCH_PATH=['/usr/local/lib', '.']

QT_PLUGINS = [
    'sqldrivers/libqsqlite.dylib',
    'platforms/libqcocoa.dylib',
    'imageformats/libqgif.dylib',
    'imageformats/libqico.dylib',
    'imageformats/libqjpeg.dylib',
    'imageformats/libqsvg.dylib',
]

QT_PLUGINS_SEARCH_PATH=[
#    os.path.join(os.environ['QTDIR'], 'plugins'),
    '/usr/local/Cellar/qt/5.2.1/plugins',
]


class Error(Exception):
  pass


class CouldNotFindQtPluginErrorFindFrameworkError(Error):
  pass


class InstallNameToolError(Error):
  pass


class CouldNotFindQtPluginError(Error):
  pass


class CouldNotFindScriptPluginError(Error):
  pass

class CouldNotFindFrameworkError(Error):
  pass

if len(sys.argv) < 3:
  print 'Usage: %s <bundle.app> <path-to-qmake>' % sys.argv[0]
  exit()

def is_exe(fpath):
  return os.path.isfile(fpath) and os.access(fpath, os.X_OK)

bundle_dir = sys.argv[1]
qmake_path = sys.argv[2]

bundle_name = os.path.basename(bundle_dir).split('.')[0]

commands = []

binary_dir = os.path.join(bundle_dir, 'Contents', 'MacOS')
frameworks_dir = os.path.join(bundle_dir, 'Contents', 'Frameworks')
commands.append(['mkdir', '-p', frameworks_dir])
resources_dir = os.path.join(bundle_dir, 'Contents', 'Resources')
commands.append(['mkdir', '-p', resources_dir])
plugins_dir = os.path.join(bundle_dir, 'Contents', 'PlugIns')
binaries = [i for i in glob(os.path.join(bundle_dir, 'Contents', 'MacOS', "*")) if is_exe(i)];

qt_version = QueryQMake('QT_VERSION')
print "Using Qt", qt_version

fixed_libraries = []
fixed_frameworks = []

def WriteQtConf():
  print "Writing qt.conf..."
  with open(os.path.join(resources_dir, 'qt.conf'), 'w') as f:
    f.write("[Paths]\nPlugins = PlugIns\n");
    f.close()

def GetBrokenLibraries(binary):
  #print "Checking libs for binary: %s" % binary
  output = subprocess.Popen(['otool', '-L', binary], stdout=subprocess.PIPE).communicate()[0]
  broken_libs = {
      'frameworks': [],
      'libs': []}
  for line in [x.split(' ')[0].lstrip() for x in output.split('\n')[1:]]:
    #print "Checking line: %s" % line
    if not line:  # skip empty lines
      continue
    if os.path.basename(binary) == os.path.basename(line):
      #print "mnope %s-%s" % (os.path.basename(binary), os.path.basename(line))
      continue
    if re.match(r'^\s*/System/', line):
      continue  # System framework
    elif re.match(r'^\s*/usr/lib/', line):
      #print "unix style system lib"
      continue  # unix style system library
    elif re.match(r'Breakpad', line):
      continue  # Manually added by cmake.
    elif re.match(r'^\s*@executable_path', line) or re.match(r'^\s*@loader_path', line):
      # Potentially already fixed library
      if '.framework' in line:
        relative_path = os.path.join(*line.split('/')[3:])
        if not os.path.exists(os.path.join(frameworks_dir, relative_path)):
          broken_libs['frameworks'].append(relative_path)
      else:
        relative_path = os.path.join(*line.split('/')[1:])
        #print "RELPATH %s %s" % (relative_path, os.path.join(binary_dir, relative_path))
        if not os.path.exists(os.path.join(binary_dir, relative_path)):
          broken_libs['libs'].append(relative_path)
    elif re.search(r'\w+\.framework', line):
      broken_libs['frameworks'].append(line)
    else:
      broken_libs['libs'].append(line)

  return broken_libs

def FindFramework(path):
  search_pathes = FRAMEWORK_SEARCH_PATH
  search_pathes.insert(0, QueryQMake('QT_INSTALL_LIBS'))
  for search_path in search_pathes:
    # The following two lines are needed for a custom built Qt from version 5.5 on, possibly not for the one from the Qt SDK.
    # Looks like the upstream macdeployqt also had an issue there https://bugreports.qt.io/browse/QTBUG-47868
    if path.find( "\@rpath/"):
      path = path.replace("@rpath/", "")
    abs_path = os.path.join(search_path, path)
    if os.path.exists(abs_path):
      return abs_path

  raise CouldNotFindFrameworkError(path)

def FindLibrary(path):
  if os.path.exists(path):
    return path
  search_pathes = LIBRARY_SEARCH_PATH
  search_pathes.insert(0, QueryQMake('QT_INSTALL_LIBS'))
  for search_path in search_pathes:
    abs_path = os.path.join(search_path, path)
    if os.path.exists(abs_path):
      return abs_path
    else: # try harder---look for lib name in library folders
      newpath = os.path.join(search_path,os.path.basename(path))
      if os.path.exists(newpath):
        return newpath

  return ""
  #raise CouldNotFindFrameworkError(path)

def FixAllLibraries(broken_libs):
  for framework in broken_libs['frameworks']:
    FixFramework(framework)
  for lib in broken_libs['libs']:
    FixLibrary(lib)

def FixFramework(path):
  if path in fixed_libraries:
    return
  else:
    fixed_libraries.append(path)
  abs_path = FindFramework(path)
  broken_libs = GetBrokenLibraries(abs_path)
  FixAllLibraries(broken_libs)

  new_path = CopyFramework(abs_path)
  id = os.sep.join(new_path.split(os.sep)[3:])
  FixFrameworkId(new_path, id)
  for framework in broken_libs['frameworks']:
    FixFrameworkInstallPath(framework, new_path)
  for library in broken_libs['libs']:
    FixLibraryInstallPath(library, new_path)

def FixLibrary(path):
  if path in fixed_libraries or FindSystemLibrary(os.path.basename(path)) is not None:
    return
  else:
    fixed_libraries.append(path)
  abs_path = FindLibrary(path)
  if abs_path == "":
    print "Could not resolve %s, not fixing!" % path
    return
  broken_libs = GetBrokenLibraries(abs_path)
  FixAllLibraries(broken_libs)

  new_path = CopyLibrary(abs_path)
  FixLibraryId(new_path)
  for framework in broken_libs['frameworks']:
    FixFrameworkInstallPath(framework, new_path)
  for library in broken_libs['libs']:
    FixLibraryInstallPath(library, new_path)

def FixPlugin(abs_path, subdir):
  broken_libs = GetBrokenLibraries(abs_path)
  FixAllLibraries(broken_libs)

  new_path = CopyPlugin(abs_path, subdir)
  for framework in broken_libs['frameworks']:
    FixFrameworkInstallPath(framework, new_path)
  for library in broken_libs['libs']:
    FixLibraryInstallPath(library, new_path)

def FixBinary(path):
  broken_libs = GetBrokenLibraries(path)
  FixAllLibraries(broken_libs)
  for framework in broken_libs['frameworks']:
    FixFrameworkInstallPath(framework, path)
  for library in broken_libs['libs']:
    FixLibraryInstallPath(library, path)

def CopyLibrary(path):
  new_path = os.path.join(binary_dir, os.path.basename(path))
  args = ['ditto', '--arch=x86_64', path, new_path]
  commands.append(args)
  args = ['chmod', 'u+w', new_path]
  commands.append(args)
  return new_path

def CopyPlugin(path, subdir):
  new_path = os.path.join(plugins_dir, subdir, os.path.basename(path))
  args = ['mkdir', '-p', os.path.dirname(new_path)]
  commands.append(args)
  args = ['ditto', '--arch=x86_64', path, new_path]
  commands.append(args)
  args = ['chmod', 'u+w', new_path]
  commands.append(args)
  return new_path

def CopyFramework(source_dylib):
  parts = source_dylib.split(os.sep)
  print "CopyFramework:", source_dylib
  for i, part in enumerate(parts):
    matchObj = re.match(r'(\w+\.framework)', part)
    if matchObj:
      framework = matchObj.group(1)
      dylib_name = parts[-1]
      source_path = os.path.join('/', *parts[:i+1])
      dest_path = os.path.join(frameworks_dir, framework)
      dest_dylib_path = os.path.join(frameworks_dir, *parts[i:-1])
      break
  if os.path.exists(dest_path):
    print dest_path, "already exists, skipping copy..."
    return os.path.join(dest_dylib_path, dylib_name)

  args = ['mkdir', '-p', dest_dylib_path]
  commands.append(args)
  args = ['ditto', '--arch=x86_64', source_dylib, dest_dylib_path]
  commands.append(args)
  args = ['chmod', 'u+w', os.path.join(dest_dylib_path, parts[-1])]
  commands.append(args)
  args = ['ln', '-s', '5', os.path.join(dest_path, 'Versions', 'Current')]
  commands.append(args)
  args = ['ln', '-s', os.path.join('Versions', 'Current', dylib_name), os.path.join(dest_path, dylib_name)]
  commands.append(args)
  args = ['ln', '-s', os.path.join('Versions', 'Current', 'Resources'), os.path.join(dest_path, 'Resources')]
  commands.append(args)
  args = ['cp', '-r', os.path.join(source_path, 'Versions', '5', 'Resources'), os.path.join(dest_path, 'Versions', '5')]
  commands.append(args)

  return os.path.join(dest_dylib_path, dylib_name)

def FixId(path, library_name):
  id = '@executable_path/../Frameworks/%s' % library_name
  args = ['install_name_tool', '-id', id, path]
  commands.append(args)

def FixLibraryId(path):
  library_name = os.path.basename(path)
  FixId(path, library_name)

def FixFrameworkId(path, id):
  FixId(path, id)

def FixInstallPath(library_path, library, new_path):
  args = ['install_name_tool', '-change', library_path, new_path, library]
  commands.append(args)

def FindSystemLibrary(library_name):
  for path in ['/lib', '/usr/lib']:
    full_path = os.path.join(path, library_name)
    if os.path.exists(full_path):
      return full_path
  return None

def FixLibraryInstallPath(library_path, library):
  system_library = FindSystemLibrary(os.path.basename(library_path))
  if system_library is None:
    new_path = '@executable_path/../MacOS/%s' % os.path.basename(library_path)
    FixInstallPath(library_path, library, new_path)
  else:
    FixInstallPath(library_path, library, system_library)

def FixFrameworkInstallPath(library_path, library):
  parts = library_path.split(os.sep)
  for i, part in enumerate(parts):
    if re.match(r'\w+\.framework', part):
      full_path = os.path.join(*parts[i:])
      break
  new_path = '@executable_path/../Frameworks/%s' % full_path
  FixInstallPath(library_path, library, new_path)

def FindQtPlugin(name):
  search_path = QT_PLUGINS_SEARCH_PATH
  search_path.insert(0, QueryQMake('QT_INSTALL_PLUGINS'))
  for path in search_path:
    if os.path.exists(path):
      if os.path.exists(os.path.join(path, name)):
        return os.path.join(path, name)
  raise CouldNotFindQtPluginError(name)

for binary in binaries:
  FixBinary(binary)


if LooseVersion(qt_version) >= LooseVersion("5.10.0"):
  QT_PLUGINS.append('styles/libqmacstyle.dylib')
for plugin in QT_PLUGINS:
  FixPlugin(FindQtPlugin(plugin), os.path.dirname(plugin))

if LooseVersion(qt_version) >= LooseVersion("5.10.0"):
  args = ['plutil', '-insert', 'LSMinimumSystemVersion', '-string', '10.10.0', os.path.join(bundle_dir, 'Contents', 'Info.plist')]
  commands.append(args)
else:
  args = ['plutil', '-insert', 'LSMinimumSystemVersion', '-string', '10.7.0', os.path.join(bundle_dir, 'Contents', 'Info.plist')]
  commands.append(args)

if len(sys.argv) <= 2:
  print 'Will run %d commands:' % len(commands)
  for command in commands:
    print ' '.join(command)

for command in commands:
  p = subprocess.Popen(command)
  os.waitpid(p.pid, 0)

WriteQtConf()
