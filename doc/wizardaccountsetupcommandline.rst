If you want to automate an Account Setup Wizard to allow the user skip entering server URL and local sync folder path in UI, you can use command-line parameters.
When you specify both, the desktop client's Account Setup Wizard will jump straight to opening a browser for account authentication/connection without the need of entering any of the connection details.
The local sync folder will also be selected to the one you specify instead of using default path (/home/Nextcloud)

The following parameters are supported:

``--overridelocaldir``
   specify a local dir to be used in the account setup wizard (e.g.: /home/nextcloud-sync-folder)

``--overrideserverurl``
        specify a server URL to use for the force override to be used in the account setup wizard (e.g.: https://cloud.example.com)

Examples:

- ``C:\Program Files\Nextcloud\nextcloud.exe" --overridelocaldir "D:/work/nextcloud-sync-folder" --overrideserverurl https://cloud.example.com``
- For Linux and mac the same example as above will work but ``nextcloud.exe path`` and ``--overridelocaldir`` value should get changed to platform specific format (e.g. ``no .exe extension`` and ``/home/<user folder>`` format)
