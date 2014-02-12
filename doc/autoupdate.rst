The Automatic Updater
=====================

To ensure you're always using the latest version of ownCloud Client, an
auto-update mechanism has been added in Version 1.5.1. It will ensure
that will automatically profit from the latest features and bugfixes.

The updater works differently depending on the operating system.

Basic Workflow
--------------

Windows
^^^^^^^

ownCloud client will check for updates and download the update if one
is available. You can view the status under ``Settings -> General -> Updates``.
If an update is available and has been successfully downloaded, ownCloud
Client will start a silent update prior to its next launch and then start itself.
If the silent update fails, the client offers a manual download.

.. note:: The user needs to be able to attain administrative privileges
          to successfully perform the update.

Mac OS X
^^^^^^^^

If a new update is available, ownCloud client will ask the user to update
to the latest version using a pop-up dialog. This is the default for Mac
OS X applications which use the Sparkle framework.

Linux
^^^^^

Since distributions provide their own update tool, ownCloud Client on Linux
will not perform any updates on its own. It will, however, check for the
latest version and passively notify the user (``Settings -> General -> Updates``)
if an update is available.


Preventing Auto Updates
-----------------------

In controlled environment such as companies or universities, the auto-update
mechanism might not be desired as it interferes with controlled deployment
tools and policies. In this case, it is possible to disable the auto-updater
entirely:

Windows
^^^^^^^

There are two alternative approaches:

1. In ``HKEY_LOCAL_MACHINE\Software\ownCloud\ownCloud``, add a key of type DWORD
   with the value ``skipUpdateCheck`` and the value 1 to the machine. This key
   can be manually overrideen by the same value in ``HKEY_CURRENT_USER``.

2. In ``HKEY_LOCAL_MACHINE\Software\Policies\ownCloud\ownCloud``, add a key of
   type DWORD  the value ``skipUpdateCheck`` and the value 1 to the machine.
   Setting the value here cannot be overridden by the user and is the preferred
   way to control the updater behavior via Group Policies.

Mac OS X
^^^^^^^^

You can disable the update check via the system-wide ``.plist`` file located
at ``/Library/Preferences/com.owncloud.desktopclient.plist``. Add a new root
level item of type bool and the name ``skipUpdateCheck`` and set it to ``true``.

Linux
^^^^^

Since there is no updating functionality, there is no need to remove the check.
If you want to disable the check nontheless, open a file called
``/etc/ownCloud/ownCloud.conf`` and add the following content::

 [General]
 skipUpdateCheck=true

