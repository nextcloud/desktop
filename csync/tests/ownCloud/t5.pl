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

use Carp::Assert;
use File::Copy;
use ownCloud::Test;

use strict;

print "Hello, this is t5, a tester for syncing of files in Shares\n";
# stat error occours on windsows when the file is busy for example

initTesting();

my $share_dir = "share_source";

printInfo( "Create a share." );
my $shareId = createShare( $share_dir, 31 );
print "Created share with id <$shareId>\n";

assert( $shareId > 0 );

my $sharee = { user => configValue('share_user'),
               passwd => configValue('share_passwd'),
	       url => server() };
# put a couple of files into the shared directory in the sharer account
glob_put( 'sharing/*', $share_dir, $sharee);

# now user kf has a new directory in shared.

# call csync, sync local t1 to remote t1
printInfo("Initial sync, sync stuff down.");
csync( server()."Shared" );
assertLocalAndRemoteDir( 'Shared', 0, server() );

# Local file to a read/write share should be synced up
printInfo("Put a file into the share.");
createLocalFile( localDir(). $share_dir . "/foobar.txt", 8094 );
csync( server()."Shared" );
assertLocalAndRemoteDir( 'Shared', 0, server() );


printInfo("Remove a Share.");
removeShare($shareId, $share_dir);
cleanup();

# --
