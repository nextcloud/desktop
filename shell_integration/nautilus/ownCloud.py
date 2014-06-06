import os
import urllib
import socket

from gi.repository import GObject, Nautilus

class ownCloudExtension(GObject.GObject, Nautilus.ColumnProvider, Nautilus.InfoProvider):
    
    nautilusVFSFile_table = {}
	    
    def __init__(self):
	self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
	self.sock.connect("/home/kf/.local/share/data/ownCloud/socket")
	self.sock.settimeout(15)
	
	GObject.io_add_watch(self.sock, GObject.IO_IN, self.handle_notify)
	
    def sendCommand(self, cmd):
	self.sock.send(cmd)

    def find_item_for_file( self, path ):
	return self.nautilusVFSFile_table[path]
    
    def callback_update_file( self, path ):
	print "Got an update callback for " + path
	
    def askForOverlay(self, file):
        if os.path.isdir(file):
            folderStatus = self.sendCommand("RETRIEVE_FOLDER_STATUS:"+file+"\n");
            
        if os.path.isfile(file):
            fileStatus = self.sendCommand("RETRIEVE_FILE_STATUS:"+file+"\n");

    # Handles a single line of server respoonse and sets the emblem
    def handle_server_response(self, l):
        Emblems = { 'NOP': '',
                    'NEED_SYNC':'view-refresh',
                    'OK': 'dialog-ok' }
    
        parts = l.split(':')
        if len(parts) > 2:
            if parts[0] == 'STATUS':
                emblem = Emblems[parts[1]]
            elif parts[0] == 'BROADCAST':
                emblem = Emblems[parts[1]]
            else:
                print "We got unknown status " + parts[0]
                
            if emblem:
                item = self.nautilusVFSFile_table[parts[2]]
                if item:
                    item.set_emblem(emblem)

    # notify is the raw answer from the socket
    def handle_notify(self, source, condition):
	print "T "
	data = source.recv(1024)
	if len(data) > 0:
	    for l in data.split('\n'):
	        self.handle_server_response( l )
	        
	return True # run again
	    
    def get_local_path(self, path):
        return path.replace("file://", "")

    def get_columns(self):
        return Nautilus.Column(name="NautilusPython::share_state_column",
                               attribute="share_state",
                               label="Share State",
                               description="The ownCloud Share State"),

    def update_file_info(self, item):
        if item.get_uri_scheme() != 'file':
            return

        filename = urllib.unquote(item.get_uri()[7:])

	self.nautilusVFSFile_table[filename] = item
	
	print "XXX " + filename
        item.add_string_attribute('share_state', "share state")
        self.askForOverlay(filename)
