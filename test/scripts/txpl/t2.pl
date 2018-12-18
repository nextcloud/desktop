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
printInfo("Initial sync, sync stuff down.");
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
printInfo("Simply move a file to another name.");
my $inode = getInode('remoteToLocal1/kernelcrash.txt');
moveRemoteFile( 'remoteToLocal1/kernelcrash.txt', 'remoteToLocal1/kernel.txt');

csync();
assertLocalAndRemoteDir( 'remoteToLocal1', 0);
my $inode2 = getInode( 'remoteToLocal1/kernel.txt');
assert( $inode == $inode2, "Inode has changed!");

printInfo("Move a file into a sub directory.");
# now move the file into a sub directory
moveRemoteFile( 'remoteToLocal1/kernel.txt', 'remoteToLocal1/rtl1/');

csync();
assertLocalAndRemoteDir( 'remoteToLocal1', 0);
$inode = getInode('remoteToLocal1/rtl1/kernel.txt');
assert( $inode == $inode2, "Inode has changed 2!");

printInfo("Move an existing directory.");
# move an existing directory
$inode = getInode('remoteToLocal1/rtl1');
moveRemoteFile( 'remoteToLocal1/rtl1', 'remoteToLocal1/movedRtl1');

csync();
assertLocalAndRemoteDir( 'remoteToLocal1', 0);
$inode = getInode('remoteToLocal1/movedRtl1');
assert( $inode == $inode2, "Inode has changed 3!");

printInfo( "Move a file in a directory and than move the dir." );
# move a file in a directory and than move the directory
moveRemoteFile('remoteToLocal1/movedRtl1/rtl11/zerofile.txt', 'remoteToLocal1/movedRtl1/rtl11/centofile.txt');
moveRemoteFile( 'remoteToLocal1/movedRtl1', 'remoteToLocal1/againRtl1');

csync();
assertLocalAndRemoteDir( 'remoteToLocal1', 0);

printInfo("Move a directory and than move a file within it.");

# move a directory and than move a file within the directory
moveRemoteFile( 'remoteToLocal1/againRtl1', 'remoteToLocal1/moved2Rtl1');
moveRemoteFile('remoteToLocal1/moved2Rtl1/rtl11/centofile.txt', 'remoteToLocal1/moved2Rtl1/tripofile.txt');

csync();
assertLocalAndRemoteDir( 'remoteToLocal1', 0);

printInfo("Rename file loally and remotely to a different name.");
# Rename a file locally and the same file remotely to another name. 
move( localDir() . 'remoteToLocal1/moved2Rtl1/tripofile.txt', localDir() . 'remoteToLocal1/moved2Rtl1/meckafile.txt' );

moveRemoteFile( 'remoteToLocal1/moved2Rtl1/tripofile.txt', 'remoteToLocal1/moved2Rtl1/sofiafile.txt' );

csync();
assertLocalAndRemoteDir( 'remoteToLocal1', 0);

# Change a file remotely and than move the directory
printInfo( "Move a directory remotely with a changed file in it.");

my $md5 = createLocalFile( '/tmp/sofiafile.txt', 43 );
put_to_dir( '/tmp/sofiafile.txt', 'remoteToLocal1/moved2Rtl1' );

moveRemoteFile( 'remoteToLocal1/moved2Rtl1', 'remoteToLocal1/newDir');

# Now in remoteToLocal1/newDir/sofiafile.txt we should have content...
csync();
assertLocalAndRemoteDir( 'remoteToLocal1', 0);

my $newMd5 = md5OfFile( localDir().'remoteToLocal1/newDir/sofiafile.txt' );
print "MD5 compare $md5 <-> $newMd5\n";
assert( $md5 eq $newMd5 );

# Move a directory on remote but remove the dir locally
printInfo("Move a directory remotely, but remove the local one");
moveRemoteFile( 'remoteToLocal1/newDir', 'remoteToLocal1/newDir2');

system( "rm -rf " . localDir() . 'remoteToLocal1/newDir');
# move a file but create a file with the same name locally.
moveRemoteFile( 'remoteToLocal1/newDir2/sofiafile.txt', 'remoteToLocal1/constantinopel.txt' );
csync();
assertLocalAndRemoteDir( 'remoteToLocal1', 0);

# Move a file remotely and create one with the same name on the 
# local repo.
printInfo("Move remotely and create a local file with same name");

moveRemoteFile('remoteToLocal1/rtl2/kb1.jpg', 'remoteToLocal1/rtl2/kb1moved.jpg');
move( localDir().'remoteToLocal1/rtl2/kb1.jpg', localDir().'remoteToLocal1/rtl2/kb1_local_gone.jpg'); 

csync();
assertLocalAndRemoteDir( 'remoteToLocal1', 0);

## make new directory remote 
printInfo("Create a remote dir, put in a file and move it, but have a similar one locally.");

createRemoteDir('remoteToLocal1/rtl2/newRemoteDir');

my $firstMd5 = createLocalFile( '/tmp/donat12.txt', 4096 );
put_to_dir( '/tmp/donat12.txt', 'remoteToLocal1/rtl2/newRemoteDir/' );
moveRemoteFile('remoteToLocal1/rtl2/newRemoteDir/donat12.txt', 
               'remoteToLocal1/rtl2/newRemoteDir/donat.txt'); 
mkdir( localDir().'remoteToLocal1/rtl2/newRemoteDir' );
createLocalFile( localDir(). 'remoteToLocal1/rtl2/newRemoteDir/donat.txt', 8021 );

csync();
assertLocalAndRemoteDir( 'remoteToLocal1', 1);

printInfo("simulate a owncloud 5 update by removing all the fileid");
## simulate a owncloud 5 update by removing all the fileid
system( "sqlite3 " . localDir() . ".sync_*.db \"UPDATE metadata SET fileid='';\"");
#refresh the ids
csync();
assertLocalAndRemoteDir( 'remoteToLocal1', 1);


printInfo("Move a file from the server");
$inode = getInode('remoteToLocal1/rtl2/kb1_local_gone.jpg');
moveRemoteFile( 'remoteToLocal1/rtl2/kb1_local_gone.jpg', 'remoteToLocal1/rtl2/kb1_local_gone2.jpg');

#also create a new directory localy for the next test
mkdir( localDir().'superNewDir' );
createLocalFile(localDir().  'superNewDir/f1', 1234 );
createLocalFile(localDir().  'superNewDir/f2', 1324 );
my $superNewDirInode = getInode('superNewDir');


csync();
assertLocalAndRemoteDir( '', 1);
$inode2 = getInode('remoteToLocal1/rtl2/kb1_local_gone2.jpg');
assert( $inode == $inode2, "Inode has changed 3!");


printInfo("Move a newly created directory");
moveRemoteFile('superNewDir', 'superNewDirRenamed');
#also add new files in new directory
createLocalFile(localDir().  'superNewDir/f3' , 2456 );
$inode = getInode('superNewDir/f3');

csync();
assertLocalAndRemoteDir( '', 1);
my $file = localDir() . 'superNewDir';
assert( ! -e $file );

$inode2 = getInode('superNewDirRenamed/f3');
assert( $inode == $inode2, "Inode of f3 changed");
$inode2 = getInode('superNewDirRenamed');
assert( $superNewDirInode == $inode2, "Inode of superNewDir changed");

cleanup();

# --
