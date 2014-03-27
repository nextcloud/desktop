FAQ
===

Some files are continuously uploaded to the server even when they are not modified
----------------------------------------------------------------------------------

It is possible that another program is changing the modification date of the file.

If the file is a ``.eml`` file, Windows automatically change all file all the time unless you remove
``\HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers`` from
the windows registry.
See http://petersteier.wordpress.com/2011/10/22/windows-indexer-changes-modification-dates-of-eml-files/




