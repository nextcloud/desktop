Usage
=====
.. index:: usage, client sync usage

To start ownCloud Client, click on the desktop icon or start it from the
application menu. In the system tray, an ownCloud icon appears.

.. index:: start application

Overview
--------

ownCloud is represented by an icon in the Desktop's system tray, also known
as notification area.

The clients menu is accessed with a right click (Windows, Linux) or left click
(Mac OS).

The status of the current sync can be observed in the Status dialog, available
trough the ``Open status...`` option. On Windows, a left click on the tray icon
also opens the status dialog.

.. note:: Until the intial setup has finished, the Connection Wizard will be
          shown instead when left-clicking on Windows.
 
The dialog provides an overview on the configured sync folders and allows to add
and remove more sync folder connections as well as pausing a sync connection.

Changing your password
----------------------

Use the ``Configure`` option. It will open the Connection Wizard, which next to
reconfiguring your connection to use a different user or server also will allow
to change the password for the local account, or to switch from HTTP to HTTPS.

Setting up a proxy
------------------

By default, the configured system proxy will be picked up. This may not be
working reliable on some Linux distributions, as only the ``http_proxy``
variable gets parsed. You can configure a proxy different from your
system default by choosing ``Configure proxy...`` from the menu.

By default, ownCloud expects a HTTP proxy. If you want to specify a SOCKS5
proxy instead, tick the "Use as SOCKSv5 proxy" option.

Options
-------
.. index:: command line switches, command line, options, parameters
.. include:: options.rst

Config File
-----------
.. index:: config file
.. include:: conffile.rst
