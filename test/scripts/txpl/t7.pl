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

print "Hello, this is t7, a tester for syncing of files in read only directory\n";

# Check if the expected rows in the DB are non-empty. Note that in some cases they might be, then we cannot use this function
# https://github.comowncloud/client/issues/2038
sub assertCsyncJournalOk {
	my $path = $_[0];

    # FIXME: should test also remoteperm but it's not working with owncloud6
    #     my $cmd = 'sqlite3 ' . $path . '._sync_*.db "SELECT count(*) from metadata where length(remotePerm) == 0 or length(fileId) == 0"';
    my $cmd = 'sqlite3 ' . $path . '._sync_*.db "SELECT count(*) from metadata where length(fileId) == 0"';
	my $result = `$cmd`;
	assert($result == "0");
}

# IMPORTANT NOTE :
print "This test use the OWNCLOUD_TEST_PERMISSIONS environement variable and _PERM_xxx_ on filenames to set the permission. ";
print "It does not rely on real permission set on the server.  This test is just for testing the propagation choices\n";
# "It would be nice" to have a test that test with real permissions on the server

$ENV{OWNCLOUD_TEST_PERMISSIONS} = "1";

initTesting();

printInfo( "Init" );

#create some files localy
my $tmpdir = "/tmp/t7/";
mkdir($tmpdir);
createLocalFile( $tmpdir . "normalFile_PERM_WVND_.data", 100 );
createLocalFile( $tmpdir . "cannotBeRemoved_PERM_WVN_.data", 101 );
createLocalFile( $tmpdir . "canBeRemoved_PERM_D_.data", 102 );
my $md5CanotBeModified = createLocalFile( $tmpdir . "canotBeModified_PERM_DVN_.data", 103 );
createLocalFile( $tmpdir . "canBeModified_PERM_W_.data", 104 );

#put them in some directories
createRemoteDir( "normalDirectory_PERM_CKDNV_" );
glob_put( "$tmpdir/*", "normalDirectory_PERM_CKDNV_" );
createRemoteDir( "readonlyDirectory_PERM_M_" );
glob_put( "$tmpdir/*", "readonlyDirectory_PERM_M_" );
createRemoteDir( "readonlyDirectory_PERM_M_/subdir_PERM_CK_" );
createRemoteDir( "readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_" );
glob_put( "$tmpdir/normalFile_PERM_WVND_.data", "readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_" );


csync();
assertCsyncJournalOk(localDir());
assertLocalAndRemoteDir( '', 0);

system("sleep 1"); #make sure changes have different mtime

printInfo( "Do some changes and see how they propagate" );

#1. remove the file than cannot be removed
#  (they should be recovered)
unlink( localDir() . 'normalDirectory_PERM_CKDNV_/cannotBeRemoved_PERM_WVN_.data' );
unlink( localDir() . 'readonlyDirectory_PERM_M_/cannotBeRemoved_PERM_WVN_.data' );

#2. remove the file that can be removed
#  (they should properly be gone)
unlink( localDir() . 'normalDirectory_PERM_CKDNV_/canBeRemoved_PERM_D_.data' );
unlink( localDir() . 'readonlyDirectory_PERM_M_/canBeRemoved_PERM_D_.data' );

#3. Edit the files that cannot be modified
#  (they should be recovered, and a conflict shall be created)
system("echo 'modified' > ". localDir() . "normalDirectory_PERM_CKDNV_/canotBeModified_PERM_DVN_.data");
system("echo 'modified_' > ". localDir() . "readonlyDirectory_PERM_M_/canotBeModified_PERM_DVN_.data");

#4. Edit other files
#  (they should be uploaded)
system("echo '__modified' > ". localDir() . "normalDirectory_PERM_CKDNV_/canBeModified_PERM_W_.data");
system("echo '__modified_' > ". localDir() . "readonlyDirectory_PERM_M_/canBeModified_PERM_W_.data");

#5. Create a new file in a read only folder
# (should be uploaded)
createLocalFile( localDir() . "normalDirectory_PERM_CKDNV_/newFile_PERM_WDNV_.data", 106 );

#do the sync
csync();
assertCsyncJournalOk(localDir());

#1.
# File should be recovered
assert( -e localDir(). 'normalDirectory_PERM_CKDNV_/cannotBeRemoved_PERM_WVN_.data' );
assert( -e localDir(). 'readonlyDirectory_PERM_M_/cannotBeRemoved_PERM_WVN_.data' );

#2.
# File should be deleted
assert( !-e localDir() . 'normalDirectory_PERM_CKDNV_/canBeRemoved_PERM_D_.data' );
assert( !-e localDir() . 'readonlyDirectory_PERM_M_/canBeRemoved_PERM_D_.data' );

#3.
# File should be recovered
assert($md5CanotBeModified eq md5OfFile( localDir().'normalDirectory_PERM_CKDNV_/canotBeModified_PERM_DVN_.data' ));
assert($md5CanotBeModified eq md5OfFile( localDir().'readonlyDirectory_PERM_M_/canotBeModified_PERM_DVN_.data' ));
# and conflict created
# TODO check that the conflict file has the right content
assert( -e glob(localDir().'normalDirectory_PERM_CKDNV_/canotBeModified_PERM_DVN__conflict-*.data' ) );
assert( -e glob(localDir().'readonlyDirectory_PERM_M_/canotBeModified_PERM_DVN__conflict-*.data' ) );
# remove the conflicts for the next assertLocalAndRemoteDir
system("rm " . localDir().'normalDirectory_PERM_CKDNV_/canotBeModified_PERM_DVN__conflict-*.data' );
system("rm " . localDir().'readonlyDirectory_PERM_M_/canotBeModified_PERM_DVN__conflict-*.data' );

