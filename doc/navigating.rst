================================
Using the Synchronization Client
================================

.. index:: navigating, usage

The Nextcloud Desktop Client remains in the background and is visible as an icon
in the system tray (Windows, KDE), menu bar (Mac OS X), or notification area
(Linux).

.. figure:: images/icon.png
   :alt: Status icon, green circle and white checkmark

The status indicator uses icons to indicate the current status of your
synchronization. The green circle with the white checkmark tells you that your
synchronization is current and you are connected to your Nextcloud server.

.. figure:: images/icon-syncing.png
   :alt: Status icon, blue circle and white semi-circles

The blue icon with the white semi-circles means synchronization is in progress.

.. figure:: images/icon-paused.png
   :alt: Status icon, yellow circle and vertical parallel
    lines

The yellow icon with the parallel lines tells you your synchronization
has been paused. (Most likely by you.)

.. figure:: images/icon-offline.png
   :alt: Status icon, gray circle and three horizontal
    white dots

The gray icon with three white dots means your sync client has lost its
connection with your Nextcloud server.

.. figure:: images/icon-information.png
   :alt: Status icon, sign "!" in yellow circle

When you see a yellow circle with the sign "!" that is the informational icon,
so you should click it to see what it has to tell you.

.. figure:: images/icon-error.png
   :alt: Status icon, red circle and white x

The red circle with the white "x" indicates a configuration error, such as an
incorrect login or server URL.

Systray Icon
------------

A right-click on the systray icon opens a menu for quick access to multiple
operations.

.. figure:: images/traymenu.png
   :alt: the right-click sync client menu

This menu provides the following options:

* Open main dialog
* Paus sync/Resume sync
* Settings
* Exit Nextcloud, logging out and closing the client

A left-click on your systray icon opens the main dialog of the desktop client.

.. figure:: images/main_dialog.png
   :alt: Main dialog

The main dialogs show recent activities, errors and server notifications.

When clicking on the main dialog and then clicking on the avatar of the user, the Settings can be opened.

Configuring Nextcloud Account Settings
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. index:: account settings, user, password, Server URL

.. figure:: images/settingsdialog.png
   :alt: Main dialog

At the top of the window are tabs for each configured sync account, and two
others for General and Network settings. On your account tabs you
have the following features:

* Connection status, showing which Nextcloud server you are connected to, and
  your Nextcloud username.
* Used and available space on the server.
* Current synchronization status.
* **Add Folder Sync Connection** button.

The little button with three dots (the overflow menu) that sits to the right of
the sync status bar offers additional options:

* Open Folder
* Choose What to Sync (This appears only when your file tree is collapsed, and
  expands the file tree)
* Pause Sync / Resume Sync
* Remove folder sync connection
* Availability (Only available if virtual files support is enabled)
* Enable virtual file support/Disable virtual file support

**Open Folder** opens your local Nextcloud sync folder.

**Pause Sync** pauses sync operations without making any changes to your
account. It will continue to update file and folder lists, without
downloading or updating files. To stop all sync activity use **Remove
Folder Sync Connection**.

.. figure:: images/general_settings_folder_context_menu.png
   :alt: Extra options for sync operations

.. note:: Nextcloud does not preserve the mtime (modification time) of
   directories, though it does update the mtimes on files. See
   `Wrong folder date when syncing
   <https://github.com/owncloud/core/issues/7009>`_ for discussion of this.

Adding New Accounts
^^^^^^^^^^^^^^^^^^^

You may configure multiple Nextcloud accounts in your desktop sync client. Simply
click the **Account** > **Add New** button on any account tab to add a new
account, and then follow the account creation wizard. The new account will
appear as a new tab in the settings dialog, where you can adjust its settings at
any time. Use **Account** > **Remove** to delete accounts.

File Manager Overlay Icons
--------------------------

The Nextcloud sync client provides overlay icons, in addition to the normal file
type icons, for your system file manager (Explorer on Windows, Finder on Mac and
Nautilus on Linux) to indicate the sync status of your Nextcloud files.

The overlay icons are similar to the systray icons introduced above. They
behave differently on files and directories according to sync status
and errors.

The overlay icon of an individual file indicates its current sync state. If the
file is in sync with the server version, it displays a green checkmark.

If the file is ignored from syncing, for example because it is on your
exclude list, or because it is a symbolic link, it displays a warning icon.

If there is a sync error, or the file is blacklisted, it displays an
eye-catching red X.

If the file is waiting to be synced, or is currently syncing, the overlay
icon displays a blue cycling icon.

When the client is offline, no icons are shown to reflect that the
folder is currently out of sync and no changes are synced to the server.

