FAQ
===

**Issue:**

Some files are continuously uploaded to the server, even when they are not modified.

**Resolution:**

It is possible that another program is changing the modification date of the file.

If the file is uses the ``.eml`` extention, Windows automatically and
continually changes all files, unless you remove
``\HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers`
from the windows registry.

See http://petersteier.wordpress.com/2011/10/22/windows-indexer-changes-modification-dates-of-eml-files/ for more information.