#4. File should be updated, that's tested by assertLocalAndRemoteDir

#5.
# the file should be in the server and local
assert( -e localDir() . "normalDirectory_PERM_CKDNV_/newFile_PERM_WDNV_.data" );

### Both side should still be the same
assertLocalAndRemoteDir( '', 0);

# Next test

#6. Create a new file in a read only folder
# (they should not be uploaded)
createLocalFile( localDir() . "readonlyDirectory_PERM_M_/newFile_PERM_WDNV_.data", 105 );

# error: can't upload to readonly
csync(1);
assertCsyncJournalOk(localDir());

#6.
# The file should not exist on the remote
# TODO: test that the file is NOT on the server
# but still be there
assert( -e localDir() . "readonlyDirectory_PERM_M_/newFile_PERM_WDNV_.data" );
# remove it so assertLocalAndRemoteDir succeed.
unlink(localDir() . "readonlyDirectory_PERM_M_/newFile_PERM_WDNV_.data");

### Both side should still be the same
assertLocalAndRemoteDir( '', 0);




#######################################################################
printInfo( "remove the read only directory" );
# -> It must be recovered
system("rm -r " . localDir().'readonlyDirectory_PERM_M_' );
csync();
assertCsyncJournalOk(localDir());
assert( -e localDir(). 'readonlyDirectory_PERM_M_/cannotBeRemoved_PERM_WVN_.data' );
assert( -e localDir(). 'readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data' );
assertLocalAndRemoteDir( '', 0);


#######################################################################
printInfo( "move a directory in a outside read only folder" );
system("sqlite3 " . localDir().'._sync_*.db .dump');

#Missing directory should be restored
#new directory should be uploaded
system("mv " . localDir().'readonlyDirectory_PERM_M_/subdir_PERM_CK_ ' . localDir().'normalDirectory_PERM_CKDNV_/subdir_PERM_CKDNV_'  );

csync();
system("sqlite3 " . localDir().'._sync_*.db .dump');
assertCsyncJournalOk(localDir());

# old name restored
assert( -e localDir(). 'readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_/' );
assert( -e localDir(). 'readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data' );

# new still exist
assert( -e localDir(). 'normalDirectory_PERM_CKDNV_/subdir_PERM_CKDNV_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data' );

assertLocalAndRemoteDir( '', 0);





#######################################################################
printInfo( "rename a directory in a read only folder and move a directory to a read-only" );

# do a sync to update the database
csync();

#1. rename a directory in a read only folder
#Missing directory should be restored
#new directory should stay but not be uploaded
system("mv " . localDir().'readonlyDirectory_PERM_M_/subdir_PERM_CK_ ' . localDir().'readonlyDirectory_PERM_M_/newname_PERM_CK_'  );

#2. move a directory from read to read only  (move the directory from previous step)
system("mv " . localDir().'normalDirectory_PERM_CKDNV_/subdir_PERM_CKDNV_ ' . localDir().'readonlyDirectory_PERM_M_/moved_PERM_CK_'  );

# error: can't upload to readonly!
csync(1);
assertCsyncJournalOk(localDir());

#1.
# old name restored
assert( -e localDir(). 'readonlyDirectory_PERM_M_/subdir_PERM_CK_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data' );

# new still exist
assert( -e localDir(). 'readonlyDirectory_PERM_M_/newname_PERM_CK_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data' );
# but is not on server: so remove for assertLocalAndRemoteDir
system("rm -r " . localDir(). "readonlyDirectory_PERM_M_/newname_PERM_CK_");

#2.
# old removed
assert( ! -e localDir(). 'normalDirectory_PERM_CKDNV_/subdir_PERM_CKDNV_/' );
# new still there
assert( -e localDir(). 'readonlyDirectory_PERM_M_/moved_PERM_CK_/subsubdir_PERM_CKDNV_/normalFile_PERM_WVND_.data' );
#but not on server
system("rm -r " . localDir(). "readonlyDirectory_PERM_M_/moved_PERM_CK_");

assertLocalAndRemoteDir( '', 0);

system("sqlite3 " . localDir().'._sync_*.db .dump');


#######################################################################
printInfo( "multiple restores of a file create different conflict files" );

system("sleep 1"); #make sure changes have different mtime

system("echo 'modified_1' > ". localDir() . "readonlyDirectory_PERM_M_/canotBeModified_PERM_DVN_.data");

#do the sync
csync();
assertCsyncJournalOk(localDir());

system("sleep 1"); #make sure changes have different mtime

system("echo 'modified_2' > ". localDir() . "readonlyDirectory_PERM_M_/canotBeModified_PERM_DVN_.data");

#do the sync
csync();
assertCsyncJournalOk(localDir());

# there should be two conflict files
# TODO check that the conflict file has the right content
my @conflicts = glob(localDir().'readonlyDirectory_PERM_M_/canotBeModified_PERM_DVN__conflict-*.data' );
assert( scalar @conflicts == 2 );
# remove the conflicts for the next assertLocalAndRemoteDir
system("rm " . localDir().'readonlyDirectory_PERM_M_/canotBeModified_PERM_DVN__conflict-*.data' );

### Both side should still be the same
assertLocalAndRemoteDir( '', 0);



cleanup();

















