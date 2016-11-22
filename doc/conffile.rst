The ownCloud Client reads a configuration file.  You can locate this configuration file as follows:

On Linux distributions:
        ``$HOME/.local/share/data/ownCloud/owncloud.cfg``

On Microsoft Windows systems:
        ``%LOCALAPPDATA%\ownCloud\owncloud.cfg``

On MAC OS X systems:
        ``$HOME/Library/Application Support/ownCloud/owncloud.cfg``


The configuration file contains settings using the Microsoft Windows .ini file
format. You can overwrite changes using the ownCloud configuration dialog.

.. note:: Use caution when making changes to the ownCloud Client configuration
   file.  Incorrect settings can produce unintended results.

You can change the following configuration settings (must be under the ``[ownCloud]`` section)

- ``remotePollInterval`` (default: ``30000``) -- Specifies the poll time for the remote repository in milliseconds.

- ``maxLogLines`` (default:  ``20000``) -- Specifies the maximum number of log lines displayed in the log window.

- ``chunkSize`` (default:  ``5242880``) -- Specifies the chunk size of uploaded files in bytes.

- ``promptDeleteAllFiles`` (default:  ``true``) -- If a UI prompt should ask for confirmation if it was detected that all files and folders were deleted.

- ``notificationRefreshInterval`` (default``300,000``)  -- Specifies the default interval of checking for new server notifications in milliseconds.

