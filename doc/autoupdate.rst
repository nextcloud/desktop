=====================
The Automatic Updater
=====================

The Automatic Updater ensures that you always have the
latest features and bug fixes for your Nextcloud synchronization client.

The Automatic Updater updates only on macOS and Windows computers; Linux
users only need to use their normal package managers. However, on Linux systems
the Updater will check for updates and notify you when a new version is
available.

Basic Workflow
--------------

The following sections describe how to use the Automatic Updater on different
operating systems.

Windows
^^^^^^^

The Nextcloud client checks for updates and downloads them when available. You
can view the update status under ``Settings -> General -> Updates`` in the
Nextcloud client.

If an update is available, and has been successfully downloaded, the Nextcloud
client starts a silent update prior to its next launch and then restarts
itself. Should the silent update fail, the client offers a manual download.

.. note:: Administrative privileges are required to perform the update.

macOS
^^^^^

The macOS client has an autoupdater which uses the Sparkle framework.
This autoupdater is bundled into the client App Bundle and checks for updates
on launch, notifying you if an update is available. This will present a pop-up
that can let you automatically download and install the latest client update
with one click.

In versions of the client where the Sparkle-based autoupdater is not bundled,
a clickable notification will appear informing of an update being available.
Upon clicking on said notification, the download page for the latest version
of the client will be opened in the system's web browser.

Like on other systems, you can view the update status under
``Settings -> General -> Updates`` in the Nextcloud client.

Linux
^^^^^

Linux distributions provide their own update tools, so Nextcloud clients that use
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
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Users may disable automatic updates by adding this line to the [General]
section of their ``nextcloud.cfg`` files::

 skipUpdateCheck=true

Windows administrators have more options for preventing automatic updates in
Windows environments by using one of two methods. The first method allows users
to override the automatic update check mechanism, whereas the second method
prevents any manual overrides.

To prevent automatic updates, but allow manual overrides:

1. Edit these Registry keys:

    a. (32-bit-Windows) ``HKEY_LOCAL_MACHINE\Software\Nextcloud\Nextcloud``
    b. (64-bit-Windows) ``HKEY_LOCAL_MACHINE\Software\Wow6432Node\Nextcloud\Nextcloud``

2. Add the key ``skipUpdateCheck`` (of type DWORD).

3. Specify a value of ``1`` to the machine.

To manually override this key, use the same value in ``HKEY_CURRENT_USER``.

To prevent automatic updates and disallow manual overrides:

.. note:: This is the preferred method of controlling the updater behavior using
   Group Policies.

1. Edit this Registry key:

    ``HKEY_LOCAL_MACHINE\Software\Policies\Nextcloud GmbH\Nextcloud``

2. Add the key ``skipUpdateCheck`` (of type DWORD).

3. Specify a value of ``1`` to the machine.

.. note:: branded clients have different key names


Preventing Automatic Updates in Linux Environments
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Because the Linux client does not provide automatic updating functionality, there is no
need to remove the automatic-update check.  However, if you want to disable it edit your desktop
client configuration file, ``$HOME/.config/Nextcloud/nextcloud.cfg``.
Add this line to the [General] section::

    skipUpdateCheck=true
