FAQ
===

Some Files Are Continuously Uploaded to the Server, Even When They Are Not Modified.
------------------------------------------------------------------------------------

It is possible that another program is changing the modification date of the file.
If the file is uses the ``.eml`` extension, Windows automatically and
continually changes all files, unless you remove
``\HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers``
from the windows registry.
See http://petersteier.wordpress.com/2011/10/22/windows-indexer-changes-modification-dates-of-eml-files/ for more information.

Syncing Stops When Attempting To Sync Deeper Than 100 Sub-directories.
----------------------------------------------------------------------

The sync client has been intentionally limited to sync no deeper than 100
sub-directories. The hard limit exists to guard against bugs with cycles
like symbolic link loops.
When a deeply nested directory is excluded from synchronization it will be
listed with other ignored files and directories in the "Not synced" tab of
the "Activity" pane.

I Want To Move My Local Sync Folder
-----------------------------------

The Nextcloud desktop client does not provide a way to change the local sync directory. 
However, it can be done, though it is a bit unorthodox. 
Specifically, you have to:

1. Remove the existing connection which syncs to the wrong directory
2. Add a new connection which syncs to the desired directory

.. figure:: images/setup/remove.png
   :alt: Remove an existing connection

To do so, in the client UI, which you can see above, click the "**Account**" drop-down menu and then click "Remove". 
This will display a "**Confirm Account Removal**" dialog window.

.. figure:: images/setup/confirm.png
   :alt: Remove existing connection confirmation dialog

If you're sure, click "**Remove connection**".

Then, click the Account drop-down menu again, and this time click "**Add new**".

.. figure:: images/setup/wizard.png
   :alt: Replacement connection wizard

This opens the Nextcloud Connection Wizard, which you can see above, *but* with an extra option.
This option provides the ability to either: keep the existing data (synced by the previous connection) or to start a clean sync (erasing the existing data).

.. important:: 

  Be careful before choosing the "Start a clean sync" option. The old sync folder *may* contain a considerable amount of data, ranging into the gigabytes or terabytes. If it does, after the client creates the new connection, it will have to download **all** of that information again. Instead, first move or copy the old local sync folder, containing a copy of the existing files, to the new location. Then, when creating the new connection choose "*keep existing data*" instead. The Nextcloud client will check the files in the newly-added sync folder and find that they match what is on the server and not need to download anything. 

Make your choice and click "**Connect...**".
This will then step you through the Connection Wizard, just as you did when you setup the previous sync connection, but giving you the opportunity to choose a new sync directory.
