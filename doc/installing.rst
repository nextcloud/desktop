=============================================
Installing the Desktop Synchronization Client
=============================================

You can download the  latest version of the ownCloud Desktop Synchronization 
Client from the `ownCloud download page`_. 
There are clients for Linux, Mac OS X, and Microsoft Windows.

Installation on Mac OS X and Windows is the same as for any software 
application: download the program and then double-click it to launch the 
installation, and then follow the installation wizard. After it is installed and 
configured the sync client will automatically keep itself updated; see 
:doc:`autoupdate` for more information.

Linux users must follow the instructions on the download page to add the 
appropriate repository for their Linux distribution, install the signing key, 
and then use their package managers to install the desktop sync client. Linux 
users will also update their sync clients via package manager, and the client 
will display a notification when an update is available. 

Linux users must also have a password manager enabled, such as GNOME Keyring or
KWallet, so that the sync client can login automatically.

You will also find links to source code archives and older versions on the 
download page.

Installation Wizard
-------------------

The installation wizard takes you step-by-step through configuration options and 
account setup. First you need to enter the URL of your ownCloud server.

.. image:: images/client-1.png
   :alt: form for entering ownCloud server URL
   
Enter your ownCloud login on the next screen.

.. image:: images/client-2.png
   :alt: form for entering your ownCloud login

On the Local Folder Option screen you may sync 
all of your files on the ownCloud server, or select individual folders. The 
default local sync folder is ``ownCloud``, in your home directory. You may 
change this as well.

.. image:: images/client-3.png
   :alt: Select which remote folders to sync, and which local folder to store 
    them in.
   
When you have completed selecting your sync folders, click the Connect button 
at the bottom right. The client will attempt to connect to your ownCloud 
server, and when it is successful you'll see two buttons: one to connect to 
your ownCloud Web GUI, and one to open your local folder. It will also start 
synchronizing your files.

.. image:: images/client-4.png
   :alt: A successful server connection, showing a button to connect to your 
    Web GUI, and one to open your local ownCloud folder

Click the Finish button, and you're all done. 

.. Links
   
.. _ownCloud download page: https://owncloud.com/download/#desktop-clients
