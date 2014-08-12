#!/usr/bin/python3

import os
import urllib
import socket

from gi.repository import GObject, Nautilus

class ownCloudExtension(GObject.GObject, Nautilus.ColumnProvider, Nautilus.InfoProvider):
    
    nautilusVFSFile_table = {}
    registered_paths = {}
    remainder = ''

    def __init__(self):
	self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	self.sock.connect(("localhost", 33001))
	self.sock.settimeout(5)
	
	GObject.io_add_watch(self.sock, GObject.IO_IN, self.handle_notify)
	
    def sendCommand(self, cmd):
	self.sock.send(cmd)

    def find_item_for_file( self, path ):
	if path in self.nautilusVFSFile_table:
	    return self.nautilusVFSFile_table[path]
	else:
	    return None
    
    def callback_update_file( self, path ):
	print "Got an update callback for " + path
	
    def askForOverlay(self, file):
        if os.path.isdir(file):
            folderStatus = self.sendCommand("RETRIEVE_FOLDER_STATUS:"+file+"\n");
            
        if os.path.isfile(file):
            fileStatus = self.sendCommand("RETRIEVE_FILE_STATUS:"+file+"\n");

    # Handles a single line of server respoonse and sets the emblem
    def handle_server_response(self, l):
        Emblems = { 'OK'        : 'oC_ok',
		    'SYNC'      : 'oC_sync',
		    'NEW'       : 'oC_sync',
		    'IGNORE'    : 'oC_warn',
		    'ERROR'     : 'oC_error',
		    'OK+SWM'    : 'oC_ok_shared',
		    'SYNC+SWM'  : 'oC_sync_shared',
		    'NEW+SWM'   : 'oC_sync_shared',
		    'IGNORE+SWM': 'oC_warn_shared',
		    'ERROR+SWM' : 'oC_error_shared',
		    'NOP'       : 'oC_error'
		  }

        print "Server response: "+l
        parts = l.split(':')
        if len(parts) > 0:
	    action = parts[0]

	    # file = parts[1]
	    # print "Action for " + file + ": "+parts[0]
            if action == 'STATUS':
                emblem = Emblems[parts[1]]
                if emblem:
                    item = self.find_item_for_file(parts[2])
                    if item:
                        item.add_emblem(emblem)
	    elif action == 'UPDATE_VIEW':
		if parts[1] in self.registered_paths:
		    for p in self.nautilusVFSFile_table:
			if p.startswith( parts[1] ):
			    item = self.nautilusVFSFile_table[p]
			    item.invalidate_extension_info()
			    self.update_file_info(item)

	    elif action == 'REGISTER_PATH':
		self.registered_paths[parts[1]] = 1
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
	        self.handle_server_response( l )
        else:
            return False

	return True # run again
	    
    def get_local_path(self, path):
        return path.replace("file://", "")

   

    def update_file_info(self, item):
        if item.get_uri_scheme() != 'file':
            return

        filename = urllib.unquote(item.get_uri()[7:])
        if item.is_directory():
	    filename += '/'

	for reg_path in self.registered_paths:
	    if filename.startswith(reg_path):
		self.nautilusVFSFile_table[filename] = item
	
		# item.add_string_attribute('share_state', "share state")
		self.askForOverlay(filename)
		break
	    else:
		print "Not in scope:"+filename
