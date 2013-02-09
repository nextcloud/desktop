mirall(1)
---------

SYNOPSIS
========

*mirall* ['OPTION'...]


DESCRIPTION
===========

mirall is a file synchronisation desktop utility.
It synchronizes files on your local machine with an ownCloud Server. If you
make a change to the files on one computer, it will flow across the others
using this desktop sync clients.

Normally you start the client by clickck on the desktoop icon or start from the
application menu. After starting an ownCloud icon appears in the system tray.


OPTIONS
=======
    
    --logwindow
        Open a window to show log output at startup.
    --logfile `<filename>`
        write log output to file.
    --flushlog
        flush the log file after every write.
    --monoicon
        Use black/white pictograms for systray.
    --help
        Print the help list.


Config File
===========

ownCloud Client reads a configuration file which on Linux can be found at
`$HOME/.local/share/data/ownCloud/owncloud.cfg`

On Windows and Mac, it can be found in
`\\Users\\<name>\\AppData\\Local\\ownCloud\\owncloud.cfg`

*Changes here should be done carefully as wrong settings can cause disfunctionality.*


These are config settings that may be changed:

    remotePollinterval
        Poll time for the remote repository in milliseconds (default 30000)
    maxLogLines
        Maximum count of log lines shown in the log window (default 20000)
    remotePollinterval
        The frequency used for polling for remote changes on the ownCloud Server.


BUGS
====

Please report bugs at https://github.com/owncloud/core/issues.


SEE ALSO
========
`csync(1)`, `mirall(1)`

