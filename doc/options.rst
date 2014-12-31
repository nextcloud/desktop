When invoking the client from the command line, the following options are supported:

``-h``, ``--help``
        Displays all command options.

``--logwindow``
        Opens a window displaying log output.

``--logfile`` `<filename>`
        Write log output to the file specified. To write to stdout, specify `-` as the filename.

``--logdir`` `<name>`
        Writes each synchronization log output in a new file in the specified directory.

``--logexpire`` `<hours>`
        Removes logs older than the value specified (in hours). This command is used with ``--logdir``.

``--logflush``
        Clears (flushes) the log file after each write action.

``--confdir`` `<dirname>`
        Uses the specified configuration directory.

