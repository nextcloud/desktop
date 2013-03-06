Troubleshooting
===============

:All of my desktop clients fail to connect to ownCloud:
  Verify that you can log on to ownClouds WebDAV server. Assuming your ownCloud
  instance is installed at ``http://yourserver.com/owncloud``, type
  ``http://yourserver.com/owncloud/remote.php/webdav`` into your browsers
  address bar.

  If you are not prompted to enter your user name and password, please verify
  that your server installation is working correctly.

  If you are prompted, but the authentication fails even though the credentials
  your provided are correct, please ensure that your authentication backend
  is configured properly.

  A more sophisticated test is to use a WebDAV command line client and log
  into the ownCloud WebDAV server, such as a little app called cadaver, available
  on Linux. I can be used to further verify that the WebDAV server is running
  properly, for example by performing PROPFIND calls:

  ``propget .`` called within cadaver will return some properties of the current
  directory and thus be a successful WebDAV connect.

:The desktop client fails for an unknown reason:
  Start the client with ``--logwindow``. You can also open a log window for an
  already running session, by simply starting the client again with this
  parameter. Syntax:

  * Windows: ``C:\Program Files (x86)\ownCloud\owncloud.exe --logwindow``
  * Mac OS X: ``/Applications/owncloud.app/Contents/MacOS/owncloud --logwindow``
  * Linux: ``owncloud --logwindow``

  The log output can help you with tracking down problem, and if you report
  a bug, it's useful to include the output.

  Also, please take a look at your webservers error log file to check if there
  are problems. For apache on linux, the error logs usually can be found at
  ``/var/log/apache2``. A file called ``error_log`` shows errors like PHP code
  problems. A file called ``access_log`` usually records all requests handled
  by the server. More information about the apache logging can be found at
  ``http://httpd.apache.org/docs/current/logs.html``.