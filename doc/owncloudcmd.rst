The ownCloud Client packages come with a command line client which
can be used to synchronize ownCloud files to client machines. The
command line client is called ``owncloudcmd``.

owncloudcmd does exactly one sync run and exits after that is finished.
That means that it processes the differences between client- and
server directory and propagates the files to get both repositories
on the same status. Other than the GUI based client, it does not
repeat that or monitors for file system changes.

To invoke the command line client, the user has to provide the local
and the remote repository urls:

``owncloudcmd <local_dir> <remote_url>``

The first parameter is the local directory. The second parameter is
the server url.

.. note:: To access the ownCloud server over SSL, the url scheme has to be ``owncluods``.
          To access it via an unencrypted http connection (not recommended) the url scheme is ``owncloud``

These are other comand line switches supported by owncloudcmd:

``--silent``: Don't give verbose log output

``--confdir <confdir>``: Fetch or store configuration in this custom config directory

``--httpproxy``: Use this http proxy. The proxy specification is ``http://<proxy>:<port>``.

**Credential Handling**

By default, owncloudcmd reads the client configuration and uses the credentials of
the GUI sync client. If no client was configured or to use a different user to sync,
the user password setting can be specified with the usual url pattern, for example ``owncloud://user:secret@192.168.178.2/remote.php/webdav``


**Example**

To sync the ownCloud directory *Music* to the local directory *media/music* through the proxy on the gateway machine 192.168.178.1, the command line would look like


``owncloudcmd --httpproxy http://192.168.178.1:8080 $HOME/media/music ownclouds://server/owncloud/remote.php/webdav/Music``
