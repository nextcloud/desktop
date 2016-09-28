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

print "Hello, this is t_recall, a tester for the recall feature\n";

initTesting();

printInfo( "Syncing two files with the same name that differ with case" );

#create some files
my $tmpdir = "/tmp/t_recall/";
mkdir($tmpdir);
createLocalFile( $tmpdir . "file1.dat", 100 );
createLocalFile( $tmpdir . "file2.dat", 150 );
createLocalFile( $tmpdir . "file3.dat", 110 );
createLocalFile( $tmpdir . "file4.dat", 170 );

#put them in some directories
createRemoteDir( "dir" );
glob_put( "$tmpdir/*", "dir" );

csync();

assertLocalAndRemoteDir( '', 0);



printInfo( "Testing with a .sys.admin#recall#" );
system("echo 'dir/file2.dat' > ". $tmpdir . ".sys.admin\#recall\#");
system("echo 'dir/file3.dat' >> ". $tmpdir . ".sys.admin\#recall\#");
system("echo 'nonexistant' >> ". $tmpdir . ".sys.admin\#recall\#");
system("echo '/tmp/t_recall/file4.dat' >> ". $tmpdir . ".sys.admin\#recall\#");
glob_put( "$tmpdir/.sys.admin\#recall\#", "" );

csync();

#test that the recall files have been created
assert( -e glob(localDir().'dir/file2_.sys.admin#recall#-*.dat' ) );
assert( -e glob(localDir().'dir/file3_.sys.admin#recall#-*.dat' ) );

# verify that the original files still exist
assert( -e glob(localDir().'dir/file2.dat' ) );
assert( -e glob(localDir().'dir/file3.dat' ) );

assert( !-e glob(localDir().'nonexistant*' ) );
assert( !-e glob('/tmp/t_recall/file4_.sys.admin#recall#-*.dat' ) );
assert( -e glob('/tmp/t_recall/file4.dat' ) );

#Remove the recall file
unlink(localDir() . ".sys.admin#recall#");

# 2 sync necessary for the recall to be uploaded
csync();

assertLocalAndRemoteDir( '', 0);

printInfo( "Testing with a dir/.sys.admin#recall#" );
system("echo 'file4.dat' > ". $tmpdir . ".sys.admin\#recall\#");
glob_put( "$tmpdir/.sys.admin\#recall\#", "dir" );

csync();
assert( -e glob(localDir().'dir/file4_.sys.admin#recall#-*.dat' ) );


cleanup();
system("rm -r " . $tmpdir);

