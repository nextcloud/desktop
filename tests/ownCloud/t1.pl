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
createRemoteDir( "remoteToLocal1/rtl4" );

glob_put( 'toremote1/*', "remoteToLocal1/" );
glob_put( 'toremote1/rtl1/*', "remoteToLocal1/rtl1/" );
glob_put( 'toremote1/rtl1/rtl11/*',  "remoteToLocal1/rtl1/rtl11/" );
glob_put( 'toremote1/rtl2/*', "remoteToLocal1/rtl2/" );
glob_put( 'toremote1/rtl4/*', "remoteToLocal1/rtl4/" );

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
printInfo( "\nRemove a local file\n" );
unlink( localDir() . 'remoteToLocal1/kernelcrash.txt' );
csync();
assertLocalAndRemoteDir( 'remoteToLocal1', 0);

# add local files to a new dir1
printInfo( "Add some more files to local:");
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
printInfo( "Move a file locally." );
move( "$locDir/kramer.jpg", "$locDir/oldtimer.jpg" );
csync( );
assertLocalAndRemoteDir( 'fromLocal1', 0);

# move a local directory.
printInfo( "Move a local directory." );
move( localDir() . 'remoteToLocal1/rtl1', localDir(). 'remoteToLocal1/rtlX');
csync();
assertLocalAndRemoteDir( 'fromLocal1', 0);

# remove a local dir
printInfo( "Remove a local directory.");
localCleanup( localDir() . 'remoteToLocal1/rtlX' );
csync();
assertLocalAndRemoteDir( 'fromLocal1', 0);

# create a false conflict, only the mtimes are changed, by content are equal.
printInfo( "Create a false conflict.");
my $srcFile = 'toremote1/kernelcrash.txt';
put_to_dir( $srcFile, 'remoteToLocal1' );
system( "sleep 2 && touch " . localDir() . "remoteToLocal1/kernelcrash.txt" );
csync( );
assertLocalAndRemoteDir( 'fromLocal1', 0);

# create a true conflict.
printInfo( "Create a conflict." );
system( "echo \"This is more stuff\" >> /tmp/kernelcrash.txt" );
put_to_dir( '/tmp/kernelcrash.txt', 'remoteToLocal1' );
system( "sleep 2 && touch " . localDir() . "remoteToLocal1/kernelcrash.txt" );
csync();
assertLocalAndRemoteDir( 'remoteToLocal1', 1);

my $localMD5 = md5OfFile( localDir().'remoteToLocal1/kernelcrash.txt' );
my $realMD5 = md5OfFile( '/tmp/kernelcrash.txt' );
print "MD5 compare $localMD5 <-> $realMD5\n";
assert( $localMD5 eq $realMD5 );
assert(  glob(localDir().'remoteToLocal1/kernelcrash_conflict-*.txt' ) );


# ==================================================================

cleanup();

# --

