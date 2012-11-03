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
