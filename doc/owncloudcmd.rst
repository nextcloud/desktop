The ownCloud Client packages contain a command line client, ``owncloudcmd``, that can 
be used to synchronize ownCloud files to client machines.

``owncloudcmd`` performs a single *sync run* and then exits the synchronization 
process. In this manner, ``owncloudcmd`` processes the differences between 
client and server directories and propagates the files to bring both 
repositories to the same state. Contrary to the GUI-based client, 
``owncloudcmd`` does not repeat synchronizations on its own. It also does not 
monitor for file system changes.

To invoke ``owncloudcmd``, you must provide the local and the remote repository 
URL using the following command::

  owncloudcmd [OPTIONS...] sourcedir owncloudurl

where ``sourcedir`` is the local directory and ``owncloudurl`` is
the server URL.

Other command line switches supported by ``owncloudcmd`` include the following:

``--user``, ``-u`` ``[user]``
       Use ``user`` as the login name.

``--password``, ``-p`` ``[password]``
       Use ``password`` as the password.

``-n``
       Use ``netrc (5)`` for login.

``--non-interactive``
       Do not prompt for questions.

``--silent``, ``--s``
       Inhibits verbose log output.

``--trust``
       Trust any SSL certificate, including invalid ones.

``--httpproxy  http://[user@pass:]<server>:<port>``
      Uses ``server`` as HTTP proxy.

``--nonshib``
      Uses Non Shibboleth WebDAV Authentication

``--davpath [path]``
      Overrides the WebDAV Path with ``path``

``--exclude [file]``
      Exclude list file

``--unsyncedfolders [file]``
      File containing the list of un-synced remote folders (selective sync)

``--max-sync-retries [n]``
      Retries maximum n times (defaults to 3)

``-h``
      Sync hidden files,do not ignore them

Credential Handling
~~~~~~~~~~~~~~~~~~~

``owncloudcmd`` uses the credentials of the GUI synchronization client.
If no client is configured, or if you choose to use a different user to synchronize,
you can specify the user
password setting with the usual URL pattern.  For example::

  $ owncloudcmd /home/user/my_sync_folder https://carla:secret@server/owncloud/remote.php/webdav/

To synchronize the ownCloud directory ``Music`` to the local directory
``media/music``, through a proxy listening on port ``8080``, and on a gateway
machine using IP address ``192.168.178.1``, the command line would be::

  $ owncloudcmd --httpproxy http://192.168.178.1:8080 \
                $HOME/media/music \
                https://server/owncloud/remote.php/webdav/Music

``owncloudcmd`` will prompt for the user name and password, unless they have
been specified on the command line or ``-n`` has been passed.

Exclude List
~~~~~~~~~~~~

``owncloudcmd`` requires access to an exclude list file. It must either be
installed along with ``owncloudcmd`` and thus be available in a system location,
be placed next to the binary as ``sync-exclude.lst`` or be explicitly specified
with the ``--exclude`` switch.
