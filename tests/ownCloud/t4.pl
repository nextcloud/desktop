#!/usr/bin/perl
#
# Test script for the ownCloud module of csync.
# This script requires a running ownCloud instance accessible via HTTP.
# It does quite some fancy tests and asserts the results.
#
# Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
#

use lib ".";

use Carp::Assert;
use File::Copy;
use ownCloud::Test;

use strict;

print "Hello, this is t4, a tester for A) files that cannot be stated and B) excluded files\n";
# stat error occours on windsows when the file is busy for example

initTesting();

print "Copy some files to the remote location\n";
mkdir( localDir() . 'test_stat' );
system( "echo foobar > " . localDir() . 'test_stat/file.txt' );

# call csync, sync local t1 to remote t1
csync();

# Check if the files from toremote1 are now in t1/remoteToLocal1
# they should have taken the way via the ownCloud.
print "Assert the local file copy\n";
assertLocalAndRemoteDir( '', 0 );


system( "echo foobar2 >> " . localDir() . 'test_stat/file.txt' );
#make the dile not statable by changing the directory right
system( "chmod 600 " . localDir() . 'test_stat' );


csync();

# TODO: some check here.


print("Restore the original rights");

system( "chmod 700 " . localDir() . 'test_stat' );
system( "echo foobar3 >> " . localDir() . 'test_stat/file.txt' );

csync();

print "Check if everything is still the same\n";

assertLocalAndRemoteDir( '', 0 );

# TODO: Check that the file content is fine on the server and that there was no conflict

print("Added a file that is on the ignore list\n");
# (*.directory is in the ignored list that needs cleanup)
# (it is names with _conflict) because i want the conflicft detection of assertLocalAndRemoteDir to work
system( "echo dir >> " . localDir() . 'test_stat/file_conflict.directory' );
csync();
# The file_conflict.directory is seen as a conflict
assertLocalAndRemoteDir( '', 1 );
# TODO: check that the file_conflict.directory is indeed NOT on the server

print("Remove a directory containing a local file\n");
remoteCleanup('test_stat');
csync();
assertLocalAndRemoteDir( '', 0 );



cleanup();

# --
