Test Plan
=========

1. Initial Setup

1.1 Normal Setup
+ Pre-Req: Remove oC-Config and all oC-folders completely from 
[Linux] ~/.local/share/data/ownCloud
[WinXP] c:\Dokumente und Einstellungen\admin\Lokale Einstellungen\Anwendungsdaten\ownCloud
[Win7]  

=> Start oCC and configure to 
  - oC setup start
    = Popup "No ownCloud connection was configured yet." appears.

  - Setup with correct values:
    - oC where local folder $HOME/ownCloud does not exist

    - oC where local folder $HOME/ownCloud does exist

    - oC where remote folder clientsync does not exist
      = oC set up, but no initial sync folder created

    - oC where remote folder clientsync does not exist
      = oC set up with initial sync folder ~/ownCloud => oC//clientsync

  - Setup with wrong url

  - Setup with wrong credentials

  - check permissions of oC setup file owncloud.cfg 
    = permissions -rw-------

2. Credentials Migration

=> the first version of oC had plain text credentials in the oC config file. 
   The migration path has to work: The plaintext password gets removed and 
   replaced by a Base64 encoded so far.

+ Pre-Req: create a credential file with correct cleartext password
           entry "password=geheim".
   - start oCC
     = oCC should start to sync without further notice. After that, the
       config file should contain a base64 encoded password.

3. SSL

=> With version 1.0.1 oCC supports SSL connections.
+ Pre-Req: Have a SSL ready host with unsigned certificate.

3.1 SSL connection
  - Start ownCloud configuration and enter the SSL url with https://...
    = The SSL Certificate dialog comes up.
    - Do not check the checkmark to trust
      = Connection does not work: "ssl handshake failed."
    - Do check the checkmark to trust
      = Connection is configured correctly.
      = oC config file contains a certificate entry (lots of strange bytes...)

4. No Password Storage
=> Since version 1.0.1 oCC supports that the password is not going to be stored.
   For that there is a checkmark in the oC setup dialog.

4.1 Do not store password.
  - Start to configure oC. Checkmark the "Do not store password.." checkbox.
   = The password entry field is grayed.
   = A dialog pops up and asks for password with displaying dots instead of chars.
   = The oC config file contains an empty passwd entry
   = The oC config file contains the entry "nostoredpasswd=true"
  - restart oC
   = oC comes up with a password dialog
   - provide good password:
     = oC works and never asks again
   - provide wrong password:
     = oC tells that username or password is wrong.

4.2 Do store password
  - Start to configure oC. Checkmark the "Do not store password.." checkbox.
   = The password entry field is enabled and takes a passwd
   = No dialog pops up to ask for the passwd
   = in oC config file the password appears and the nostoredpasswd param is false.
  - Restart oC.
   = Sync starts, no ask for password.
