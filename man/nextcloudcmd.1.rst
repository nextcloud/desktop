:orphan:

nextcloudcmd(1)
—————

SYNOPSIS
========
*nextcloudcmd* [`OPTIONS`...] sourcedir nextcloudurl

DESCRIPTION
===========
nextcloudcmd is the command line tool used for the nextCloud file synchronization
desktop utility.

Contrary to the :manpage:`nextcloud(1)` GUI client, `nextcloudcmd` only performs
a single sync run and then exits. In so doing, `nextcloudcmd` replaces the
`ocsync` binary used for the same purpose in earlier releases.

A *sync run* synchronizes a single local directory using a WebDAV share on a
remote nextCloud server.

To invoke the command line client, provide the local and the remote repository:
The first parameter is the local directory. The second parameter is
the server URL.

.. note:: Prior to the 1.6 release of nextcloudcmd, the tool only accepted
   ``owncloud://`` or ``ownclouds://`` in place of ``http://`` and ``https://`` as
   a scheme. See ``Examples`` for details.

OPTIONS
=======
``--path``
       Overrides default remote root folder to a specific subfolder on the server(e.g.: /Documents would sync the Documents subfolder on the server)

``—user``, ``-u`` ``[user]``
       Use ``user`` as the login name.

``—password``, ``-p`` ``[password]``
       Use ``password`` as the password.

``-n``
       Use ``netrc (5)`` for login.

``—non-interactive``
       Do not prompt for questions.

``—silent``, ``—s``
       Inhibits verbose log output.

``—trust``
       Trust any SSL certificate, including invalid ones.

``—httpproxy  http://[user@pass:]<server>:<port>``
      Uses ``server`` as HTTP proxy.

``—exclude [file]``
      Exclude list file

``—unsyncedfolders [file]``
      File containing the list of unsynced folders (selective sync)

``—max-sync-retries [n]``
      Retries maximum n times (defaults to 3)

``-h``
      Sync hidden files,do not ignore them

Example
=======
To synchronize the nextCloud directory ``Music`` to the local directory ``media/music``
through a proxy listening on port ``8080`` on the gateway machine ``192.168.178.1``,
the command line would be::

  $ nextcloudcmd —httpproxy http://192.168.178.1:8080 --path /Music \
                $HOME/media/music \
                https://server/nextcloud

``nextcloudcmd`` will enquire user name and password, unless they have
been specified on the command line or ``-n`` (see `netrc(5)`) has been passed.

Using the legacy scheme, it would be::

  $ nextcloudcmd —httpproxy http://192.168.178.1:8080 --path /Music \
                $HOME/media/music \
                ownclouds://server/nextcloud


BUGS
====
Please report bugs at https://github.com/nextcloud/client/issues.

SEE ALSO
========
:manpage:`nextcloud(1)`
