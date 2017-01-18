Appendix B: History and Architecture
====================================

.. index:: architecture

ownCloud provides desktop sync clients to synchronize the contents of local
directories from computers, tablets, and handheld devices to the ownCloud
server.

Synchronization is accomplished using csync_, a bidirectional file
synchronizing tool that provides both a command line client as well as a
library. A special module for csync was written to synchronize with the
ownCloud built-in WebDAV server.

The ownCloud Client software is written in C++ using the `Qt Framework`_. As a
result, the ownCloud Client runs on Linux, Windows, and MacOS.

.. _csync: http://www.csync.org
.. _`Qt Framework`: http://www.qt-project.org

The Synchronization Process
---------------------------

The process of synchronization keeps files in two separate repositories the 
same. When synchronized:

- If a file is added to one repository it is copied to the other synchronized repository.
- When a file is changed in one repository, the change is propagated to any other
  synchronized repository.
- If a file is deleted in one repository, it is deleted in any other.

It is important to note that the ownCloud synchronization process does not use
a typical client/server system where the server is always master.  This is a
major difference between the ownCloud synchronization process and other systems
like a file backup, where only changes to files or folders and the addition of
new files are propagated, but these files and folders are never deleted unless
explicitly deleted in the backup.

During synchronization, the ownCloud Client checks both repositories for
changes frequently. This process is referred to as a *sync run*. In between
sync runs, the local repository is monitored by a file system monitoring
process that starts a sync run immediately if something was edited, added, or
removed.

Synchronization by Time versus ETag
-----------------------------------
.. index:: time stamps, file times, etag, unique id

Until the release of ownCloud 4.5 and ownCloud Client 1.1, the ownCloud
synchronization process employed a single file property -- the file modification
time -- to decide which file was newer and needed to be synchronized to the
other repository.

The *modification timestamp* is part of the files metadata. It is available on
every relevant filesystem and is the typical indicator for a file change.
Modification timestamps do not require special action to create, and have a
general meaning. One design goal of csync is to not require a special server
component. This design goal is why csync was chosen as the backend component.

To compare the modification times of two files from different systems, csync
must operate on the same base. Before ownCloud Client version 1.1.0, csync
required both device repositories to run on the exact same time.  This
requirement was achieved through the use of enterprise standard `NTP time
synchronization`_ on all machines.

Because this timing strategy is rather fragile without the use of NTP, ownCloud
4.5 introduced a unique number (for each file?) that changes whenever the file
changes. Although this number is a unique value, it is not a hash of the file.
Instead, it is a randomly chosen number, that is transmitted in the Etag_
field. Because the file number changes if the file changes, its use is
guaranteed to determine if one of the files has changed and, thereby, launching
a synchronization process.

.. note:: ownCloud Client release 1.1 and later requires file ID capabilities
   on the ownCloud server.  Servers that run with release earlier than 4.5.0 do
   not support using the file ID functionality.

Before the 1.3.0 release of the Desktop Client, the synchronization process
might create false conflict files if time deviates. Original and changed files
conflict only in their timestamp, but not in their content. This behavior was
changed to employ a binary check if files differ.

Like files, directories also hold a unique ID that changes whenever one of the
contained files or directories is modified. Because this is a recursive
process, it significantly reduces the effort required for a synchronization
cycle, because the client only analyzes directories with a modified ID.


The following table outlines the different synchronization methods used,
depending on server/client combination:

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

We strongly recommend using ownCloud Server release 4.5 or later when using
ownCloud Client 1.1 or later. Using an incompatible time stamp-based
synchronization mechanism can lead to data loss in rare cases, especially when
multiple clients are involved and one utilizes a non-synchronized NTP time.

.. _`NTP time synchronization`: http://en.wikipedia.org/wiki/Network_Time_Protocol
.. _Etag: http://en.wikipedia.org/wiki/HTTP_ETag

Comparison and Conflict Cases
-----------------------------

As mentioned above, during a *sync run* the client must first detect if one of
the two repositories have changed files. On the local repository, the client
traverses the file tree and compares the modification time of each file with an
expected value stored in its database. If the value is not the same, the client
determines that the file has been modified in the local repository.

.. note:: On the local side, the modification time is a good attribute to use for 
   detecting changes, because
   the value does not depend on time shifts and such.

For the remote (that is, ownCloud server) repository, the client compares the
ETag of each file with its expected value. Again, the expected ETag value is
queried from the client database. If the ETag is the same, the file has not
changed and no synchronization occurs.

