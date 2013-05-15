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

print "Hello, this is t2, a tester for renaming directories\n";

initTesting();

print "Copy some files to the remote location\n";
createRemoteDir( "remoteToLocal1" );
createRemoteDir( "remoteToLocal1/rtl1" );
createRemoteDir( "remoteToLocal1/rtl1/rtl11" );
createRemoteDir( "remoteToLocal1/rtl2" );

glob_put( 'toremote1/*', "remoteToLocal1/" );
glob_put( 'toremote1/rtl1/*', "remoteToLocal1/rtl1/" );
glob_put( 'testfiles/*',  "remoteToLocal1/rtl1/rtl11/" );
glob_put( 'toremote1/rtl2/*', "remoteToLocal1/rtl2/" );

# call csync, sync local t1 to remote t1
csync();

# Check if the files from toremote1 are now in t1/remoteToLocal1
# they should have taken the way via the ownCloud.
print "Assert the local file copy\n";
assertLocalDirs( 'toremote1', localDir().'remoteToLocal1' );

# Check if the synced files from ownCloud have the same timestamp as the local ones.
print "\nNow assert remote 'toremote1' with local " . localDir() . " :\n";
assertLocalAndRemoteDir( 'remoteToLocal1', 0);

# Make a new directory, moves a sub directory into.  Remove the parent directory.
# create a new file on the server in the directory that will be renamed
my $newfile_md5 = createLocalFile("remoteToLocal1/rtl1/rtl11/newfile.dat", 123);
unlink( localDir() . 'remoteToLocal1/rtl1/rtl11/test.txt' );
mkdir( localDir() . 'newdir' );
move( localDir() . 'remoteToLocal1/rtl1/', localDir() . 'newdir/' );
system( "rm -rf " . localDir() . 'remoteToLocal1' );
system( "echo \"my file\" >> /tmp/myfile.txt" );
put_to_dir( '/tmp/myfile.txt', 'remoteToLocal1/rtl1/rtl11' );

csync();
assertLocalAndRemoteDir( '', 0);

#TODO: test that newfile.dat and myfile.txt exists in newdir/rtl1
#      and test that there is no newdir/rtl11/test.txt

cleanup();

# --
