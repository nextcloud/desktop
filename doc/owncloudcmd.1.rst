:orphan:

owncloudcmd(1)
--------------

SYNOPSIS
========
*owncloudcmd* [`OPTIONS`...] sourcedir owncloudurl

DESCRIPTION
===========
owncloudcmd is the command line tool used for the ownCloud file synchronization
desktop utility.  This command line tool is based on mirall.

Contrary to the :manpage:`owncloud(1)` GUI client, `owncloudcmd` only performs
a single sync run and then exits. In so doing, `owncloudcmd` replaces the
`ocsync` binary used for the same purpose in earlier releases.

A *sync run* synchronizes a single local directory using a WebDAV share on a
remote ownCloud server.

To invoke the command line client, provide the local and the remote repository:
The first parameter is the local directory. The second parameter is
the server URL.

.. note:: Prior to the 1.6 release of owncloudcmd, the tool only accepted
   ``owncloud://`` or ``ownclouds://`` in place of ``http://`` and ``https://`` as
   a scheme. See ``Examples`` for details.

OPTIONS
=======
``--confdir`` `PATH`
       Specifies the configuration directory where `csync.conf` is located.

``--silent``
       Inhibits verbose log output.

``--httpproxy  http://[user@pass:]<server>:<port>``
      Uses ``server`` as HTTP proxy.

Example
=======
To synchronize the ownCloud directory ``Music`` to the local directory ``media/music``
through a proxy listening on port ``8080`` on the gateway machine ``192.168.178.1``,
the command line would be::

  $ owncloudcmd --httpproxy http://192.168.178.1:8080 \
                $HOME/media/music \
                https://server/owncloud/remote.php/webdav/Music


Using the legacy scheme, it would be::

  $ owncloudcmd --httpproxy http://192.168.178.1:8080 \
                $HOME/media/music \
                ownclouds://server/owncloud/remote.php/webdav/Music


BUGS
====
Please report bugs at https://github.com/owncloud/mirall/issues.

SEE ALSO
========
:manpage:`owncloud(1)`

