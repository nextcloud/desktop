.. _installing-linux:

Installing the Linux Desktop Client
===================================

The ownCloud Desktop Client is provided for a wide range of Linux
distributions. The following table provides a list of Linux operating systems
and the specific distributions on which you can install the Desktop Client.

+------------------+-------------------------+
| Operating System | Distribution            | 
+==================+=========================+ 
| CentOS (Redhat)  | - Red Hat RHEL-6        |
|                  | - CentOS CentOS-6       | 
+------------------+-------------------------+ 
| Debian           | - Debian 7.0            |
|                  | - Fedora 19             |
|                  | - Fedora 20             | 
+------------------+-------------------------+ 
| openSUSE         | - openSUSE              |
|                  | - Factory PPC           |
|                  | - openSUSE Factory ARM  |	
|                  | - openSUSE Factory      |
|                  | - openSUSE 13.1 Ports   |	
|                  | - openSUSE 13.1         |
|                  | - openSUSE 12.3 Ports   |
|                  | - openSUSE 12.3         |
|                  | - openSUSE 12.2         | 
+------------------+-------------------------+ 
| SUSE (SLE)       | - SLE 11 SP3            | 
+------------------+-------------------------+
| Ubuntu           | - xUbuntu 14.04         |
|                  | - xUbuntu 13.10         |
|                  | - xUbuntu 12.10         |	
|                  | - xUbuntu 12.04         |
+------------------+-------------------------+ 

General instructions for how to install the ownCloud Desktop Client on any
supported Linux distribution can be found on the `ownCloud download page
<http://software.opensuse.org/download/package?project=isv:ownCloud:desktop&package=owncloud-client>`_.

Linux Installation Methods
--------------------------

You can install the ownCloud Desktop Client using either of the following three methods:

- One Click Install (openSUSE and SUSE SLE distributions only) — Installs the
  ownCloud Desktop using a bundled installation package.
- Adding the ownCloud package repository — Installs the ownCloud Desktop client
  using a Linux terminal and keeps it up to date using the distribution's
  package manager.

.. note::
	
	Manual command line installation requires that you perform the installation as root.
	
- Binary package — Installs the ownCloud Desktop Client using a raw binary package.

Installing the Desktop Client on Redhat or CentOS Linux Operating Systems
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To install the ownCloud Desktop Client on a Redhat or CentOS operating system manually:

1. Open a Linux terminal window.

2. Specify the directory in which you want to install the client.

	``cd /etc/yum.repos.d/``

3. Choose and download the client for your specific distribution: 

	* Red Hat RHEL-6: ``wget http://download.opensuse.org/repositories/isv:ownCloud:desktop/RedHat_RHEL-6/isv:ownCloud:desktop.repo``
	* CentOS CentOS-6: ``wget http://download.opensuse.org/repositories/isv:ownCloud:desktop/CentOS_CentOS-6/isv:ownCloud:desktop.repo``
	
4. Install the client.

	``yum install owncloud-client``
	
5. After the installation completes, go to Setting Up the ownCloud Desktop Client.

**Installing the Desktop Client on Debian 7.0 Linux Operating Systems Manually**

To install the ownCloud Desktop Client on the Debian 7.0 distribution manually:

1. Open a Linux terminal window.

2. Download the client.
	
	``echo 'deb http://download.opensuse.org/repositories/isv:/ownCloud:/desktop/Debian_7.0/ /' >> /etc/apt/sources.list.d/owncloud-client.list``

3. Download the package lists from any repositories and updates them to ensure the latest package versions and their dependencies.

	``apt-get update``

4. Install the client.

	``apt-get install owncloud-client``

5. (Optional) Download the apt-key for the Debian repository

	``wget http://download.opensuse.org/repositories/isv:ownCloud:desktop/Debian_7.0/Release.key``

6. After the installation completes, go to Setting Up the ownCloud Desktop Client.

Installing the Desktop Client on Fedora Linux Operating Systems
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To install the ownCloud Desktop Client on the Fedora operating system manually:

1. Open a Linux terminal window.
2. Specify the directory in which you want to install the client.

	cd /etc/yum.repos.d/

3. Choose and download the client for your specific distribution

	* Fedora 19: ``wget http://download.opensuse.org/repositories/isv:ownCloud:desktop/Fedora_19/isv:ownCloud:desktop.repo``
	* Fedora 20: ``wget http://download.opensuse.org/repositories/isv:ownCloud:desktop/Fedora_20/isv:ownCloud:desktop.repo``
	
4. Install the client.

	``yum install owncloud-client``

5. After the installation completes, go to Setting Up the ownCloud Desktop Client.

Installing the Desktop Client on openSUSE Linux Operating Systems
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To install the ownCloud Desktop Client on the openSUSE operating system manually:

