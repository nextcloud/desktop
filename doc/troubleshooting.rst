Appendix C: Troubleshooting
===========================

If the client fails to start syncing it basically can have two
basic reasons: Either the server setup has a problem or the client
has a bug. When reporting bugs, it is crucial to find out what part
of the system causes the problem.

Identifying basic functionality problems
----------------------------------------

:Perform a general ownCloud Server test:
  A very first check is to verify that you can log on to ownClouds web 
  application. Assuming your ownCloud instance is installed at 
  ``http://yourserver.com/owncloud``, type
  ``http://yourserver.com/owncloud/`` into your browsers address bar.
   
  If you are not prompted to enter your user name and password, or if you 
  see a red warning box on the page, your server setup is not correct or needs
  fixes. Please verify that your server installation is working correctly.

:Ensure the WebDAV API is working:
  If all desktop clients fail to connect to ownCloud, but the access via the
  web interface works, the problem often is a mis-configuration of the WebDAV
  API.

  The ownCloud client uses the built-in WebDAV access of the server content.
  Verify that you can log on to ownClouds WebDAV server. Assuming your ownCloud
  instance is installed at ``http://yourserver.com/owncloud``, type
  ``http://yourserver.com/owncloud/remote.php/webdav`` into your browsers
  address bar.

  If you are prompted, but the authentication fails even though the credentials
  your provided are correct, please ensure that your authentication backend
  is configured properly.

:Use a WebDAV command line tool to test:  
  A more sophisticated test is to use a WebDAV command line client and log
  into the ownCloud WebDAV server, such as a little app called cadaver,
  available on Linux. It can be used to further verify that the WebDAV server is
  running properly, for example by performing PROPFIND calls:

  ``propget .`` called within cadaver will return some properties of the current
  directory and thus be a successful WebDAV connect.

Isolating other issues
----------------------

If the sync result is unreliable, please ensure that the folder synced with
ownCloud is not shared with other syncing apps.

.. note:: Syncing the same directory with ownCloud and other sync software such
          as Unison, rsync, Microsoft Windows Offline Folders or cloud services
          such as DropBox or Microsoft SkyDrive is not supported and should
          not be attempted. In the worst case, doing so can result in data
          loss.

If some files do not get take a look at the sync protocol. Some files are
automatically automatically being ignored because they are system files,
others get ignored because their file name contains characters that cannot
be represented on certain file systems. See :ref:`_ignored-files-label` for
details.

If you are operating your own server and use the local storage backend (the
default), make sure that ownCloud has exclusive access to the directory.

.. note:: The data directory on the server is exclusive to ownCloud and must
          not be modified manually.

If you are using a different backend, you can try to exclude a bug in the
backend by reverting to the local backend.

Logfiles
--------

Doing effective debugging requires to provide as much as relevant logs as
possible. The log output can help you with tracking down problem, and if you 
report a bug, you're advised to include the output.

Client Logfile
~~~~~~~~~~~~~~

Start the client with ``--logwindow``. That opens a window providing a view
on the current log. It provides a Save button to let you save the log to a 
file.

You can also open a log window for an already running session, by simply 
starting the client again with this parameter. Syntax:

  * Windows: ``C:\Program Files (x86)\ownCloud\owncloud.exe --logwindow``
  * Mac OS X: ``/Applications/owncloud.app/Contents/MacOS/owncloud --logwindow``
  * Linux: ``owncloud --logwindow``

It is also possible to directly log to a directory, which is an useful option
in case the problem only happens ocassionally. In that case it is better to
create a huge amount of data, as the log window has a limited buffer.

To write logs to disk, start the client with ``--logfile <file>``, where
``<file`` is the file you want to log to, or ``--logdir <dir>``, where ``<dir>``
is an existing directory. In case of ``--logdir``, each sync run will create a
new file. To limit the amount of data that accumulates over time, there is another
useful parameter: ``--logexpire <hours>```. If that is combined with ```--logdir```
the client automatically erases log data in that directory that is older than the
given expiry period.

For example, for a long running test where you intend to keep the log data of the
last two days, this would be the command line:

```
owncloud --logdir /tmp/owncloud_logs --logexpire 48
```

ownCloud server Logfile
~~~~~~~~~~~~~~~~~~~~~~~

The ownCloud server maintains an ownCloud specific logfile as well. It can and
must be enabled through the ownCloud Administration page. There you can adjust
the loglevel. It is advisable to set it to a verbose level like ``Debug`` or
``Info``.
  
The logfile can be viewed either in the web interface or can be found in the
filesystem in the ownCloud server data dir.

Webserver Logfiles
~~~~~~~~~~~~~~~~~~

Also, please take a look at your webservers error log file to check if there
are problems. For Apache on Linux, the error logs usually can be found at
``/var/log/apache2``. A file called ``error_log`` shows errors like PHP code
problems. A file called ``access_log`` usually records all requests handled
by the server. Especially the access_log is a very good debugging tool as the
log line contains a lot of information of every request and it's result.
  
More information about the apache logging can be found at
``http://httpd.apache.org/docs/current/logs.html``.

