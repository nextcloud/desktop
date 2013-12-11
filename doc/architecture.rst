Appendix B: Architecture
========================

.. index:: architecture 

The ownCloud project provides desktop sync clients to synchronize the
contents of local directories on the desktop machines to the ownCloud.

The syncing is done with csync_, a bidirectional file synchronizing tool which
provides both a command line client as well as a library. A special module for
csync was written to synchronize with ownCloud’s built-in WebDAV server.

The ownCloud sync client is based on a tool called mirall initially written by
Duncan Mac Vicar. Later Klaas Freitag joined the project and enhanced it to work
with ownCloud server.

ownCloud Client is written in C++ using the `Qt Framework`_. As a result, the
ownCloud Client runs on the three important platforms Linux, Windows and MacOS.

.. _csync: http://www.csync.org
.. _`Qt Framework`: http://www.qt-project.org

The Sync Process
----------------

First it is important to recall what syncing is: It tries to keep the files
on two repositories the same. That means if a file is added to one repository
it is going to be copied to the other repository. If a file is changed on one
repository, the change is propagated to the other repository. Also, if a file
is deleted on one side, it is deleted on the other. As a matter of fact, in
ownCloud syncing we do not have a typical client/server system where the
server is always master.

This is the major difference to other systems like a file backup where just
changes and new files are propagated but files never get deleted.

The ownCloud Client checks both repositories for changes frequently after a
certain time span. That is refered to as a sync run. In between the local
repository is monitored by a file system monitor system that starts a sync run
immediately if something was edited, added or removed.

Sync by Time versus ETag
------------------------
.. index:: time stamps, file times, etag, unique id 

Until the release of ownCloud 4.5 and ownCloud Client 1.1, ownCloud employed
a single file property to decide which file is newer and hence needs to be
synced to the other repository: the files modification time.

The *modification timestamp* is part of the files metadata. It is available on
every relevant filesystem and is the natural indicator for a file change.
Modification timestamps do not require special action to create and have
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
transmit in the Etag_ field. Since the file number is guaranteed to change if
the file changes, it can now be used to determine if one of the files has
changed.

.. note:: ownCloud Client 1.1 and newer require file ID capabilities on the
   ownCloud server, hence using them with a server earlier than 4.5.0 is
   not supported.

Before the 1.3.0 release of the client the sync process might create faux
conflict files if time deviates. The original and the conflict files only
differed in the timestamp, but not in content. This behaviour was changed
towards a binary check if the files are different.

Just like files, directories also hold a unique id, which changes whenever
one of the contained files or directories gets modified. Since this is a
recursive process, it significantly reduces the effort required for a sync
cycle, because the client will only walk directories with a modified unique id.


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

Comparison and Conflict Cases
-----------------------------

In a sync run the client first has to detect if one of the two repositories have
changed files. On the local repository, the client traverses the file
tree and compares the modification time of each file with the value it was 
before. The previous value is stored in the client's database. If it is not, it
means that the file has been added to the local repository. Note that on 
the local side, the modificaton time a good attribute to detect changes because
it does not depend on time shifts and such.

For the remote (ie. ownCloud) repository, the client compares the ETag of each
file with it's previous value. Again the previous value is queried from the
database. If the ETag is still the same, the file has not changed.

In case a file has changed on both, the local and the remote repository since
the last sync run, it can not easily be decided which version of the file is
 the one that should be used. However, changes to any side must not be lost.

That is called a **conflict case**. The client solves it by creating a conflict
file of the older of the two files and save the newer one under the original
file name. Conflict files are always created on the client and never on the
server. The conflict file has the same name as the original file appended with
the timestamp of the conflict detection.


.. _ignored-files-label:

Ignored Files
-------------

ownCloud Client supports that certain files are excluded or ignored from
the synchronization. There are a couple of system wide file patterns which 
come with the client. Custom patterns can be added by the user.

ownCloud Client will ignore the following files:

* Files matched by one of the pattern in :ref:`ignoredFilesEditor-label`
* Files containing characters that do not work on certain file systems.
  Currently, these characters are: `\, :, ?, *, ", >, <, |`
* Files starting in ``.csync_journal.db*`` (reserved for journalling)

If a pattern is checkmarked in the `ignoredFilesEditor-label` (or if a line in
the exclude file starts with the character `]` directly followed
by the file pattern), files matching this pattern are considered fleeting
meta data. These files are ingored and *removed* by the client if found 
in the sync folder. This is suitable for meta files created by some 
applications that have no sustainable meaning.

If a pattern is ending with character `/` it means that only directories are
matched. The pattern is only applied for directory components of the checked
filename.

To match file names against the exclude patterns, the unix standard C
library function fnmatch is used. It checks the filename against the pattern
using standard shell wildcard pattern matching. Check `The opengroup website
<http://pubs.opengroup.org/onlinepubs/009695399/utilities/xcu_chap02.html#tag_02_13_01>`
for the gory details.

The path that is checked is the relative path unter the sync root directory.

Examples:
^^^^^^^^^
+-----------+------------------------------+
| Pattern   | Matches                      |
+===========+==============================+
| ``~$*``   | ``~$foo``, ``~$example.doc`` |
+-----------+------------------------------+
| ``fl?p``  | ``flip``, ``flap``           |
+-----------+------------------------------+
| ``moo/``  | ``map/moo/``, ``moo/``       |
+-----------+------------------------------+


The Sync Journal
----------------

The client stores the ETag number in a per-directory database,
called the journal.  It is a hidden file right in the directory
to be synced.

If the journal database gets removed, ownCloud Client's CSync backend will
rebuild the database by comparing the files and their modification times. Thus
it should be made sure that both server and client synchronized with NTP time
before restarting the client after a database removal.

Pressing ``F5`` in the Account Settings Dialog that allows to "reset" the
journal. That can be used to recreate the journal database. Use this only
if advised to do so by the developer or support staff.
