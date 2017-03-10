FAQ
===

Some files are continuously uploaded to the server, even when they are not modified.
------------------------------------------------------------------------------------

It is possible that another program is changing the modification date of the file.
If the file is uses the ``.eml`` extension, Windows automatically and
continually changes all files, unless you remove
``\HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers``
from the windows registry.
See http://petersteier.wordpress.com/2011/10/22/windows-indexer-changes-modification-dates-of-eml-files/ for more information.

Syncing breaks when attempting to sync deeper than 50 sub-directories, but the sync client does not report an error (RC=0)
--------------------------------------------------------------------------------------------------------------------------

The sync client has been intentionally limited to sync no deeper than
fifty sub-directories, to help prevent memory problems. 
Unfortunately, it, *currently*, does not report an error when this occurs. 
However, a UI notification is planned for a future release of ownCloud.

I want to move my local sync folder
-----------------------------------

The ownCloud desktop client does not provide a way to change the local sync directory. 
However, it can be done, though it is a bit unorthodox. 
Specifically, you have to:

1. Remove the existing connection which syncs to the wrong directory
2. Add a new connection which syncs to the desired directory

image:: images/setup/ownCloud-remove_existing_connection.png

To do so, in the client UI, which you can see above, click the "**Account**" drop-down menu and then click "Remove". 
This will display a "**Confirm Account Removal**" dialog window.

image:: images/setup/ownCloud-remove_existing_connection_confirmation_dialog.png

If you're sure, click "**Remove connection**".

Then, click the Account drop-down menu again, and this time click "**Add new**".

image:: images/setup/ownCloud-replacement_connection_wizard.png

This opens the ownCloud Connection Wizard, which you can see above, *but* with an extra option.
This option provides the ability to either: keep the existing data (synced by the previous connection) or to start a clean sync (erasing the existing data).

.. important:: 

  Be careful before choosing the "Start a clean sync" option. The old sync folder *may* contain a considerable amount of data, ranging into the gigabytes or terabytes. If it does, after the client creates the new connection, it will have to download **all** of that information again. Instead, first move or copy the old local sync folder, containing a copy of the existing files, to the new location. Then, when creating the new connection choose "*keep existing data*" instead. The ownCloud client will check the files in the newly-added sync folder and find that they match what is on the server and not need to download anything. 

Make your choice and click "**Connect...**".
This will then step you through the Connection Wizard, just as you did when you setup the previous sync connection, but giving you the opportunity to choose a new sync directory.