In the event a file has changed on both the local and the remote repository
since the last sync run, it can not easily be decided which version of the file
is the one that should be used. However, changes to any side will not be lost.  Instead,
a *conflict case* is created. The client resolves this conflict by creating a
conflict file of the older of the two files and saving the newer file under the
original file name. Conflict files are always created on the client and never
on the server. The conflict file uses the same name as the original file, but
is appended with the timestamp of the conflict detection.


.. _ignored-files-label:

Ignored Files
-------------

The ownCloud Client supports the ability to exclude or ignore certain files
from the synchronization process. Some system wide file patterns that are used
to exclude or ignore files are included with the client by default and the
ownCloud Client provides the ability to add custom patterns.

By default, the ownCloud Client ignores the following files:

* Files matched by one of the patterns defined in the Ignored Files Editor
* Files containing characters that do not work on certain file systems ``(`\, /, :, ?, *, ", >, <, |`)``.
* Files starting with ``._sync_xxxxxxx.db`` and the old format ``.csync_journal.db``, 
as these files are reserved for journalling.

If a pattern selected using a checkbox in the `ignoredFilesEditor-label` (or if
a line in the exclude file starts with the character ``]`` directly followed by
the file pattern), files matching the pattern are considered *fleeting meta
data*. These files are ignored and *removed* by the client if found in the
synchronized folder. This is suitable for meta files created by some
applications that have no sustainable meaning.

If a pattern ends with the forwardslash (``/``) character, only directories are
matched. The pattern is only applied for directory components of filenames
selected using the checkbox.

To match filenames against the exclude patterns, the unix standard C library
function fnmatch is used. This process checks the filename against the
specified pattern using standard shell wildcard pattern matching. For more
information, please refer to `The opengroup website
<http://pubs.opengroup.org/onlinepubs/009695399/utilities/xcu_chap02.html#tag_02_13_01>`_.

The path that is checked is the relative path under the sync root directory.

**Pattern and File Match Examples:**

+-----------+------------------------------+
| Pattern   | File Matches                 |
+===========+==============================+
| ``~$*``   | ``~$foo``, ``~$example.doc`` |
+-----------+------------------------------+
| ``fl?p``  | ``flip``, ``flap``           |
+-----------+------------------------------+
| ``moo/``  | ``map/moo/``, ``moo/``       |
+-----------+------------------------------+


The Sync Journal
----------------

The client stores the ETag number in a per-directory database, called the
*journal*.  This database is a hidden file contained in the directory to be
synchronized.

If the journal database is removed, the ownCloud Client CSync backend rebuilds
the database by comparing the files and their modification times. This process
ensures that both server and client are synchronized using the appropriate NTP
time before restarting the client following a database removal.

Custom WebDAV Properties
------------------------

In the communication between client and server a couple of custom WebDAV properties
were introduced. They are either needed for sync functionality or help have a positive
effect on synchronization performance.

This chapter describes additional xml elements which the server returns in response
to a successful PROPFIND request on a file or directory. The elements are returned in
the namespace oc.

Server Side  Permissions
------------------------

The XML element ``<oc:permissions>`` represents the permission- and sharing state of the
item. It is a list of characters, and each of the chars has a meaning as outlined
in the table below:

+----+----------------+-------------------------------------------+
|Code|   Resource     |  Description                              |
+----+----------------+-------------------------------------------+
| S  | File or Folder | is shared                                 |
+----+----------------+-------------------------------------------+
| R  | File or Folder | can share (includes reshare)              |
+----+----------------+-------------------------------------------+
| M  | File or Folder | is mounted (like on DropBox, Samba, etc.) |
+----+----------------+-------------------------------------------+
| W  | File           | can write file                            |
+----+----------------+-------------------------------------------+
| C  | Folder         |can create file in folder                  |
+----+----------------+-------------------------------------------+
| K  | Folder         | can create folder (mkdir)                 |
+----+----------------+-------------------------------------------+
| D  | File or Folder |can delete file or folder                  |
+----+----------------+-------------------------------------------+
| N  | File or Folder | can rename file or folder                 |
+----+----------------+-------------------------------------------+
| V  | File or Folder | can move file or folder                   |
+----+----------------+-------------------------------------------+


Example:

  <oc:permissions>RDNVCK</oc:permissions>

File- or Directory Size
-----------------------

The XML element ``<oc:size>`` represents the file- or directory size in bytes. For
directories, the size of the whole file tree underneath the directory is accumulated.

Example:

  <oc:size>2429176697</oc:size>

FileID
------

The XML element ``<oc:id>`` represents the so called file ID. It is a non volatile string id
that stays constant as long as the file exists. It is not changed if the file changes or
is renamed or moved.

Example:

  <oc:id>00000020oc5cfy6qqizm</oc:id>