1. Open a Linux terminal window.

2. Choose and download the client for your specific distribution: 

   * Factory PPC: ``zypper addrepo http://download.opensuse.org/repositories/isv:ownCloud:desktop/openSUSE_Factory_PPC/isv:ownCloud:desktop.repo``
   * **Factory ARM**: ``zypper addrepo http://download.opensuse.org/repositories/isv:ownCloud:desktop/openSUSE_Factory_ARM/isv:ownCloud:desktop.repo``
   * **Factory**: ``zypper addrepo http://download.opensuse.org/repositories/isv:ownCloud:desktop/openSUSE_Factory/isv:ownCloud:desktop.repo``
   * **13.1 Ports**: ``zypper addrepo http://download.opensuse.org/repositories/isv:ownCloud:desktop/openSUSE_13.1_Ports/isv:ownCloud:desktop.repo``
   * **13.1**: ``zypper addrepo http://download.opensuse.org/repositories/isv:ownCloud:desktop/openSUSE_13.1/isv:ownCloud:desktop.repo``
   * **12.3 Ports**: ``zypper addrepo http://download.opensuse.org/repositories/isv:ownCloud:desktop/openSUSE_12.3_Ports/isv:ownCloud:desktop.repo``
   * **12.3**: ``zypper addrepo http://download.opensuse.org/repositories/isv:ownCloud:desktop/openSUSE_12.3/isv:ownCloud:desktop.repo``
   * **12.2**: ``zypper addrepo http://download.opensuse.org/repositories/isv:ownCloud:desktop/openSUSE_12.2/isv:ownCloud:desktop.repo``

3. Download any package metadata  from the medium  and store  it  in  local  cache.

	``zypper refresh``

4. Install the client.

	``zypper install owncloud-client``

5. After the installation completes, go to Setting Up the ownCloud Desktop Client.

Installing the Desktop Client on SLE Linux Operating Systems
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To install the ownCloud Desktop Client on the SUSE Linux Enterprise (SLE) operating system.

1. Open a Linux terminal window.

2. Download the client.

	``zypper addrepo http://download.opensuse.org/repositories/isv:ownCloud:desktop/SLE_11_SP3/isv:ownCloud:desktop.repo``

3. Download any package metadata  from the medium  and store  it  in  local  cache.

	``zypper refresh``

4. Install the client.

	``zypper install owncloud-client``

5. After the installation completes, go to Setting Up the ownCloud Desktop Client.

Installing the Desktop Client on Ubuntu Linux Operating Systems
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To install the ownCloud Desktop Client on the Ubuntu operating system:

1. Open a Linux terminal window.

2. Choose and download  the client for your specific distribution:

	* **xUbuntu 14.04**: ``sudo sh -c "echo 'deb http://download.opensuse.org/repositories/isv:/ownCloud:/desktop/xUbuntu_14.04/ /' >> /etc/apt/sources.list.d/owncloud-client.list"``
	* **xUbuntu 13.10**: ``sudo sh -c "echo 'deb http://download.opensuse.org/repositories/isv:/ownCloud:/desktop/xUbuntu_13.10/ /' >> /etc/apt/sources.list.d/owncloud-client.list"``
	* **xUbuntu 12.10**: ``sudo sh -c "echo 'deb http://download.opensuse.org/repositories/isv:/ownCloud:/desktop/xUbuntu_12.10/ /' >> /etc/apt/sources.list.d/owncloud-client.list"``
	* **xUbuntu 12.04**: ``sudo sh -c "echo 'deb http://download.opensuse.org/repositories/isv:/ownCloud:/desktop/xUbuntu_12.04/ /' >> /etc/apt/sources.list.d/owncloud-client.list"``

3. Download the package lists from any repositories and updates them to ensure the latest package versions and their dependencies.

	``apt-get update``

4. Install the client.

	``sudo apt-get install owncloud-client``

5. (Optional) Download the apt-key for the Ubuntu repository:

	* **xUbuntu 14.04**: ``wget http://download.opensuse.org/repositories/isv:ownCloud:desktop/xUbuntu_14.04/Release.key``
	* **xUbuntu 13.10**: ``wget http://download.opensuse.org/repositories/isv:ownCloud:desktop/xUbuntu_13.10/Release.key``
	* **xUbuntu 12.10**: ``wget http://download.opensuse.org/repositories/isv:ownCloud:desktop/xUbuntu_12.10/Release.key``
	* **xUbuntu 12.04**: ``wget http://download.opensuse.org/repositories/isv:ownCloud:desktop/xUbuntu_12.04/Release.key``

6. (Optional) Add the apt key.

	``sudo apt-key add - < Release.key``

7. After the installation completes, go to `Setting Up the ownCloud Desktop Client`_.
