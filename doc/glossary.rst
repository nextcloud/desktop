Glossary
========

.. glossary::
   :sorted:

   Nextcloud Sync Client
   Nextcloud Client
     Name of the official Nextcloud syncing client for desktop, which runs on
     Windows, Mac OS X and Linux. It uses the CSync sync engine for 
     synchronization with the Nextcloud server.

   Nextcloud Server
     The server counter part of Nextcloud Client as provided by the Nextcloud
     community.

   mtime
   modification time
   file modification time
     File property used to determine whether the servers' or the clients' file
     is more recent. Used only when no sync database exists and files already
     exist in the client directory.

   unique id
   ETag
     ID assigned to every file and submitted
     via the HTTP ``Etag``. Used to check if files on client and server have
     changed.
