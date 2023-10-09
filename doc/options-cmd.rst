``--path``
       Overrides default remote root folder to a specific subfolder on the server(e.g.: /Documents would sync the Documents subfolder on the server)

``--user``, ``-u`` `<user>`
       Use ``user`` as the login name.

``--password``, ``-p`` `<password>`
       Use `password` as the password.

``-n``
       Use :manpage:`netrc(5)` for login.

``--non-interactive``
       Do not prompt for questions.

``--silent``, ``--s``
       Inhibits verbose log output.

``--trust``
       Trust any SSL certificate, including invalid ones.

``--httpproxy``  `http://[user@pass:]<server>:<port>`
      Uses `server` as HTTP proxy.

``--exclude`` `<file>`
      Exclude list file

``--unsyncedfolders`` `<file>`
      File containing the list of unsynced folders (selective sync)

``--max-sync-retries`` `<n>`
      Retries maximum n times (defaults to 3)

``-h``
      Sync hidden files,do not ignore them
