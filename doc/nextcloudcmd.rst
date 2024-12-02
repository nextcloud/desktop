The Nextcloud Client packages contain a command line client, ``nextcloudcmd``, that can 
be used to synchronize Nextcloud files to client machines.

``nextcloudcmd`` performs a single *sync run* and then exits the synchronization 
process. In this manner, ``nextcloudcmd`` processes the differences between 
client and server directories and propagates the files to bring both 
repositories to the same state. Contrary to the GUI-based client, 
``nextcloudcmd`` does not repeat synchronizations on its own. It also does not 
monitor for file system changes.


Install ``nextcloudcmd``
~~~~~~~~~~~~~~~~~~~~~~~~

CentOS

::

    $ sudo yum -y install epel-release
    $ sudo yum -y install nextcloud-client

Ubuntu

::

    $ sudo add-apt-repository ppa:nextcloud-devs/client
    $ sudo apt update
    $ sudo apt install nextcloud-client

Debian

::

    $ sudo apt install nextcloud-desktop-cmd


Refer to the link

- https://nextcloud.com/install/#install-clients
- https://launchpad.net/~nextcloud-devs/+archive/ubuntu/client
- https://pkgs.alpinelinux.org/packages?name=nextcloud-client
- https://help.nextcloud.com/t/linux-packages-status/10216


To invoke ``nextcloudcmd``, you must provide the local and the remote repository 
URL using the following command::

  nextcloudcmd [OPTIONS...] sourcedir nextcloudurl

where ``sourcedir`` is the local directory and ``nextcloudurl`` is
the server URL.

Other command line switches supported by ``nextcloudcmd`` include the following:

.. include:: ../doc/options-cmd.rst

Credential Handling
~~~~~~~~~~~~~~~~~~~

``nextcloudcmd`` requires the user to specify the username and password using the standard URL pattern, e.g., 

::

  $ nextcloudcmd /home/user/my_sync_folder https://carla:secret@server/nextcloud

To synchronize the Nextcloud directory ``Music`` to the local directory
``media/music``, through a proxy listening on port ``8080``, and on a gateway
machine using IP address ``192.168.178.1``, the command line would be::

  $ nextcloudcmd --httpproxy http://192.168.178.1:8080 --path /Music \
                $HOME/media/music \
                https://server/nextcloud

``nextcloudcmd`` will prompt for the user name and password, unless they have
been specified on the command line or ``-n`` has been passed.

Exclude List
~~~~~~~~~~~~

``nextcloudcmd`` requires access to an exclude list file. It must either be
installed along with ``nextcloudcmd`` and thus be available in a system location,
be placed next to the binary as ``sync-exclude.lst`` or be explicitly specified
with the ``--exclude`` switch.

The required file content is one exclude item per line where wildcards are allowed, e.g.: 
::

    ~*.tmp
    ._*
    ]Thumbs.db
    ]photothumb.db
    System Volume Information

Example
~~~~~~~~~~~~

- Synchronize a local directory to the specified directory of the nextcloud server

::

    $ nextcloudcmd --path /<Directory_that_has_been_created> /home/user/<my_sync_folder> \
    https://<username>:<secret>@<server_address>
