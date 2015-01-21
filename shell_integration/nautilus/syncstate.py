#!/usr/bin/python3
#
# Copyright (C) by Klaas Freitag <freitag@owncloud.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
# for more details.

import os
import urllib
import socket

from gi.repository import GObject, Nautilus

# do not touch the following line.
appname = 'ownCloud'

def get_local_path(path):
    return path.replace("file://", "")


def get_runtime_dir():
    """Returns the value of $XDG_RUNTIME_DIR, a directory path.

    If the value is not set, returns the same default as in Qt5
    """
    try:
        return os.environ['XDG_RUNTIME_DIR']
    except KeyError:
        fallback = '/tmp/runtime-' + os.environ['USER']
        return fallback

class SocketConnect(GObject.GObject):

    _connected = False
    _watch_id = 0
    _sock = None
    registered_paths = {}

    def __init__(self):
        GObject.GObject.__init__(self)

    def connectToSocketServer(self):
        do_reconnect = True

        try:
            SocketConnect._sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            postfix = "/"+appname+"/socket"
            sock_file = get_runtime_dir()+postfix
            print ("XXXX " + sock_file + " <=> " + postfix)
            if sock_file != postfix:
                try:
                    print("Socket File: "+sock_file)
                    SocketConnect._sock.connect(sock_file)
                    SocketConnect._connected = True
                    print("Setting connected to %r" % self._connected )
                    do_reconnect = False
                except Exception as e:
                    print("Could not connect to unix socket." + str(e))
            else:
                print("Sock-File not valid: "+sock_file)
        except Exception as e:
            print("Connect could not be established, try again later ")
            SocketConnect._sock.close()
        # print("Returning %r" % do_reconnect)
        return do_reconnect

    def sendCommand(self, cmd):
        if self._connected:
            try:
                SocketConnect._sock.send(cmd)
            except:
                print("Sending failed.")
                GObject.source_remove(self._watch_id)
                self._connected = False
                GObject.timeout_add(5000, self.connectToSocketServer)
        else:
            print "Cannot send, not connected!"

    def addSocketWatch(self, callback):
        self._watch_id = GObject.io_add_watch(SocketConnect._sock, GObject.IO_IN, callback)
        print "Socket watch id: "+str(self._watch_id)

class MenuExtension( Nautilus.MenuProvider, SocketConnect):
    def __init__(self):
	pass

    def get_file_items(self, window, files):
        if len(files) != 1:
	    return
        file=files[0]
        items=[]

        # internal or external file?!
        syncedFile = False
        for reg_path in self.registered_paths:
            if file.get_name().startswith(reg_path):
                syncedFile = True

        # if it is neither in a synced folder or is a directory
        if (not syncedFile and file.is_directory()):
            return items

        # create an menu item
        item = Nautilus.MenuItem(name='NautilusPython::ShareItem', label='ownCloud Share...' ,
				 tip='Share file %s through ownCloud' % file.get_name())
        item.connect("activate", self.menu_share, file)
        items.append(item)

        return items


    def menu_share(self, menu, file):
        filename = get_local_path(file.get_uri())
        print "Share file "+filename
        self.sendCommand("SHARE:"+filename+"\n")

