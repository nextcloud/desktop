#!/usr/bin/perl
#
# Test script for the ownCloud module of csync.
# This script requires a running ownCloud instance accessible via HTTP.
# It does quite some fancy tests and asserts the results.
#
# Copyright (C) by Klaas Freitag <freitag@owncloud.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
#

use lib ".";

use File::Copy;
use ownCloud::Test;

use strict;

print "Hello, this is t9, a tester for content checksums.\n";

initTesting();

printInfo( "Add some files to local");
my $locDir = localDir();
copy( "testfiles/test.txt", "$locDir/test.txt");
copy( "testfiles/test.txt", "$locDir/test.eml");

csync( );
print "\nAssert local and remote dirs.\n";
assertLocalAndRemoteDir( '', 0);

# Get file properties before syncing again
my $txtpropbefore = remoteFileProp("", "test.txt");
my $emlpropbefore = remoteFileProp("", "test.eml");
assert($txtpropbefore);
assert($emlpropbefore);

printInfo( "Touch local files");
system( "touch $locDir/test.txt" );
system( "touch $locDir/test.eml" );

csync( );

# Get file properties afterwards
my $txtpropafter = remoteFileProp("", "test.txt");
my $emlpropafter = remoteFileProp("", "test.eml");
assert($txtpropafter);
assert($emlpropafter);

# The txt file is uploaded normally, etag and mtime differ
assert($txtpropafter->get_property( "getetag" ) ne
       $txtpropbefore->get_property( "getetag" ));
assert($txtpropafter->get_property( "getlastmodified" ) ne
       $txtpropbefore->get_property( "getlastmodified" ));
# The eml was not uploaded, nothing differs
assert($emlpropafter->get_property( "getetag" ) eq
       $emlpropbefore->get_property( "getetag" ));
assert($emlpropafter->get_property( "getlastmodified" ) eq
       $emlpropbefore->get_property( "getlastmodified" ));

printInfo( "Change content of eml file (but not size)");
system( "sleep 1 && sed -i -e 's/in/IN/' $locDir/test.eml" );

csync( );

# Get file properties afterwards
my $emlpropchanged = remoteFileProp("", "test.eml");
assert($emlpropchanged);
assert($emlpropafter->get_property( "getetag" ) ne
       $emlpropchanged->get_property( "getetag" ));
assert($emlpropafter->get_property( "getlastmodified" ) ne
       $emlpropchanged->get_property( "getlastmodified" ));

# ==================================================================

cleanup();

# --

