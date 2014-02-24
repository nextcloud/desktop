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

use Carp::Assert;
use File::Copy;
use ownCloud::Test;

use strict;

print "Hello, this is t4, a tester for A) files that cannot be stated and B) excluded files\n";
# stat error occours on windsows when the file is busy for example

initTesting();

printInfo( "Copy some files to the remote location" );
mkdir( localDir() . 'test_stat' );
system( "echo foobar > " . localDir() . 'test_stat/file.txt' );

# call csync, sync local t1 to remote t1
csync();

# Check if the files from toremote1 are now in t1/remoteToLocal1
# they should have taken the way via the ownCloud.
print "Assert the local file copy\n";
assertLocalAndRemoteDir( '', 0 );


printInfo( "Make a file not statable" );


system( "echo foobar2 >> " . localDir() . 'test_stat/file.txt' );
#make the file not statable by changing the directory right
system( "chmod 400 " . localDir() . 'test_stat' );


csync();

# TODO: some check here.



printInfo("Add a file in a read only directory");

system( "echo \"Hello World\" >> /tmp/kernelcrash.txt" );
put_to_dir( '/tmp/kernelcrash.txt', 'test_stat' );

csync();

assert( ! -e localDir().'test_stat/kernelcrash' );


printInfo("Restore the original rights");

system( "chmod 700 " . localDir() . 'test_stat' );
system( "echo foobar3 >> " . localDir() . 'test_stat/file.txt' );

csync();

print "Check if everything is still the same\n";

assertLocalAndRemoteDir( '', 0 );

# TODO: Check that the file content is fine on the server and that there was no conflict
assert( -e localDir().'test_stat/file.txt' );
assert( -e localDir().'test_stat/kernelcrash.txt' );

my $localMD5 = md5OfFile( localDir().'test_stat/kernelcrash.txt' );
my $realMD5 = md5OfFile( '/tmp/kernelcrash.txt' );
print "MD5 compare $localMD5 <-> $realMD5\n";
assert( $localMD5 eq $realMD5 );


printInfo("Added a file that is on the ignore list");
# (*.directory is in the ignored list that needs cleanup)
# (it is names with _conflict) because i want the conflicft detection of assertLocalAndRemoteDir to work
system( "echo dir >> " . localDir() . 'test_stat/file_conflict.directory' );
csync();
# The file_conflict.directory is seen as a conflict
assertLocalAndRemoteDir( '', 1 );
# TODO: check that the file_conflict.directory is indeed NOT on the server

printInfo("Remove a directory containing a local file\n");
remoteCleanup('test_stat');

#Add an executable file for next test
system( "echo echo hello >> " . localDir() . 'echo.sh' );
chmod 0751, localDir() . 'echo.sh';

csync();
assertLocalAndRemoteDir( '', 0 );

open(my $fh, "<", localDir() . 'echo.sh');
my $perm = (stat $fh)[2] & 07777;
assert( $perm eq 0751, "permissions not kept" );


printInfo("Modify a file in the remote and check its permission\n");
system( "echo \"echo bonjour\" > /tmp/echo.sh" );
put_to_dir( '/tmp/echo.sh', "" );
csync();
assertLocalAndRemoteDir( '', 0 );

open(my $fh, "<", localDir() . 'echo.sh');
my $perm = (stat $fh)[2] & 07777;
assert( $perm eq 0751, "permissions not kept" );


cleanup();

# --