class syncStateExtension(Nautilus.ColumnProvider, Nautilus.InfoProvider, SocketConnect):

    nautilusVFSFile_table = {}

    remainder = ''

    def find_item_for_file(self, path):
        if path in self.nautilusVFSFile_table:
            return self.nautilusVFSFile_table[path]
        else:
            return None

    def askForOverlay(self, file):
        # print("Asking for overlay for "+file)
        if os.path.isdir(file):
            folderStatus = self.sendCommand("RETRIEVE_FOLDER_STATUS:"+file+"\n");

        if os.path.isfile(file):
            fileStatus = self.sendCommand("RETRIEVE_FILE_STATUS:"+file+"\n");

    def invalidate_items_underneath(self, path):
        update_items = []
        if not self.nautilusVFSFile_table:
            self.askForOverlay(path)
        else:
            for p in self.nautilusVFSFile_table:
                if p == path or p.startswith(path):
                    item = self.nautilusVFSFile_table[p]['item']
                    update_items.append(item)

            for item in update_items:
                item.invalidate_extension_info()

    # Handles a single line of server respoonse and sets the emblem
    def handle_server_response(self, l):
        Emblems = { 'OK'        : appname +'_ok',
                    'SYNC'      : appname +'_sync',
                    'NEW'       : appname +'_sync',
                    'IGNORE'    : appname +'_warn',
                    'ERROR'     : appname +'_error',
                    'OK+SWM'    : appname +'_ok_shared',
                    'SYNC+SWM'  : appname +'_sync_shared',
                    'NEW+SWM'   : appname +'_sync_shared',
                    'IGNORE+SWM': appname +'_warn_shared',
                    'ERROR+SWM' : appname +'_error_shared',
                    'NOP'       : appname +'_error'
                  }

        print("Server response: "+l)
        parts = l.split(':')
        if len(parts) > 0:
            action = parts[0]

            # file = parts[1]
            # print "Action for " + file + ": "+parts[0]
            if action == 'STATUS':
                newState = parts[1]
                emblem = Emblems[newState]
                if emblem:
                    itemStore = self.find_item_for_file(parts[2])
                    if itemStore:
                        if( not itemStore['state'] or newState != itemStore['state'] ):
                            item = itemStore['item']
                            item.add_emblem(emblem)
                            # print "Setting emblem on " + parts[2]+ "<>"+emblem+"<>"
                            self.nautilusVFSFile_table[parts[2]] = {'item': item, 'state':newState}

            elif action == 'UPDATE_VIEW':
                # Search all items underneath this path and invalidate them
                if parts[1] in self.registered_paths:
                    self.invalidate_items_underneath(parts[1])

            elif action == 'REGISTER_PATH':
                self.registered_paths[parts[1]] = 1
                self.invalidate_items_underneath(parts[1])
            elif action == 'UNREGISTER_PATH':
                del self.registered_paths[parts[1]]
                self.invalidate_items_underneath(parts[1])

                # check if there are non pathes any more, if so, its usual
                # that mirall went away. Try reconnect.
                if not self.registered_paths:
                    self.sock.close()
                    self._connected = False
                    GObject.source_remove(self._watch_id)
                    GObject.timeout_add(5000, self.connectToSocketServer)

            else:
                # print "We got unknown action " + action
                1

    # notify is the raw answer from the socket
    def handle_notify(self, source, condition):

        data = source.recv(1024)
        # prepend the remaining data from last call
        if len(self.remainder) > 0:
            data = self.remainder+data
            self.remainder = ''

        if len(data) > 0:
            # remember the remainder for next round
            lastNL = data.rfind('\n');
            if lastNL > -1 and lastNL < len(data):
                self.remainder = data[lastNL+1:]
                data = data[:lastNL]

            for l in data.split('\n'):
                self.handle_server_response(l)
        else:
            return False

        return True # run again



    def update_file_info(self, item):
        if item.get_uri_scheme() != 'file':
            return

        filename = urllib.unquote(item.get_uri()[7:])
        if item.is_directory():
            filename += '/'

        for reg_path in self.registered_paths:
            if filename.startswith(reg_path):
                self.nautilusVFSFile_table[filename] = {'item': item, 'state':''}

                # item.add_string_attribute('share_state', "share state")
                self.askForOverlay(filename)
                break
            else:
                # print("Not in scope:"+filename)
                pass

    def __init__(self):
        self.connectToSocketServer()
        if not self._connected:
            # try again in 5 seconds - attention, logic inverted!
            print "Not yet connected, pull timer."
            GObject.timeout_add(5000, self.connectToSocketServer)
        else:
            self.addSocketWatch(self.handle_notify)
