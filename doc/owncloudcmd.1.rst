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

OPTIONS
=======
``--confdir`` `PATH`
       The configuration dir where `csync.conf` is located

BUGS
====
Please report bugs at https://github.com/owncloud/mirall/issues.

SEE ALSO
========
:manpage:`owncloud(1)`

