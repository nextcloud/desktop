#!/bin/sh

rm -vrf admin/
rm -vrf src/3rdparty/sqlite3   # FIXME: For CentOS6 we have to use our bundled sqlite
rm -vrf binary/
rm -vrf src/3rdparty/libcrashreporter-qt
rm -vrf shell_integration/windows
rm -vrf shell_integration/MacOSX
