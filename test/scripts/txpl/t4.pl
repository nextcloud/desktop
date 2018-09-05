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

sub getInode($)
{
    my ($filename) = @_;
    my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
        $atime,$mtime,$ctime,$blksize,$blocks) = stat($filename);

        return $ino;
}

print "Hello, this is t4, a tester for A) files that cannot be stated and B) excluded files C) hard links\n";
# stat error occours on windsows when the file is busy for example

initTesting();

printInfo( "Copy some files to the remote location" );
mkdir( localDir() . 'test_stat' );
system( "echo foobar > " . localDir() . 'test_stat/file.txt' );

mkdir( localDir() . 'test_ignored' );
mkdir( localDir() . 'test_ignored/sub' );
system( "echo foobarfoo > " . localDir() . 'test_ignored/sub/file.txt' );

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

# Sync failed, can't download file to readonly dir
csync(1);

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
# (it is names with conflicted copy) because i want the conflicft detection of assertLocalAndRemoteDir to work
system( "echo dir >> " . localDir() . 'test_stat/file_conflicted\ copy.directory' );
# this one should retain the directory
system( "echo foobarfoo > " . localDir() . 'test_ignored/sub/ignored_conflicted\ copy.part' );
csync();
# The file_conflicted\ copy.directory is seen as a conflict
assertLocalAndRemoteDir( '', 1 );
# TODO: check that the file_conflicted\ copy.directory is indeed NOT on the server
# TODO: check that test_ignored/sub/ignored_conflicted\ copy.part is NOT on the server
assert(-e localDir() . 'test_ignored/sub/ignored_conflicted copy.part');

printInfo("Remove a directory containing an ignored file that should not be removed\n");
remoteCleanup('test_ignored');
csync();
assert(-e localDir() . 'test_ignored/sub/ignored_conflicted copy.part');
#remove the file so next sync allow the directory to be removed
system( "rm " . localDir() . 'test_ignored/sub/ignored_conflicted\ copy.part' );

printInfo("Remove a directory containing a local file\n");
remoteCleanup('test_stat');

#Add an executable file for next test
system( "echo echo hello >> " . localDir() . 'echo.sh' );
chmod 0751, localDir() . 'echo.sh';

#and add a file in anotherdir for the next test
mkdir( localDir() . 'anotherdir' );
mkdir( localDir() . 'anotherdir/sub' );
system( "echo foobar > " . localDir() . 'anotherdir/file1.txt' );
system( "echo foobar > " . localDir() . 'anotherdir/sub/file2.txt' );

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

printInfo("Remove a directory and make it a symlink instead\n");
system( "rm -rf " . localDir() . 'anotherdir' );
system( "ln -s /bin " . localDir() . 'anotherdir' );
# remember the fileid of the file on the server
my $oldfileid1 = remoteFileId( 'anotherdir/', 'file1.txt' );
my $oldfileid2 = remoteFileId( 'anotherdir/sub', 'file2.txt' );
csync();

#check that the files in ignored directory has NOT been removed
my $newfileid1 = remoteFileId( 'anotherdir/', 'file1.txt' );
my $newfileid2 = remoteFileId( 'anotherdir/sub', 'file2.txt' );
assert( $oldfileid1 eq $newfileid1, "File removed (file1.txt)" );
assert( $oldfileid2 eq $newfileid2, "File removed (file2.txt)" );

printInfo("Now remove the symlink\n");
system( "rm -f " . localDir() . 'anotherdir' );
csync();
assertLocalAndRemoteDir( '', 0 );
assert(! -e localDir(). 'anotherdir' );


printInfo("Test hardlinks\n");
#make a hard link
mkdir( localDir() . 'subdir' );
createLocalFile( localDir() .'subdir/original.data', 1568 );
system( "ln " . localDir() . 'subdir/original.data ' . localDir() . 'file.link');
csync();
assertLocalAndRemoteDir( '', 0 );
my $inode = getInode(localDir() . 'subdir/original.data');
my $inode2 = getInode(localDir() . 'file.link');
assert( $inode == $inode2, "Inode is not the same!");


printInfo("Modify hard link\n");
system( "echo 'another line' >> " . localDir() . 'file.link');
csync();
assertLocalAndRemoteDir( '', 0 );
my $inode1 = getInode(localDir() .'subdir/original.data');
$inode2 = getInode( localDir() .'file.link');
assert( $inode == $inode1, "Inode is not the same!");
assert( $inode == $inode2, "Inode is not the same!");


printInfo("Rename a hard link\n");
move( localDir() . 'subdir/original.data', localDir() . 'subdir/kernelcrash.txt' );
csync();
assertLocalAndRemoteDir( '', 0 );
$inode1 = getInode(localDir() .'subdir/kernelcrash.txt');
$inode2 = getInode(localDir() .'file.link');
assert( $inode == $inode1, "Inode is not the same!");
assert( $inode == $inode2, "Inode is not the same!");

printInfo("Modify a hard link on the server\n");
put_to_dir( '/tmp/kernelcrash.txt', 'subdir' );
csync();
assertLocalAndRemoteDir( '', 0 );
$inode1 = getInode(localDir() .'subdir/kernelcrash.txt');
$inode2 = getInode( localDir() .'file.link');
# only the first inode must change
print(" $inode $inode1 $inode2" );
assert( $inode != $inode1, "Inode did not change");
assert( $inode == $inode2, "Inode is not the same!");

cleanup();

# --
