# SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
# SPDX-FileCopyrightText: 2014 ownCloud GmbH
# SPDX-License-Identifier: GPL-2.0-or-later
#
# This program is the core of the nextcloud integration to Nautilus
# It will be installed on /usr/share/nautilus-python/extensions/ with the paquet Nextcloud-client-nautilus
# (https://github.com/nextcloud/desktop/edit/master/shell_integration/nautilus/syncstate.py)

import sys
python3 = sys.version_info[0] >= 3

import os
import urllib
if python3:
    import urllib.parse
import socket
import tempfile
import time

from gi.repository import GObject, Nautilus

# Note: setappname.sh will search and replace 'Nextcloud' on this file to update this line and other
# occurrences of the name
appname = 'Nextcloud'

print("Initializing "+appname+"-client-nautilus extension")
print("Using python version {}".format(sys.version_info))

def get_local_path(url):
    if url[0:7] == 'file://':
        url = url[7:]
    unquote = urllib.parse.unquote if python3 else urllib.unquote
    return unquote(url)

def get_runtime_dir():
    """Returns the value of $XDG_RUNTIME_DIR, a directory path.

    If the value is not set, returns the same default as in Qt5
    """
    try:
        return os.environ['XDG_RUNTIME_DIR']
    except KeyError:
        fallback = os.path.join(tempfile.gettempdir(), 'runtime-' + os.environ['USER'])
        return fallback


class SocketConnect(GObject.GObject):
    def __init__(self):
        GObject.GObject.__init__(self)
        self.connected = False
        self.registered_paths = {}
        self._watch_id = 0
        self._sock = None
        self._listeners = [self._update_registered_paths, self._get_version]
        self._remainder = ''.encode() if python3 else ''
        self.protocolVersion = '1.0'
        self.nautilusVFSFile_table = {}  # not needed in this object actually but shared
                                         # all over the other objects.

        # returns true when one should try again!
        if self._connectToSocketServer():
            GObject.timeout_add(5000, self._connectToSocketServer)

    def reconnect(self):
        self._sock.close()
        self.connected = False
        GObject.source_remove(self._watch_id)
        GObject.timeout_add(5000, self._connectToSocketServer)

    def sendCommand(self, cmd):
        # print("Server command: " + cmd)
        if self.connected:
            try:
                self._sock.send(cmd.encode() if python3 else cmd)
            except:
                print("Sending failed.")
                self.reconnect()
        else:
            print("Cannot send, not connected!")

    def addListener(self, listener):
        self._listeners.append(listener)

    def _connectToSocketServer(self):
        try:
            self._sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            sock_file = os.path.join(get_runtime_dir(), appname, "socket")
            try:
                self._sock.connect(sock_file) # fails if sock_file doesn't exist
                self.connected = True
                self._watch_id = GObject.io_add_watch(self._sock, GObject.IO_IN, self._handle_notify)

                self.sendCommand('VERSION:\n')
                self.sendCommand('GET_STRINGS:\n')

                return False  # Don't run again
            except Exception as e:
                print("Could not connect to unix socket " + sock_file + ". " + str(e))
        except Exception as e:  # Bad habit
            print("Connect could not be established, try again later.")
            self._sock.close()

        return True  # Run again, if enabled via timeout_add()

    # Reads data that becomes available.
    # New responses can be accessed with get_available_responses().
    # Returns false if no data was received within timeout
    def read_socket_data_with_timeout(self, timeout):
        self._sock.settimeout(timeout)
        try:
            self._remainder += self._sock.recv(1024)
        except socket.timeout:
            return False
        else:
            return True
        finally:
            self._sock.settimeout(None)

    # Parses response lines out of collected data, returns list of strings
    def get_available_responses(self):
        end = self._remainder.rfind(b'\n')
        if end == -1:
            return []
        data = self._remainder[:end]
        self._remainder = self._remainder[end+1:]
        data = data.decode() if python3 else data
        return data.split('\n')

    # Notify is the raw answer from the socket
    def _handle_notify(self, source, condition):
        # Blocking is ok since we're notified of available data
        self._remainder += self._sock.recv(1024)

        if len(self._remainder) == 0:
            return False

        for line in self.get_available_responses():
            self.handle_server_response(line)

        return True  # Run again

    def handle_server_response(self, line):
        # print("Server response: " + line)
        parts = line.split(':')
        action = parts[0]
        args = parts[1:]

        for listener in self._listeners:
            listener(action, args)

    def _update_registered_paths(self, action, args):
        if action == 'REGISTER_PATH':
            self.registered_paths[args[0]] = 1
        elif action == 'UNREGISTER_PATH':
            del self.registered_paths[args[0]]

            # Check if there are no paths left. If so, its usual
            # that mirall went away. Try reconnecting.
            if not self.registered_paths:
                self.reconnect()

    def _get_version(self, action, args):
        if action == 'VERSION':
            self.protocolVersion = args[1]

