The ownCloud Client packages contain a command line client that can be used to
synchronize ownCloud files to client machines. The command line client is
called ``owncloudcmd``.

owncloudcmd performs a single *sync run* and then exits the synchronization
process. In this manner, owncloudcmd processes the differences between client
and server directories and propagates the files to bring both repositories to
the same state. Contrary to the GUI-based client, owncloudcmd does not repeat
synchronizations on its own. It also does not monitor for file system changes.

To invoke the owncloudcmd, you must provide the local and the remote repository
urls using the following command::

  owncloudcmd [OPTIONS...] sourcedir owncloudurl

where ``sourcedir`` is the local directory and ``owncloudurl`` is
the server URL.

.. note:: Prior to the 1.6 version of owncloudcmd, the tool only accepted
   ``owncloud://`` or ``ownclouds://`` in place of ``http://`` and ``https://`` as
   a scheme. See ``Examples`` for details.

Other comand line switches supported by owncloudcmd include the following:

``--user``, ``-u`` ``[user]``
       Use ``user`` as the login name.

``--password``, ``-p`` ``[password]``
       Use ``password`` as the password.

``-n``
       Use ``netrc (5)`` for login.

``--non-interactive``
       Do not prompt for questions.

``--silent``, ``-s``
       Inhibits verbose log output.

``--trust``
       Trust any SSL certificate, including invalid ones.

``--httpproxy  http://[user@pass:]<server>:<port>``
      Uses the specified ``server`` as the HTTP proxy.

Credential Handling
~~~~~~~~~~~~~~~~~~~

By default, owncloudcmd reads the client configuration and uses the credentials
of the GUI syncrhonization client. If no client is configured, or if you choose
to use a different user to synchronize, you can specify the user password
setting with the usual URL pattern.  For example::

  https://user:secret@192.168.178.2/remote.php/webdav

Example
~~~~~~~

To synchronize the ownCloud directory ``Music`` to the local directory
``media/music``, through a proxy listening on port ``8080``, and on a gateway
machine using IP address ``192.168.178.1``, the command line would be::

  $ owncloudcmd --httpproxy http://192.168.178.1:8080 \
                $HOME/media/music \
                https://server/owncloud/remote.php/webdav/Music

``owncloudcmd`` will enquire user name and password, unless they have
been specified on the command line or ``-n`` has been passed.

Using the legacy scheme, the command line would be::

  $ owncloudcmd --httpproxy http://192.168.178.1:8080 \
                $HOME/media/music \
                ownclouds://server/owncloud/remote.php/webdav/Music