The overlay icon of a synced directory indicates the status of the files in the
directory. If there are any sync errors, the directory is marked with a warning
icon.

If a directory includes ignored files that are marked with warning icons
that does not change the status of the parent directories.

Set the user status
-------------------

If you have the user status app installed on your Nextcloud server,
you can set your user status from the desktop client. To do so, open
the main dialog. Then click on your avatar and then click on the three
dots. In the menu that opens click on **Set status**.

.. figure:: images/user_status_selector_main_dialog.png
   :alt: Open user status dialog from main dialog.

In the dialog that opens, you can set your online status if
you click on either **Online**, **Away**, **Do not disturb** or
**Invisible**. You can also set a custom status message with the text
field below or choose one of the predefined status messages below. It
is also possible to set a custom emoji if you click on the button with
the emoji beside the text input field. The last thing you might want
to set is when your user status should be cleared. You can choose the
period after which the user status will be cleared by clicking on the
button on the left hand side of the text **Clear status message after**.

.. figure:: images/user_status-selector_dialog.png
   :alt: Dialog to set user status.

If you are happy with the status you have created you can enable this
status with the button **Set status message**. If you had already a
status set, you can clear the status by clicking the cutton **Clear
status message**.

Sharing From Your Desktop
-------------------------

The Nextcloud desktop sync client integrates with your file manager. Finder on
macOS and Explorer on Windows. Linux users must install an additional package 
depending on the used file manager. Available are e.g. ``nautilus-nextcloud`` 
(Ubuntu/Debian), ``dolphin-nextcloud`` (Kubuntu), ``nemo-nextcloud`` and 
``caja-nextcloud``. You can create  share links, and share with internal 
Nextcloud users the same way as in your Nextcloud Web interface.

.. figure:: images/mac-share.png
   :alt: Sync client integration in Windows Explorer.

In you file explorer, click on a file and in the context menu go to
**Nextcloud** and then lick on **Share options** to bring up the Share
dialog.

.. figure:: images/share_context_menu.png
   :alt: Sharing from Windows Explorer.

From this dialog you can share a file.

.. figure:: images/share_dialog.png
   :alt: Share dialog


General Window
--------------

The General window has configuration options such as **Launch on System
Startup**, **Use Monochrome Icons**, and **Show Desktop Notifications**. This
is where you will find the **Edit Ignored Files** button, to launch the ignored
files editor, and **Ask confirmation before downloading
folders larger than [folder size]**.

.. figure:: images/settings_general.png
   :alt: General window contains configuration options.

Using the Network Window
------------------------

.. index:: proxy settings, SOCKS, bandwidth, throttling, limiting

The Network settings window enables you to define network proxy settings, and
also to limit download and upload bandwidth.

.. figure:: images/settings_network.png

.. _usingIgnoredFilesEditor-label:

Using the Ignored Files Editor
------------------------------

.. index:: ignored files, exclude files, pattern

You might have some local files or directories that you do not want to backup
and store on the server. To identify and exclude these files or directories, you
can use the *Ignored Files Editor* (General tab.)

.. figure:: images/ignored_files_editor.png

For your convenience, the editor is pre-populated with a default list of
typical
ignore patterns. These patterns are contained in a system file (typically
``sync-exclude.lst``) located in the Nextcloud Client application directory. You
cannot modify these pre-populated patterns directly from the editor. However,
if
necessary, you can hover over any pattern in the list to show the path and
filename associated with that pattern, locate the file, and edit the
``sync-exclude.lst`` file.

.. note:: Modifying the global exclude definition file might render the client
   unusable or result in undesired behavior.

Each line in the editor contains an ignore pattern string. When creating custom
patterns, in addition to being able to use normal characters to define an
ignore pattern, you can use wildcards characters for matching values.  As an
example, you can use an asterisk (``*``) to identify an arbitrary number of
characters or a question mark (``?``) to identify a single character.

Patterns that end with a slash character (``/``) are applied to only directory
components of the path being checked.

.. note:: Custom entries are currently not validated for syntactical
   correctness by the editor, so you will not see any warnings for bad
   syntax. If your synchronization does not work as you expected, check your
   syntax.

Each pattern string in the list is followed by a checkbox. When the check box
contains a check mark, in addition to ignoring the file or directory component
matched by the pattern, any matched files are also deemed "fleeting metadata"
and removed by the client.

In addition to excluding files and directories that use patterns defined in
this list:

- The Nextcloud Client always excludes files containing characters that cannot
  be synchronized to other file systems.

- Files are removed that cause individual errors three times during a
  synchronization. However, the client provides the option of retrying a
  synchronization three additional times on files that produce errors.

For more detailed information see :ref:`ignored-files-label`.
