Architecture
============
.. index:: architecture 

The ownCloud project provides desktop sync clients to synchronize the
contents of local directories on the desktop machines to the ownCloud.

The syncing is done with csync_, a bidirectional file synchronizing tool which
provides both a command line client as well as a library. A special module for
csync was written to synchronize with ownCloud’s built-in WebDAV server.

The ownCloud sync client is based on a tool called mirall initially written by
Duncan Mac Vicar. Later Klaas Freitag joined the project and enhanced it to work
with ownCloud server. Both mirall and ownCloud Client (oCC) build from the same
source, currently hosted in the ownCloud source repo on gitorious.

oCC is written in C++ using the `Qt Framework`_. As a result oCC runs on the
three important platforms Linux, Windows and MacOS.

.. _csync: http://www.csync.org
.. _`Qt Framework`: http://www.qt-project.org

The Sync Process
----------------

First it is important to recall what syncing is. Syncing tries to keep the files
on both repositories the same. That means if a file is added to one repository
it is going to be copied to the other repository. If a file is changed on one
repository, the change is propagated to the other repository. Also, if a file
is deleted on one side, it is deleted on the other. As a matter of fact, in
ownCloud syncing we do not have a typical client/server system where the
server is always master.

This is the major difference to other systems like a file backup where just
changes and new files are propagated but files never get deleted.

Sync Direction and Strategies
-----------------------------
.. index:: time stamps, file times, etag, unique id 

Until the release of ownCloud 4.5 and ownCloud Client 1.1, ownCloud employed
a single file property to decide which file is newer and hence needs to be
synced to the other repository: the files modification time.

The *modification timestamp* is part of the files metadata. It is available on
every relevant filesystem and is the natural indicator for a file change.
modification timestamps do not require special action to create and have
a general meaning. One design goal of csync is to not require a special server
component, that’s why it was chosen as the backend component.

To compare the modification times of two files from different systems,
it is needed to operate on the same base. Before version 1.1.0,
csync requires both sides running on the exact same time, which can
be achieved through enterprise standard `NTP time synchronisation`_ on all
machines.

Since this strategy is rather fragile without NTP, ownCloud 4.5 introduced a
unique number, which changes whenever the file changes. Although it is a unique
value, it is not a hash of the file, but a randomly chosen number, which it will
transmit in the Etag_ field. The client will store this number in a
per-directory database, located in the application directory (version 1.1) or
as a hidden file right in the directory to be synced (later versions).
Since the file number is guaranteed to change if the file changes, it can now be
used to determine if one of the files has changed.

.. todo:: describe what happens if both sides change

If the per-directory database gets removed, oCC's CSync backend will fall back
to a time-stamp based sync process to rebuild the database. Thus it should be
made sure that both server and client synchronized to NTP time before
restarting the client after a database removal. If time deviates, the sync
process might create faux conflict files, which only differ in their time.
Those need to be cleaned up manually later on and will not be synced back
to the server. However, no files will get deleted in this process.
  
Just like files, directories also hold a unique id, which changes whenever
one of the contained files or directories gets modified. Since this is a
recursive process, it significantly reduces the effort required for a sync
cycle, because the client will only walk directories with a modified unique id.

.. note:: oCC 1.1 and newer require file ID capabilities on the ownCloud server,
  hence using them with a server earlier than 4.5.0 is not supported.

This table outlines the different sync methods attempted depending
on server/client combination:

.. index:: compatiblity table

+--------------------+-------------------+----------------------------+
| Server Version     | Client Version    | Sync Methods               |
+====================+===================+============================+
| 4.0.x or earlier   | 1.0.5 or earlier  | Time Stamp                 |
+--------------------+-------------------+----------------------------+
| 4.0.x or earlier   | 1.1 or later      | n/a (incompatible)         |
+--------------------+-------------------+----------------------------+
| 4.5 or later       | 1.0.5 or earlier  | Time Stamp                 |
+--------------------+-------------------+----------------------------+
| 4.5 or later       | 1.1 or later      | File ID, Time Stamp        |
+--------------------+-------------------+----------------------------+

It is highly recommended to upgrade to ownCloud 4.5 or later with ownCloud
Client 1.1 or later, since the time stamp-based sync mechanism can
lead to data loss in certain edge-cases, especially when multiple clients
are involved and one of them is not in sync with NTP time.

.. _`NTP time synchronisation`: http://en.wikipedia.org/wiki/Network_Time_Protocol
.. _Etag: http://en.wikipedia.org/wiki/HTTP_ETag

