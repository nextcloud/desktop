When invoking the client from the command line, the following options are supported:

``-h``, ``--help``
        shows all the below options (opens a window on Windows)

``--logwindow``
        open a window to show log output.

``--logfile`` `<filename>`
        write log output to file <filename>. To write to stdout, specify `-`
        as filename

``--logdir`` `<name>`
        write each sync log output in a new file in directory <name>

``--logexpire`` `<hours>`
        removes logs older than <hours> hours. (to be used with --logdir)

``--logflush``
        flush the log file after every write.

``--confdir`` `<dirname>`
        Use the given configuration directory.

