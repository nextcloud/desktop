Setting up an Account
=====================

If no account has been configured, the ownCloud Client will automatically
assist in connecting to your ownCloud server after the application has been
started.

As a first step, specify the URL to your Server. This is the same address
that is used in the browser.

.. image:: images/wizard_url.png
   :scale: 50 %

.. note:: Make sure to use ``https://`` if the server supports it. Otherwise,
   your password and all data will be transferred to the server unencrypted.
   This makes it easy for third parties to intercept your communication, and
   getting hold of your password!

Next, enter the username and password.  These are the same credentials used
to log into the web interface.

.. image:: images/wizard_user.png
   :scale: 50 %

Finally, choose the folder that ownCloud Client is supposed to sync the
contents of your ownCloud account with. By default, this is a folder
called `ownCloud`, which will be created in the home directory.

.. image:: images/wizard_targetfolder.png
   :scale: 50 %

At this time, the synchronization between the root directories of the
ownCloud server will begin.

.. image:: images/wizard_overview.png
   :scale: 50 %

If selecting a local folder that already contains data, there are
two options that exist.

* Keep local data: If selected, the files in the local folder on the
  client will be synced up to the ownCloud server.
* Start a clean sync: If selected, all files in the local folder on
  the client will be deleted and therefore not synced to the ownCloud
  server.
