Usage
=====
.. index:: usage, client sync usage

To start ownCloud Client, click on the desktop icon or start it from the
application menu. In the system tray, an ownCloud icon appears.

.. index:: start application

A left click on the tray icon open a status dialog which gives an overview on
the configured sync folders and allows to add and remove more sync folder
connections as well as pausing a sync connection.

A right click on the tray icon gives other configuration options.


Command line switches
---------------------
.. index:: command line switches, command line, options, parameters


ownCloud Client supports the following command line switches:

+--------------------------+------------------------------------------------+
| Switch                   | Action                                         |
+==========================+================================================+
| ``--logwindow``          | open a window to show log output at startup.   |
+--------------------------+------------------------------------------------+
| ``--logfile <filename>`` | write log output to file.                      |
+--------------------------+------------------------------------------------+
| ``--flushlog``           | flush the log file after every write.          |
+--------------------------+------------------------------------------------+

Config File
-----------
.. index:: config file

ownCloud Client reads a configuration file which on Linux can be found at ``$HOME/.local/share/data/ownCloud/owncloud.cfg``
On Windows, it can be found in ``%LOCALAPPDATA%\ownCloud\owncloud.cfg``
.. todo:: Mac?
It contains settings in the ini file format known from Windows. 

.. note:: Changes here should be done carefully as wrong settings can cause disfunctionality.

.. note:: Changes may be overwritten by using ownCloud's configuration dialog.


These are config settings that may be changed:

+---------------------------+-----------+--------------+-----------+-----------------------------------------------------+
| Setting                   | Data Type | Unit         | Default   | Description                                         |
+===========================+===========+==============+===========+=====================================================+
| ``remotePollinterval``    | integer   | milliseconds | ``30000`` | Poll time for the remote repository                 |
+---------------------------+-----------+--------------+-----------+-----------------------------------------------------+
| ``maxLogLines``           | integer   | lines        | ``20000`` | Maximum count of log lines shown in the log window  |
+---------------------------+-----------+--------------+-----------+-----------------------------------------------------+

* ``remotePollinterval`` The frequency used for polling for remote changes on
  the ownCloud Server.

