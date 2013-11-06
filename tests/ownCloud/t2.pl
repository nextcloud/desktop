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

sub getInode($)
{
  my ($filename) = @_;
  my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
      $atime,$mtime,$ctime,$blksize,$blocks) = stat($filename);
      
  return $ino;
}

print "Hello, this is t2, a tester for remote renaming\n";

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
assertLocalDirs( localDir().'remoteToLocal1', 'toremote1' );

# Check if the synced files from ownCloud have the same timestamp as the local ones.
print "\nNow assert remote 'toremote1' with local " . localDir() . " :\n";
assertLocalAndRemoteDir( 'remoteToLocal1', 0);

# Do some remote moves:

# First a simple file move.
my $inode = getInode('remoteToLocal1/kernelcrash.txt');
moveRemoteFile( 'remoteToLocal1/kernelcrash.txt', 'remoteToLocal1/kernel.txt');

csync();
assertLocalAndRemoteDir( 'remoteToLocal1', 0);
my $inode2 = getInode( 'remoteToLocal1/kernel.txt');
assert( $inode == $inode2, "Inode has changed!");

# now move the file into a sub directory
$inode = getInode('remoteToLocal1/kernel.txt');
moveRemoteFile( 'remoteToLocal1/kernel.txt', 'remoteToLocal1/rtl1/');

csync();
assertLocalAndRemoteDir( 'remoteToLocal1', 0);
$inode = getInode('remoteToLocal1/rtl1/kernel.txt');
assert( $inode == $inode2, "Inode has changed 2!");

# move an existing directory
$inode = getInode('remoteToLocal1/rtl1');
moveRemoteFile( 'remoteToLocal1/rtl1', 'remoteToLocal1/movedRtl1');

csync();
assertLocalAndRemoteDir( 'remoteToLocal1', 0);
$inode = getInode('remoteToLocal1/movedRtl1');
assert( $inode == $inode2, "Inode has changed 3!");

# move a file in a directory and than move the directory
moveRemoteFile('remoteToLocal1/movedRtl1/rtl11/zerofile.txt', 'remoteToLocal1/movedRtl1/rtl11/centofile.txt');
moveRemoteFile( 'remoteToLocal1/movedRtl1', 'remoteToLocal1/againRtl1');

csync();
assertLocalAndRemoteDir( 'remoteToLocal1', 0);

# move a directory and than move a file within the directory
moveRemoteFile( 'remoteToLocal1/againRtl1', 'remoteToLocal1/moved2Rtl1');
moveRemoteFile('remoteToLocal1/moved2Rtl1/rtl11/centofile.txt', 'remoteToLocal1/moved2Rtl1/tripofile.txt');

csync();
assertLocalAndRemoteDir( 'remoteToLocal1', 0);

# Rename a file locally and the same file remotely to another name. The server
# name should win.
move( localDir() . 'remoteToLocal1/moved2Rtl1/tripofile.txt', localDir() . 'remoteToLocal1/moved2Rtl1/meckafile.txt' );

moveRemoteFile( 'remoteToLocal1/moved2Rtl1/tripofile.txt', 'remoteToLocal1/moved2Rtl1/sofiafile.txt' );

csync();
assertLocalAndRemoteDir( 'remoteToLocal1', 0);

# Change a file remotely and than move the directory

my $md5 = createLocalFile( '/tmp/sofiafile.txt', 43 );
put_to_dir( '/tmp/sofiafile.txt', 'remoteToLocal1/moved2Rtl1' );

moveRemoteFile( 'remoteToLocal1/moved2Rtl1', 'remoteToLocal1/newDir');

# Now in remoteToLocal1/newDir/sofiafile.txt we should have content...
csync();
assertLocalAndRemoteDir( 'remoteToLocal1', 0);

my $newMd5 = md5OfFile( localDir().'remoteToLocal1/newDir/sofiafile.txt' );
print "MD5 compare $md5 <-> $newMd5\n";
assert( $md5 eq $newMd5 );


cleanup();

# --
