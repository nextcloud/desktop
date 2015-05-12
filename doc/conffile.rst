The ownCloud Client reads a configuration file.  You can locate this configuration files as follows:

On Linux distributions:
        ``$HOME/.local/share/data/ownCloud/owncloud.cfg``

On Microsoft Windows systems:
        ``%LOCALAPPDATA%\ownCloud\owncloud.cfg``

On MAC OS X systems:
        ``$HOME/Library/Application Support/ownCloud``


The configuration file contains settings using the Microsoft Windows .ini file
format. You can overwrite changes using the ownCloud configuration dialog.

.. note:: Use caution when making changes to the ownCloud Client configuration
   file.  Incorrect settings can produce unintended results.

You can change the following configuration settings (must be under the ``[ownCloud]`` section)

- ``remotePollInterval`` (default: ``30000``) -- Specifies the poll time for the remote repository in milliseconds.

- ``maxLogLines`` (default:  ``20000``) -- Specifies the maximum number of log lines displayed in the log window.

