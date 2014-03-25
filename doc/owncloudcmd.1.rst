:orphan:

owncloudcmd(1)
--------------

SYNOPSIS
========
*owncloudcmd* [`OPTIONS`...] sourcedir owncloudurl

DESCRIPTION
===========
owncloudcmd is the command line tool for the ownCloud file synchronisation
desktop utility, based on mirall.

Contrary to the :manpage:`owncloud(1)` GUI client, `owncloudcmd` will only
perform a single sync run and then exit. It thus replaces the `ocsync` binary
used for the same purpose in earlier releases.

A sync run will sync a single local directory with a WebDAV share on a
remote ownCloud server.

To invoke the command line client, provide the local and the remote repository:
The first parameter is the local directory. The second parameter is
the server URL.

.. note:: Prior to 1.6, the tool only accepted ``owncloud://`` or ``ownclouds://``
          in place of ``http://`` and ``https://`` as a scheme. See ``Examples``
          for details.

OPTIONS
=======
``--confdir`` `PATH`
       The configuration dir where `csync.conf` is located

``--silent``
       Don't give verbose log output

``--httpproxy  http://[user@pass:]<server>:<port>``
      Use ``server`` as HTTP proxy

Example
=======
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


BUGS
====
Please report bugs at https://github.com/owncloud/mirall/issues.

SEE ALSO
========
:manpage:`owncloud(1)`

