#!/usr/bin/perl
#
# Test script for the ownCloud module of csync.
# This script requires a running ownCloud instance accessible via HTTP.
# It does quite some fancy tests and asserts the results.
#
# Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
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

print "Hello, this is t8, a tester for syncing of files on a case sensitive FS\n";


# The test is run on a 'normal' file system, but we tell pwncloud that it is case preserving anyway
$ENV{OWNCLOUD_TEST_CASE_PRESERVING} = "1";

# No parallelism for more deterministic action.
$ENV{OWNCLOUD_MAX_PARALLEL}="1";

initTesting();

printInfo( "Syncing two files with the same name that differ with case" );

#create some files localy
my $tmpdir = "/tmp/t8/";
mkdir($tmpdir);
createLocalFile( $tmpdir . "HELLO.dat", 100 );
createLocalFile( $tmpdir . "Hello.dat", 150 );
createLocalFile( $tmpdir . "Normal.dat", 110 );
createLocalFile( $tmpdir . "test.dat", 170 );

#put them in some directories
createRemoteDir( "dir" );
glob_put( "$tmpdir/*", "dir" );

csync();

# Check that only one of the two file was synced.
# The one that exist here is undefined, the current implementation will take the
# first one alphabetically,  but the other one would also be fine. What's imporant
# is that there is only one.
assert( -e localDir() . 'dir/HELLO.dat' );
assert( !-e localDir() . 'dir/Hello.dat' );

printInfo( "Remove one file should remove it on the server and download the other one" );
unlink( localDir() . 'dir/HELLO.dat' );

csync();
assert( -e localDir() . 'dir/Hello.dat' );
assert( !-e localDir() . 'dir/HELLO.dat' );
assertLocalAndRemoteDir( '', 0);


printInfo( "Renaming one file to the same name as another one with different casing" );
moveRemoteFile( 'dir/Hello.dat', 'dir/NORMAL.dat');
moveRemoteFile( 'dir/test.dat', 'dir/TEST.dat');

csync();

# Hello -> NORMAL should not have do the move since the case conflict
assert( -e localDir() . 'dir/Hello.dat' );
assert( !-e localDir() . 'dir/NORMAL.dat' );
assert( -e localDir() . 'dir/Normal.dat' );

#test->TEST should have been worked.
assert( -e localDir() . 'dir/TEST.dat' );
assert( !-e localDir() . 'dir/test.dat' );


printInfo( "Another directory with the same name but different casing is created" );

createRemoteDir( "DIR" );
glob_put( "$tmpdir/*", "DIR" );

csync();

assert( !-e localDir() . 'DIR' );


printInfo( "Remove the old dir localy" );

system("rm -r " . localDir() . "dir");

csync();

# now DIR was fetched
assert( -e localDir() . 'DIR' );
assert( -e localDir() . 'DIR/HELLO.dat' );
assert( !-e localDir() . 'DIR/Hello.dat' );
assert( !-e localDir() . 'dir' );

# dir/NORMAL.dat is still on the server


printInfo( "Attempt downloading two clashing files in parallel" );

# Enable parallelism
$ENV{OWNCLOUD_MAX_PARALLEL}="2";

my $tmpdir2 = "/tmp/t8/parallel/";
mkdir($tmpdir2);
createLocalFile( $tmpdir2 . "FILE.dat", 23251233 );
createLocalFile( $tmpdir2 . "file.dat",       33 );
createRemoteDir( "parallel" );
glob_put( "$tmpdir2/*", "parallel" );

csync();

# only one file must exist
assert( (!-e localDir() . 'parallel/FILE.dat' ) or (!-e localDir() . 'parallel/file.dat') );
assert( (-e localDir() . 'parallel/FILE.dat' ) or (-e localDir() . 'parallel/file.dat') );

cleanup();
system("rm -r " . $tmpdir);

