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

print "Hello, this is t3, a tester for renaming directories\n";

initTesting();

printInfo( "Copy some files to the remote location\n" );
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
printInfo( "Assert the local file copy\n" );
assertLocalDirs( localDir().'remoteToLocal1', 'toremote1' );

# Check if the synced files from ownCloud have the same timestamp as the local ones.
printInfo( "Now assert remote 'toremote1' with local " . localDir() );
assertLocalAndRemoteDir( 'remoteToLocal1', 0);

# Make a new directory, moves a sub directory into.  Remove the parent directory.
# create a new file on the server in the directory that will be renamed
printInfo( "Create a new directory and move subdirs into." );
my $newfile_md5 = createLocalFile(localDir()."remoteToLocal1/rtl1/rtl11/newfile.dat", 123);
unlink( localDir() . 'remoteToLocal1/rtl1/rtl11/test.txt' );
mkdir( localDir() . 'newdir' );
move( localDir() . 'remoteToLocal1/rtl1', localDir() . 'newdir/rtl1' );
system( "rm -rf " . localDir() . 'remoteToLocal1' );
system( "echo \"my file\" >> /tmp/myfile.txt" );
put_to_dir( '/tmp/myfile.txt', 'remoteToLocal1/rtl1/rtl11' );

# Also add a file with symbols
my $symbolName = "a\%b#c\$d-e";

system( "echo \"my symbols\" >> /tmp/$symbolName" );
put_to_dir( "/tmp/$symbolName", 'remoteToLocal1/rtl1/rtl11' );


my $fileid = remoteFileId( 'remoteToLocal1/rtl1/', 'rtl11' );
my $fid2 =   remoteFileId( 'remoteToLocal1/rtl1/', 'La ced' );
assert($fid2 eq "" or $fileid ne $fid2, "File IDs are equal" );

csync();
my $newFileId = remoteFileId( 'newdir/rtl1/', 'rtl11' );
my $newfid2   = remoteFileId( 'newdir/rtl1/', 'La ced' );
assert($newFileId eq "" or $newFileId ne $newfid2, "File IDs are equal" );

assert( $fileid eq $newFileId, "file ID mixup: 'newdir/rtl1/rtl11" );
assert( $fid2 eq $newfid2, "file ID mixup: 'newdir/La ced" );

assertLocalAndRemoteDir( 'newdir', 0);

assert( -e localDir().'newdir/rtl1/rtl11/newfile.dat' );
assert( -e localDir().'newdir/rtl1/rtl11/myfile.txt' );
assert( ! -e localDir().'newdir/rtl11/test.txt' );
# BUG!  remoteToLocal1 is not deleted because changes were detected
#       (even if the changed fileswere moved)
# assert( ! -e localDir().'remoteToLocal1' );
assert( ! -e localDir().'remoteToLocal1/rtl1' );

printInfo("Move file and create another one with the same name.");
move( localDir() . 'newdir/myfile.txt', localDir() . 'newdir/oldfile.txt' );
system( "echo \"super new\" >> " . localDir() . 'newdir/myfile.txt' );

#Move a file with symbols as well
move( localDir() . "newdir/$symbolName", localDir() . "newdir/$symbolName.new" );

#Add some files for the next test.
system( "echo \"un\" > " . localDir() . '1.txt' );
system( "echo \"deux\" > " . localDir() . '2.txt' );
system( "echo \"trois\" > " . localDir() . '3.txt' );
mkdir( localDir() . 'newdir2' );

csync();
assertLocalAndRemoteDir( 'newdir', 0);


printInfo("Rename a directory that was just changed");
# newdir was changed so it's etag is not yet saved in the database,  but still it needs to be moved.
my $newdirId = remoteFileId( localDir(), 'newdir' );
my $newdir2Id = remoteFileId( localDir(), 'newdir2' );
move(localDir() . 'newdir' , localDir() . 'newdir3');
move(localDir() . 'newdir2' , localDir() . 'newdir4');


# FIXME:  this test is currently failing
#  see csync_update.c in _csyn_detect_update, the commen near the commented fs->inode != tmp->inode
# unlink( localDir() . '1.txt' );
# move( localDir() . '2.txt', localDir() . '1.txt' );

csync();
assertLocalAndRemoteDir( '', 0);
my $newdir3Id = remoteFileId( localDir(), 'newdir3' );
my $newdir4Id = remoteFileId( localDir(), 'newdir4' );
assert( $newdirId eq $newdir3Id, "newdir was not MOVE'd to newdir3?" );
assert( $newdir2Id eq $newdir4Id, "newdir2 was not MOVE'd to newdir4?" );

printInfo("Move a file and replace it by a new one");


move( localDir() . '1.txt', localDir() . '1_bis.txt' );
move( localDir() . '3.txt', localDir() . '3_bis.txt' );
system( "echo \"new file un\" > " . localDir() . '1.txt' );
system( "echo \"new file trois\" > " . localDir() . '3.txt' );

#also add special file with special character for next sync
#and file with special characters
createLocalFile(localDir().  'hêllo%20th@re.txt' , 1208 );

csync();
assertLocalAndRemoteDir( '', 0);

printInfo("Move a file containing special character");

move(localDir().  'hêllo%20th@re.txt', localDir().  'hêllo%20th@re.doc');
csync();
assertLocalAndRemoteDir( '', 0);


cleanup();

# --
