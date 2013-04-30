Troubleshooting
===============

If the client fails to start syncing it basically can have two
basic reasons: Either the server setup has a problem or the client
has a bug. When reporting bugs, it is crucial to find out what part
of the system causes the problem.

Here are a couple of useful steps to isolate the problem.

:A general ownCloud Server test:
  A very first check is to verify that you can log on to ownClouds web 
  application. Assuming your ownCloud instance is installed at 
  ``http://yourserver.com/owncloud``, type
  ``http://yourserver.com/owncloud/`` into your browsers address bar.
   
  If you are not prompted to enter your user name and password, or if you 
  see a red warning box on the page, your server setup is not correct or needs
  fixes. Please verify that your server installation is working correctly.

:All desktop clients fail to connect to ownCloud:
  The ownCloud syncing use the built in WebDAV server of ownCloud. 
  Verify that you can log on to ownClouds WebDAV server. Assuming your ownCloud
  instance is installed at ``http://yourserver.com/owncloud``, type
  ``http://yourserver.com/owncloud/remote.php/webdav`` into your browsers
  address bar.

  If you are prompted, but the authentication fails even though the credentials
  your provided are correct, please ensure that your authentication backend
  is configured properly.

:Use a WebDAV command line tool to test:  
  A more sophisticated test is to use a WebDAV command line client and log
  into the ownCloud WebDAV server, such as a little app called cadaver, available
  on Linux. It can be used to further verify that the WebDAV server is running
  properly, for example by performing PROPFIND calls:

  ``propget .`` called within cadaver will return some properties of the current
  directory and thus be a successful WebDAV connect.

Logfiles
========

Doing effective debugging requires to provide as much as relevant logfiles as
possible. The log output can help you with tracking down problem, and if you 
report a bug, you're advised to include the output.

:Client Logfile:
Start the client with ``--logwindow``. That opens a window providing a view
on the current log. It provides a Save button to let you save the log to a 
file.

You can also open a log window for an already running session, by simply 
starting the client again with this parameter. Syntax:

  * Windows: ``C:\Program Files (x86)\ownCloud\owncloud.exe --logwindow``
  * Mac OS X: ``/Applications/owncloud.app/Contents/MacOS/owncloud --logwindow``
  * Linux: ``owncloud --logwindow``

It is also possible to directly log into a file, which is an useful option
in case the problem only happens ocassionally. In that case it is better to
create a huge logfile than piping the whole log through the log window.

To create a log file, start the client with ``--logfile <filename>``.

:ownCloud server Logfile:
The ownCloud server maintains an ownCloud specific logfile as well. It can and
must be enabled through the ownCloud Administration page. There you can adjust
the loglevel. It is advisable to set it to a verbose level like ``Debug`` or ``Info``.
  
The logfile can be viewed either in the web interface or can be found in the
filesystem in the ownCloud server data dir.

:Webserver Logfiles:
Also, please take a look at your webservers error log file to check if there
are problems. For apache on linux, the error logs usually can be found at
``/var/log/apache2``. A file called ``error_log`` shows errors like PHP code
problems. A file called ``access_log`` usually records all requests handled
by the server. Especially the access_log is a very good debugging tool as the
log line contains a lot of information of every request and it's result.
  
More information about the apache logging can be found at
``http://httpd.apache.org/docs/current/logs.html``.

 
