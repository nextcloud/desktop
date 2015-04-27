=====================
The Automatic Updater
=====================

The Automatic Updater ensures that you always have the 
latest features and bugfixes for your ownCloud synchronization client.

The Automatic Updater updates only on Mac OS X and Windows computers; Linux 
users only need to use their normal package managers. However, on Linux systems 
the Updater will check for updates and notify you when a new version is 
available.

.. note:: Because of various technical issues, desktop sync clients older than 
   1.7 will not be allowed to connect and sync with the ownCloud 8.1 server. It is 
   highly recommended to keep your client updated.

Basic Workflow
--------------

The following sections describe how to use the Automatic Updater on different 
operating systems.

Windows
^^^^^^^

The ownCloud client checks for updates and downloads them when available. You
can view the update status under ``Settings -> General -> Updates`` in the
ownCloud client.

If an update is available, and has been successfully downloaded, the ownCloud
client starts a silent update prior to its next launch and then restarts
itself. Should the silent update fail, the client offers a manual download.

When you upgrade from 1.7 you should restart Windows to ensure that all the new 
features in 1.8 are enabled.

.. note:: Administrative privileges are required to perform the update.

Mac OS X
^^^^^^^^

If a new update is available, the ownCloud client initializes a pop-up dialog
to alert you of the update and requesting that you update to the latest
version. Due to their use of the Sparkle frameworks, this is the default
process for Mac OS X applications.

Linux
^^^^^

Linux distributions provide their own update tools, so ownCloud clients that use
the Linux operating system do not perform any updates on their own. The client 
will inform you (``Settings -> General -> Updates``) when an update is 
available.


Preventing Automatic Updates
----------------------------

In controlled environments, such as companies or universities, you might not
want to enable the auto-update mechanism, as it interferes with controlled
deployment tools and policies. To address this case, it is possible to disable
the auto-updater entirely.  The following sections describe how to disable the
auto-update mechanism for different operating systems.

Preventing Automatic Updates in Windows Environments
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

You can prevent automatic updates from occuring in Windows environments using
one of two methods.  The first method allows users to override the automatic
update check mechanism whereas the second method prevents any manual overrides.

To prevent automatic updates, but allow manual overrides:

1. Edit these Registry keys:

    a. (32-bit) ``HKEY_LOCAL_MACHINE\Software\ownCloud\ownCloud``
    b. (64-bit) ``HKEY_LOCAL_MACHINE\Software\Wow6432Node\ownCloud\ownCloud``

2. Add the key ``skipUpdateCheck`` (of type DWORD).

3. Specify a value of ``1`` to the machine.

To manually override this key, use the same value in ``HKEY_CURRENT_USER``.

To prevent automatic updates and disallow manual overrides:

.. note::This is the preferred method of controlling the updater behavior using 
   Group Policies.

1. Migrate to the following directory::

	HKEY_LOCAL_MACHINE\Software\Policies\ownCloud\ownCloud

2. Add the key ``skipUpdateCheck`` (of type DWORD).

3. Specify a value of ``1`` to the machine.


Preventing Automatic Updates in Mac OS X Environments
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

You can disable the automatic update mechanism in Mac OS X operating systems
using the system-wide ``.plist`` file.  To access this file:

1. Go to this directory::

 	/Library/Preferences/

 2. Locate and open the following file::

 	com.owncloud.desktopclient.plist

3. Add a new root level item of type ``bool``.

4. Name the item ``skipUpdateCheck``.

5. Set the item to ``true``.

Alternatively, you can copy the file
``owncloud.app/Contents/Resources/deny_autoupdate_com.owncloud.desktopclient.plist``
to ``/Library/Preferences/com.owncloud.desktopclient.plist``.

Preventing Automatic Updates in Linux Environments
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Because the Linux client does not provide automatic updating functionality, there is no
need to remove the automatic-update check.  However, if you want to disable it edit your desktop
client configuration file, ``$HOME/.local/share/data/ownCloud/owncloud.cfg``. Add these lines:

    [General]
    skipUpdateCheck=true

