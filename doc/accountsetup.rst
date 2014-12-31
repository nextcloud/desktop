Setting up an Account
=====================

When you run the ownCloud Desktop Sync client the first time, it automatically 
opens the account setup wizard. Just follow these steps:

1. Enter the URL to your Server. This is the same address used by your Web 
browser, for example ``https://example.com/owncloud``

.. image:: images/wizard_url.png
   :scale: 50 %

.. note:: Always use ``https://`` if SSL encryption is enabled on your server. 
   Otherwise, your password and all traffic between your computer and the 
   ownCloud server will be transmitted in the clear and wide open for 
   eavesdroppers.

2. Enter your username and password.  These are the same credentials used to 
   log into the ownCloud Web interface.

.. image:: images/wizard_user.png
   :scale: 50 %

3. Choose the local folder you want to store your ownCloud files in. By 
   default, this is a folder called ``ownCloud`` in your home directory.

.. image:: images/wizard_targetfolder.png
   :scale: 50 %

4. The synchronization automatically begins.

.. image:: images/wizard_overview.png
   :scale: 50 %

When selecting a local folder that already contains data, you can choose from two options:

* :guilabel:`Keep local data`: When selected, the files in the local folder on
  the client are synchronized to the ownCloud server.

* :guilabel:`Start a clean sync`: When selected, all files in the local folder on the
  client are deleted.  These files are not synchronized to the ownCloud server.

See :doc:`navigating` to learn how to choose specific folders to sync with on 
your ownCloud server.