socketConnect = SocketConnect()


class MenuExtension_Nextcloud(GObject.GObject, Nautilus.MenuProvider):
    def __init__(self):
        GObject.GObject.__init__(self)

        self.strings = {}
        socketConnect.addListener(self.handle_commands)

    def handle_commands(self, action, args):
        if action == 'STRING':
            self.strings[args[0]] = ':'.join(args[1:])

    def check_registered_paths(self, filename):
        topLevelFolder = False
        internalFile = False
        absfilename = os.path.realpath(filename)
        for reg_path in socketConnect.registered_paths:
            if absfilename == reg_path:
                topLevelFolder = True
                break
            if absfilename.startswith(reg_path):
                internalFile = True
                # you can't have a registered path below another so it is save to break here
                break
        return (topLevelFolder, internalFile)

    # The get_file_items method of Nautilus.MenuProvider no longer takes
    # the window argument. To keep supporting older versions of Nautilus,
    # we can use variadic arguments.
    def get_file_items(self, *args):
        # Show the menu extension to share a file or folder

        files = args[-1]
        # Get usable file paths from the uris
        all_internal_files = True
        for i, file_uri in enumerate(files):
            filename = get_local_path(file_uri.get_uri())
            filename = os.path.realpath(filename)

            # Check if its a folder (ends with an /), if yes add a "/"
            # otherwise it will not find the entry in the table
            isDir = os.path.isdir(filename + os.sep)
            if isDir:
                filename += os.sep

            # Check if toplevel folder, we need to ignore those as they cannot be shared
            topLevelFolder, internalFile = self.check_registered_paths(filename)
            if not internalFile:
                all_internal_files = False

            files[i] = filename

        # Don't show a context menu if some selected files aren't in a sync folder
        if not all_internal_files:
            return []

        if socketConnect.protocolVersion >= '1.1':  # lexicographic!
            return self.ask_for_menu_items(files)
        else:
            return self.legacy_menu_items(files)

    def ask_for_menu_items(self, files):
        record_separator = '\x1e'
        filesstring = record_separator.join(files)
        socketConnect.sendCommand(u'GET_MENU_ITEMS:{}\n'.format(filesstring))

        done = False
        start = time.time()
        timeout = 0.1 # 100ms
        menu_items = []
        while not done:
            dt = time.time() - start
            if dt >= timeout:
                break
            if not socketConnect.read_socket_data_with_timeout(timeout - dt):
                break
            for line in socketConnect.get_available_responses():
                # Process lines we don't care about
                if done or not (line.startswith('GET_MENU_ITEMS:') or line.startswith('MENU_ITEM:')):
                    socketConnect.handle_server_response(line)
                    continue
                if line == 'GET_MENU_ITEMS:END':
                    done = True
                    # don't break - we'd discard other responses
                if line.startswith('MENU_ITEM:'):
                    args = line.split(':')
                    if len(args) < 4:
                        continue
                    menu_items.append([args[1], 'd' not in args[2], ':'.join(args[3:])])

        if not done:
            return self.legacy_menu_items(files)

        if len(menu_items) == 0:
            return []

        # Set up the 'Nextcloud...' submenu
        item_nextcloud = Nautilus.MenuItem(
            name='IntegrationMenu', label=self.strings.get('CONTEXT_MENU_TITLE', appname))
        menu = Nautilus.Menu()
        item_nextcloud.set_submenu(menu)

        for action, enabled, label in menu_items:
            item = Nautilus.MenuItem(name=action, label=label, sensitive=enabled)
            item.connect("activate", self.context_menu_action, action, filesstring)
            menu.append_item(item)

        return [item_nextcloud]


    def legacy_menu_items(self, files):
        # No legacy menu for a selection of several files
        if len(files) != 1:
            return []
        filename = files[0]

        entry = socketConnect.nautilusVFSFile_table.get(filename)
        if not entry:
            return []

        # Currently 'sharable' also controls access to private link actions,
        # and we definitely don't want to show them for IGNORED.
        shareable = False
        state = entry['state']
        state_ok = state.startswith('OK')
        state_sync = state.startswith('SYNC')
        isDir = os.path.isdir(filename + os.sep)
        if state_ok:
            shareable = True
        elif state_sync and isDir:
            # some file below is OK or SYNC
            for key, value in socketConnect.nautilusVFSFile_table.items():
                if key != filename and key.startswith(filename):
                    state = value['state']
                    if state.startswith('OK') or state.startswith('SYNC'):
                        shareable = True
                        break

        if not shareable:
            return []

        # Set up the 'Nextcloud...' submenu
        item_nextcloud = Nautilus.MenuItem(
            name='IntegrationMenu', label=self.strings.get('CONTEXT_MENU_TITLE', appname))
        menu = Nautilus.Menu()
        item_nextcloud.set_submenu(menu)

        # Add share menu option
        item = Nautilus.MenuItem(
            name='NautilusPython::ShareItem',
            label=self.strings.get('SHARE_MENU_TITLE', 'Share...'))
        item.connect("activate", self.context_menu_action, 'SHARE', filename)
        menu.append_item(item)

        # Add permalink menu options, but hide these options for older clients
        # that don't have these actions.
        if 'COPY_PRIVATE_LINK_MENU_TITLE' in self.strings:
            item_copyprivatelink = Nautilus.MenuItem(
                name='CopyPrivateLink', label=self.strings.get('COPY_PRIVATE_LINK_MENU_TITLE', 'Copy private link to clipboard'))
            item_copyprivatelink.connect("activate", self.context_menu_action, 'COPY_PRIVATE_LINK', filename)
            menu.append_item(item_copyprivatelink)

        if 'EMAIL_PRIVATE_LINK_MENU_TITLE' in self.strings:
            item_emailprivatelink = Nautilus.MenuItem(
                name='EmailPrivateLink', label=self.strings.get('EMAIL_PRIVATE_LINK_MENU_TITLE', 'Send private link by email...'))
            item_emailprivatelink.connect("activate", self.context_menu_action, 'EMAIL_PRIVATE_LINK', filename)
            menu.append_item(item_emailprivatelink)

        return [item_nextcloud]


    def context_menu_action(self, menu, action, filename):
        # print("Context menu: " + action + ' ' + filename)
        socketConnect.sendCommand(action + ":" + filename + "\n")


