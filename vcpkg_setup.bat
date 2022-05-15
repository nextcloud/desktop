@echof off
mkdir c:\tools
cd c:\tools
git clone https://github.com/microsoft/vcpkg
.\vcpkg\bootstrap-vcpkg.bat
pause
.\vcpkg\vcpkg integrate install

