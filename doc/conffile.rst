ownCloud Client reads a configuration file.

On Linux it can be found in:
        ``$HOME/.local/share/data/ownCloud/owncloud.cfg``

On Windows it can be found in:
        ``%LOCALAPPDATA%\ownCloud\owncloud.cfg``

On Mac it can be found in:
        ``$HOME/Library/Application Support/ownCloud``


It contains settings in the ini file format known from Windows. 

.. note:: Changes here should be done carefully as wrong settings can cause disfunctionality.

.. note:: Changes may be overwritten by using ownCloud's configuration dialog.

.. note:: The new version is less precise in this regard.

These are config settings that may be changed:

``remotePollinterval`` (default: ``30000``)
        Poll time for the remote repository in milliseconds

``maxLogLines`` (default:  ``20000``)
        Maximum count of log lines shown in the log window

``remotePollinterval``
        The frequency used for polling for remote changes on the ownCloud Server.

