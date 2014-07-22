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

print "Hello, this is t6, a tester for csync with ownCloud.\n";

initTesting();

sub chunkFileTest( $$ ) 
{
    my ($name, $size) = @_;

    # Big file chunking
    createLocalFile( localDir().$name, $size );
    assert( -e localDir().$name );
    
    my $bigMd5 = md5OfFile( localDir().$name );

    csync();
    my $newMd5 = md5OfFile( localDir().$name );
    assert( $newMd5 eq $bigMd5, "Different MD5 sums!" );

    # download
    my $ctrlFile = "/tmp/file.download";
    getToFileCurl( $name, $ctrlFile );

    assert( -e $ctrlFile, "File does not exist!" );

    # assert files
    my $dlMd5 = md5OfFile( $ctrlFile );
    assert( $dlMd5 eq $newMd5, "Different MD5 sums 2" );

    unlink( $ctrlFile );
}

printInfo("Big file that needs chunking with default chunk size");
chunkFileTest( "BIG.file", 23251233 );

# change the existing file again -> update
chunkFileTest( "BIG.file", 21762122 );

 # Set a custom chunk size in environment.
my $ChunkSize = 1*1024*1024;
$ENV{'OWNCLOUD_CHUNK_SIZE'} = $ChunkSize;

printInfo("Big file exactly as big as one chunk size");
chunkFileTest( "oneChunkSize.bin", $ChunkSize);

printInfo("Big file exactly as big as one chunk size minus 1 byte");
chunkFileTest( "oneChunkSizeminusone.bin", $ChunkSize-1);

printInfo("Big file exactly as big as one chunk size plus 1 byte");
chunkFileTest( "oneChunkSizeplusone.bin", $ChunkSize+1);

printInfo("Big file exactly as big as 2*chunk size");
chunkFileTest( "twoChunkSize.bin", 2*$ChunkSize);

printInfo("Big file exactly as big as 2*chunk size minus 1 byte");
chunkFileTest( "twoChunkSizeminusone.bin", 2*$ChunkSize-1);

printInfo("Big file exactly as big as 2*chunk size plus 1 byte");
chunkFileTest( "twoChunkSizeplusone.bin", 2*$ChunkSize+1);


# ==================================================================

cleanup();


# --
