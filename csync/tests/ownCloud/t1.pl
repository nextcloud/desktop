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
assertLocalAndRemoteDir( '', 0);

# remove a local file.
printInfo( "\nRemove a local file\n" );
unlink( localDir() . 'remoteToLocal1/rtl4/quitte.pdf' );
csync();
assertLocalAndRemoteDir( '', 0);

# add local files to a new dir1
printInfo( "Add some more files to local:");
my $locDir = localDir() . 'fromLocal1';

mkdir( $locDir );
assert( -d $locDir );
foreach my $file ( <./tolocal1/*> ) {
    print "Copying $file to $locDir\n";
    copy( $file, $locDir );
}

# Also add a file with symbols
my $symbolName = "a\%b#c\$d-e";
system( "echo \"my symbols\" >>  $locDir/$symbolName" );

#Also on the server
put_to_dir( "$locDir/$symbolName", 'remoteToLocal1' );


csync( );
print "\nAssert local and remote dirs.\n";
assertLocalAndRemoteDir( '', 0);
assert( ! -e localDir().$symbolName );

# move a local file
printInfo( "Move a file locally." );
move( "$locDir/kramer.jpg", "$locDir/oldtimer.jpg" );
csync( );
assertLocalAndRemoteDir( '', 0);

# move a local directory.
printInfo( "Move a local directory." );
move( localDir() . 'remoteToLocal1/rtl1', localDir(). 'remoteToLocal1/rtlX');
csync();
assertLocalAndRemoteDir( '', 0);

# remove a local dir
printInfo( "Remove a local directory.");
localCleanup( 'remoteToLocal1/rtlX' );
csync();
assertLocalAndRemoteDir( '', 0);
assert( ! -e localDir().'remoteToLocal1/rtlX' );

# create twos false conflict, only the mtimes are changed, by content are equal.
printInfo( "Create two false conflict.");
put_to_dir( 'toremote1/kernelcrash.txt', 'remoteToLocal1' );
put_to_dir( 'toremote1/kraft_logo.gif', 'remoteToLocal1' );
# don't wait so mtime are likely the same on the client and the server.
system( "touch " . localDir() . "remoteToLocal1/kraft_logo.gif" );
# wait two second so the mtime are different
system( "sleep 2 && touch " . localDir() . "remoteToLocal1/kernelcrash.txt" );


csync( );
assertLocalAndRemoteDir( '', 0);

# The previous sync should have updated the etags, and this should NOT be a conflict
printInfo( "Update the file again");

my $f1 = localDir() . "remoteToLocal1/kernelcrash.txt";
my $s1 = 2136;
createLocalFile( $f1, $s1);

# stat the file
my @stat1 = stat $f1;
print "Updating File $f1 to $s1, size is $stat1[7]\n";


my $f2 = localDir() . "remoteToLocal1/kraft_logo.gif";
my $s2 = 2332;

createLocalFile( $f2, $s2);
# stat the file
my @stat2 = stat $f2;
print "Updating File $f2 to $s2, size is $stat2[7]\n";

system( "sleep 2 && touch " . localDir() . "remoteToLocal1/kernelcrash.txt" );
csync( );
assertLocalAndRemoteDir( '', 0);

# create a true conflict.
printInfo( "Create a conflict." );
system( "echo \"This is more stuff\" >> /tmp/kernelcrash.txt" );
put_to_dir( '/tmp/kernelcrash.txt', 'remoteToLocal1' );
system( "sleep 2 && touch " . localDir() . "remoteToLocal1/kernelcrash.txt" );
csync();
assertLocalAndRemoteDir( '', 1);

my $localMD5 = md5OfFile( localDir().'remoteToLocal1/kernelcrash.txt' );
my $realMD5 = md5OfFile( '/tmp/kernelcrash.txt' );
print "MD5 compare $localMD5 <-> $realMD5\n";
assert( $localMD5 eq $realMD5 );
assert(  glob(localDir().'remoteToLocal1/kernelcrash_conflict-*.txt' ) );
system("rm " . localDir().'remoteToLocal1/kernelcrash_conflict-*.txt' );


# prepare test for issue 1329, rtlX need to be modified
# [https://github.comowncloud/client/issues/1329]
printInfo( "Add a local directory");
system("cp -r 'toremote1/rtl1/'  '" . localDir(). "remoteToLocal1/rtlX'");
csync();
assertLocalAndRemoteDir( '', 0);

# remove a local dir (still for issue 1329)
printInfo( "Remove that directory.");
localCleanup( 'remoteToLocal1/rtlX' );
csync();
assertLocalAndRemoteDir( '', 0);
assert( ! -e localDir().'remoteToLocal1/rtlX' );


# add it back again  (still for issue 1329)
printInfo( "Add back the local dir.");
system("cp -r 'toremote1/rtl1/'  '" . localDir(). "remoteToLocal1/rtlX'");
assert( -e localDir().'remoteToLocal1/rtlX' );
assert( -e localDir().'remoteToLocal1/rtlX/rtl11/file.txt' );
csync();
assertLocalAndRemoteDir( '', 0);
assert( -e localDir().'remoteToLocal1/rtlX' );
assert( -e localDir().'remoteToLocal1/rtlX/rtl11/file.txt' );

printInfo( "Remove a directory on the server with new files on the client");
removeRemoteDir('remoteToLocal1/rtlX');
system("echo hello > " . localDir(). "remoteToLocal1/rtlX/rtl11/hello.txt");
csync();
assertLocalAndRemoteDir( '', 0);
# file.txt must be gone because the directory was removed on the server, but hello.txt must be there
#   as it is a new file
assert( ! -e localDir().'remoteToLocal1/rtlX/rtl11/file.txt' );
assert( -e localDir().'remoteToLocal1/rtlX/rtl11/hello.txt' );




# ==================================================================

cleanup();

# --

