#!/usr/bin/perl
#
# Test script for the ownCloud module of csync.
# This script requires a running ownCloud instance accessible via HTTP.
# It does quite some fancy tests and asserts the results.
#
# Copyright (C) by Klaas Freitag <freitag@owncloud.com>
#

use lib ".";

use Carp::Assert;
use File::Copy;
use ownCloud::Test;

use strict;

print "Hello, this is t1, a tester for csync with ownCloud.\n";

initTesting();

print "Copy some files to the remote location\n";
createRemoteDir( "remoteToLocal1" );
createRemoteDir( "remoteToLocal1/rtl1" );
createRemoteDir( "remoteToLocal1/rtl1/rtl11" );
createRemoteDir( "remoteToLocal1/rtl2" );

glob_put( 'toremote1/*', "remoteToLocal1/" );
glob_put( 'toremote1/rtl1/*', "remoteToLocal1/rtl1/" );
glob_put( 'toremote1/rtl1/rtl11/*',  "remoteToLocal1/rtl1/rtl11/" );
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

# remove a local file.
print "\nRemove a local file\n";
unlink( localDir() . 'remoteToLocal1/kernelcrash.txt' );
csync();
assertLocalAndRemoteDir( 'remoteToLocal1', 0);

# add local files to a new dir1
print "\nAdd some more files to local:\n";
my $locDir = localDir() . 'fromLocal1';

mkdir( $locDir );
assert( -d $locDir );
foreach my $file ( <./tolocal1/*> ) {
    print "Copying $file to $locDir\n";
    copy( $file, $locDir );
}
csync( );
print "\nAssert local and remote dirs.\n";
assertLocalAndRemoteDir( 'fromLocal1', 0);

# move a local file
print "\nMove a file locally.\n";
move( "$locDir/kramer.jpg", "$locDir/oldtimer.jpg" );
csync( );
assertLocalAndRemoteDir( 'fromLocal1', 0);

# move a local directory.
print "\nMove a local directory.\n";
move( localDir() . 'remoteToLocal1/rtl1', localDir(). 'remoteToLocal1/rtlX');
csync();
assertLocalAndRemoteDir( 'fromLocal1', 0);

# remove a local dir
print "\nRemove a local directory.\n";
localCleanup( localDir() . 'remoteToLocal1/rtlX' );
csync();
assertLocalAndRemoteDir( 'fromLocal1', 0);

# create a false conflict, only the mtimes are changed, by content are equal.
print "\nCreate a false conflict.\n";
my $srcFile = 'toremote1/kernelcrash.txt';
put_to_dir( $srcFile, 'remoteToLocal1' );
system( "sleep 2 && touch $srcFile" );
csync( );
assertLocalAndRemoteDir( 'fromLocal1', 0);

# create a true conflict.
print "\nCreate a conflict.\n";
system( "echo \"This is more stuff\" >> /tmp/kernelcrash.txt" );
put_to_dir( '/tmp/kernelcrash.txt', 'remoteToLocal1' );
system( "sleep 2 && touch $srcFile" );
csync();
assertLocalAndRemoteDir( 'remoteToLocal1', 1);

# ==================================================================

cleanup();

# --

