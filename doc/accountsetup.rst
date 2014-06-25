Setting up an Account
=====================

If no account has been configured, the ownCloud Client automatically assist in
connecting to your ownCloud server after the application has been started.

To set up an account:

1. Specify the URL to your Server. This is the same address that is used in the browser.

.. image:: images/wizard_url.png
   :scale: 50 %

.. note:: Make sure to use ``https://`` if the server supports it. Otherwise,
   your password and all data will be transferred to the server unencrypted.  This
   makes it easy for third parties to intercept your communication, and getting
   hold of your password!

2. Enter the username and password.  These are the same credentials used to log into the web interface.

.. image:: images/wizard_user.png
   :scale: 50 %

3. Choose the folder with which you want the ownCloud Client to synchronize the
   contents of your ownCloud account. By default, this is a folder called
   `ownCloud`. This folder is created in the home directory.

.. image:: images/wizard_targetfolder.png
   :scale: 50 %

   The synchronization between the root directories of the ownCloud server begins.

.. image:: images/wizard_overview.png
   :scale: 50 %

When selecting a local folder that already contains data, you can choose from two options:

* :guilabel:`Keep local data`: When selected, the files in the local folder on
  the client are synchronized to the ownCloud server.

* :guilabel:`Start a clean sync`: When selected, all files in the local folder on the
  client are deleted.  These files are not syncrhonized to the ownCloud server.
