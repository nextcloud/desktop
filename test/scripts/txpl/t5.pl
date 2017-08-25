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

print "Hello, this is t5, a tester for syncing of files in Shares\n";

initTesting();

# Create empty test dirs.
csync();

my $share_dir = "share_source";
my $sharee = { user   => configValue('share_user'),
               passwd => configValue('share_passwd'),
	       url    => server() };

# first remove a possibly left over share dir.
printInfo( "Remove possibly left over share dir" );
removeRemoteDir( $share_dir, $sharee );

printInfo( "Create a share." );
my $shareId = createShare( $share_dir, 31 );
print "Created share with id <$shareId>\n";

assert( $shareId > 0 );

if( $ENV{SERVER_VERSION} eq "owncloud6" ) {
  print "This test does not make more sense for ownCloud6, leaving for good!\n\n";
  exit;
} 

# put a couple of files into the shared directory in the sharer account
glob_put( 'sharing/*', $share_dir, $sharee);

# Move the shared dir remotely into the test dir, otherwise the script
# has a hard time to find it.
moveRemoteFile( server() . $share_dir, localDir(), 1 ); 

# call csync, sync local t1 to remote t1
printInfo("Initial sync, sync stuff down.");
csync();

assertLocalAndRemoteDir( '', 0 );

# Local file to a read/write share should be synced up
printInfo("Put a file into the share.");
createLocalFile(localDir() . "$share_dir/foobar.txt", 8094 );
csync( );
assertLocalAndRemoteDir( '', 0 );

# now move the file locally and sync
printInfo("Move the file locally and sync.");
my $cmd = "mv " . localDir() . "$share_dir/foobar.txt ". localDir() . "$share_dir/moved_file.txt";
system( $cmd );
csync( );
assertLocalAndRemoteDir( '', 0 );

# now create aother directory and redo
printInfo("Create another directory and file");
my $cmd = "mkdir ". localDir() . "$share_dir/newDir";
system( $cmd );
createLocalFile( localDir() . "$share_dir/newDir/a_file.bin", 5321 );
csync( );
assertLocalAndRemoteDir( '', 0 );

# Remove the local file again
printInfo("Remove the local file again.");
unlink( localDir() . "$share_dir/newDir/a_file.bin" );
csync( );
assertLocalAndRemoteDir( '', 0 );

# Remove the local directory again
printInfo("Remove the local directory again.");
rmdir( localDir() . "$share_dir/newDir" );
csync( );
assertLocalAndRemoteDir( '', 0 );



printInfo("Remove a Share.");
removeShare($shareId, $share_dir);
cleanup();

# --
