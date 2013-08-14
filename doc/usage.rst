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

Changing Your Password and Account Settings
-------------------------------------------

In the ``Settings`` Dialog, choose ``Account`` -> ``Modify Account``. It will open
Setup Wizard, which next to reconfiguring your connection to use a different
user or server also will allow to change the password for the local account,
or to switch from HTTP to HTTPS.

Setting up a Proxy
------------------

By default, the configured system proxy will be picked up. This may not be
working reliably on some Linux distributions, as only the ``http_proxy``
variable gets picked up. You can configure a proxy different from your
system default in the ``Network`` section of the ``Settings`` dialog.

The default settings  assume an HTTP proxy, which is the typical use case.
If you require SOCKS 5 proxy, pick ``SOCKS5 proxy`` instead of ``HTTP(S) proxy``
from the drop down menu. SOCKS 5 proxies are typically provided by some
SSH implementations, for instance OpenSSH's ``-D`` parameter. This is
useful for scenarios where SSH is employed to securely tunnel a client
to the network running the ownCloud server.

Limiting Bandwidth
------------------

Starting with Version 1.4, the Client provides bandwidth limiter.
This option can be found in the ``Network`` section of the
``Settings Dialog``.

You will find two settings for ``Download Bandwidth`` and
``Upload Bandwidth``.

Upload Bandwidth
~~~~~~~~~~~~~~~~

The default is to automatically limit the upload. The rationale
for this default is that typically, Computers and laptops are
not directly connected to the server, but via a Cable Modems
or DSL lines, which provide significantly more downstream than
upstream bandwith. Sataurating the upstream bandwidth would
interfere with other applications, especially Voice-Over-IP or
Games.

The automatic limiter will throttle the speed to about 75%
of the available upstream bandwidth. If you are communicating
with the server via a fast, symetric connection, you can set the
Limiter to ``No Limit`` instead. If want a stronger limitation,
choose ``Limit to`` and specify a limit manually.


Download Bandwidth
~~~~~~~~~~~~~~~~~~

Because the download bandwidth is usually no concern, it is not
automatically limited. Should you find that the Client is taking
up too much bandwidth, you can manually specify a limit (in KB).

Options
-------
.. index:: command line switches, command line, options, parameters
.. include:: options.rst

Config File
-----------
.. index:: config file
.. include:: conffile.rst
