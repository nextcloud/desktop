Compiling the desktop client on Windows with Qt6
================================================

System requirements
-------------------
- Windows 10 or Windows 11
- `The desktop client code <https://github.com/nextcloud/desktop>`_  
- Python 3
- PowerShell
- Microsoft Visual Studio 2022 and tools to compile C++
- `KDE Craft <https://community.kde.org/Craft>`_


Setting up Microsoft Visual Studio
----------------------------------

- Click on 'Modify' in the Visual Studio Installer:

  .. image:: ./images/dev/visual-studio-installer.png
    :alt: Visual Studio Installer

- Select 'Desktop development with C++'

  .. image:: ./images/dev/desktop-development-with-cpp.png
    :alt: Desktop development with C++

Handling the dependencies 
-------------------------

We decided to use `KDE Craft <https://community.kde.org/Craft>`_ to get all binary dependencies of the desktop client.
because it is convenient to mantain and to set it up.

- Set up KDE Craft as instructed in `Get Involved/development/Windows - KDE Community Wiki <https://community.kde.org/Get_Involved/development/Windows>`_ -  it requires Python 3 and PowerShell.
- After running:

.. code-block:: winbatch

   C:\CraftRoot\craft\craftenv.ps1

- Add the desktop client blueprints - the instructions to handle the client dependencies:

.. code-block:: winbatch

  craft --add-blueprint-repository [git]https://github.com/nextcloud/desktop-client-blueprints.git
  craft craft

- Install all client dependencies:

.. code-block:: winbatch

  craft --install-deps nextcloud-client

Compiling
---------

- Make sure your environment variable %PATH% has the possible minimum 
- Open the Command Prompt (cmd.exe)
- Run:

.. code-block:: winbatch

  "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

- To use the tools installed with Visual Studio, you need the following in your %PATH%:

  .. image:: ./images/dev/path.png
    :alt: Windows environment variables    

- Alternatively you can use the tools installed with KDE Craft by adding them to %PATH%:

.. code-block:: winbatch

  set "PATH=C:\CraftRoot\bin;C:\CraftRoot\dev-utils\bin;%PATH%"

.. note::
  C:\CraftRoot is the path used by default by KDE Craft. When you are setting it up you may choose a different folder.

- Create build folder, run cmake, compile and install:

.. code-block:: winbatch

  cd <desktop-repo-path>
  mkdir build
  cd build
  cmake .. -G Ninja -DCMAKE_INSTALL_PREFIX=. -DCMAKE_PREFIX_PATH=C:\CraftRoot -DCMAKE_BUILD_TYPE=RelWithDebInfo
  cmake --build . --target install
  
After this, you can use `Qt Creator <https://doc.qt.io/qtcreator>`_ to import the build folder with its configurations to be able to work with the code.


