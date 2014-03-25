The ownCloud Client packages come with a command line client which
can be used to synchronize ownCloud files to client machines. The
command line client is called ``owncloudcmd``.

owncloudcmd performs a single sync run and then exits.
That means that it processes the differences between client- and
server directory and propagates the files to get both repositories
on the same status. Contrary to the GUI based client, it does not
repeat syncs on its own. It does also not monitor for file system
changes.

To invoke the command line client, the user has to provide the local
and the remote repository urls::

  owncloudcmd [OPTIONS...] sourcedir owncloudurl

where ``sourcedir`` is the local directory and ``owncloudurl`` is
the server url.

.. note:: Prior to 1.6, the tool only accepted ``owncloud://`` or
          ``ownclouds://`` in place of ``http://`` and ``https://``
          as a scheme. See ``Examples`` for details.

These are other comand line switches supported by owncloudcmd:

``--silent``
      Don't give verbose log output

``--confdir`` `PATH`
      Fetch or store configuration in this custom config directory

``--httpproxy  http://[user@pass:]<server>:<port>``
      Use ``server`` as HTTP proxy

Credential Handling
~~~~~~~~~~~~~~~~~~~

By default, owncloudcmd reads the client configuration and uses the credentials
of the GUI sync client. If no client was configured or to use a different user
to sync, the user password setting can be specified with the usual URL pattern,
for example::

  https://user:secret@192.168.178.2/remote.php/webdav


Example
~~~~~~~

To sync the ownCloud directory ``Music`` to the local directory ``media/music``
through a proxy listening on port ``8080`` on the gateway machine ``192.168.178.1``,
the command line would be::

  $ owncloudcmd --httpproxy http://192.168.178.1:8080 \
                $HOME/media/music \
                https://server/owncloud/remote.php/webdav/Music


Using the legacy scheme, it would look like this::

  $ owncloudcmd --httpproxy http://192.168.178.1:8080 \
                $HOME/media/music \
                ownclouds://server/owncloud/remote.php/webdav/Music