class SyncStateExtension_Nextcloud(GObject.GObject, Nautilus.InfoProvider):
    def __init__(self):
        GObject.GObject.__init__(self)

        socketConnect.nautilusVFSFile_table = {}
        socketConnect.addListener(self.handle_commands)

    def find_item_for_file(self, path):
        if path in socketConnect.nautilusVFSFile_table:
            return socketConnect.nautilusVFSFile_table[path]
        else:
            return None

    def askForOverlay(self, file):
        # print("Asking for overlay for "+file)  # For debug only
        if os.path.isdir(file):
            folderStatus = socketConnect.sendCommand("RETRIEVE_FOLDER_STATUS:"+file+"\n");

        if os.path.isfile(file):
            fileStatus = socketConnect.sendCommand("RETRIEVE_FILE_STATUS:"+file+"\n");

    def invalidate_items_underneath(self, path):
        update_items = []
        if not socketConnect.nautilusVFSFile_table:
            self.askForOverlay(path)
        else:
            for p in socketConnect.nautilusVFSFile_table:
                if p == path or p.startswith(path):
                    item = socketConnect.nautilusVFSFile_table[p]['item']
                    update_items.append(p)

            for path1 in update_items:
                socketConnect.nautilusVFSFile_table[path1]['item'].invalidate_extension_info()

    # Handles a single line of server response and sets the emblem
    def handle_commands(self, action, args):
        # file = args[0]  # For debug only
        # print("Action for " + file + ": " + args[0])  # For debug only
        if action == 'STATUS':
            newState = args[0]
            filename = ':'.join(args[1:])

            itemStore = self.find_item_for_file(filename)
            if itemStore:
                if( not itemStore['state'] or newState != itemStore['state'] ):
                    item = itemStore['item']

                    # print("Setting emblem on " + filename + "<>" + emblem + "<>")  # For debug only

                    # If an emblem is already set for this item, we need to
                    # clear the existing extension info before setting a new one.
                    #
                    # That will also trigger a new call to
                    # update_file_info for this item! That's why we set
                    # skipNextUpdate to True: we don't want to pull the
                    # current data from the client after getting a push
                    # notification.
                    invalidate = itemStore['state'] != None
                    if invalidate:
                        item.invalidate_extension_info()
                    self.set_emblem(item, newState)

                    socketConnect.nautilusVFSFile_table[filename] = {
                        'item': item,
                        'state': newState,
                        'skipNextUpdate': invalidate }

        elif action == 'UPDATE_VIEW':
            # Search all items underneath this path and invalidate them
            if args[0] in socketConnect.registered_paths:
                self.invalidate_items_underneath(args[0])

        elif action == 'REGISTER_PATH':
            self.invalidate_items_underneath(args[0])
        elif action == 'UNREGISTER_PATH':
            self.invalidate_items_underneath(args[0])

    def set_emblem(self, item, state):
        Emblems = { 'OK'        : [appname + '_ok'],
                    'SYNC'      : [appname + '_sync'],
                    'NEW'       : [appname + '_sync'],
                    'IGNORE'    : [appname + '_warn'],
                    'ERROR'     : [appname + '_error'],
                    'OK+SWM'    : [appname + '_a_shared', appname + '_ok'],
                    'SYNC+SWM'  : [appname + '_a_shared', appname + '_sync'],
                    'NEW+SWM'   : [appname + '_a_shared', appname + '_sync'],
                    'IGNORE+SWM': [appname + '_a_shared', appname + '_warn'],
                    'ERROR+SWM' : [appname + '_a_shared', appname + '_error'],
                    'NOP'       : ''
                  }

        emblem = 'NOP' # Show nothing if no emblem is defined.
        if state in Emblems:
            for emblem in Emblems[state]:
                item.add_emblem(emblem)

    def update_file_info(self, item):
        if item.get_uri_scheme() != 'file':
            return

        filename = get_local_path(item.get_uri())
        filename = os.path.realpath(filename)
        if item.is_directory():
            filename += os.sep

        inScope = False
        for reg_path in socketConnect.registered_paths:
            if filename.startswith(reg_path):
                inScope = True
                break

        if not inScope:
            return

        # Ask for the current state from the client -- unless this update was
        # triggered by receiving a STATUS message from the client in the first
        # place.
        itemStore = self.find_item_for_file(filename)
        if itemStore and itemStore['skipNextUpdate'] and itemStore['state']:
            itemStore['skipNextUpdate'] = False
            itemStore['item'] = item
            self.set_emblem(item, itemStore['state'])
        else:
            socketConnect.nautilusVFSFile_table[filename] = {
                'item': item,
                'state': None,
                'skipNextUpdate': False }

            # item.add_string_attribute('share_state', "share state")  # ?
            self.askForOverlay(filename)
