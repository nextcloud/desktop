:orphan:

owncloudcmd(1)
--------------

SYNOPSIS
========
*owncloudcmd* [`OPTIONS`...] sourcedir owncloudurl

DESCRIPTION
===========
owncloudcmd is the command line tool used for the ownCloud file synchronization
desktop utility.

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
      File containing the list of un-synced folders (selective sync)

``--max-sync-retries [n]``
      Retries maximum n times (defaults to 3)

``-h``
      Sync hidden files,do not ignore them

Example
=======
To synchronize the ownCloud directory ``Music`` to the local directory ``media/music``
through a proxy listening on port ``8080`` on the gateway machine ``192.168.178.1``,
the command line would be::

  $ owncloudcmd --httpproxy http://192.168.178.1:8080 \
                $HOME/media/music \
                https://server/owncloud/remote.php/webdav/Music

``owncloudcmd`` will enquire user name and password, unless they have
been specified on the command line or ``-n`` (see `netrc(5)`) has been passed.

Using the legacy scheme, it would be::

  $ owncloudcmd --httpproxy http://192.168.178.1:8080 \
                $HOME/media/music \
                ownclouds://server/owncloud/remote.php/webdav/Music


BUGS
====
Please report bugs at https://github.com/owncloud/client/issues.

SEE ALSO
========
:manpage:`owncloud(1)`

