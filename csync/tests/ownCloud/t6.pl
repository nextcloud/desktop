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

print "Hello, this is t6, a tester for csync with ownCloud.\n";

initTesting();

sub createPostUpdateScript($)
{
  my ($name) = @_;

    my $srcFile = localDir().'BIG1.file';
    my $cred = configValue("user") . ":" . configValue("passwd");
    my $cmd = "curl -T $srcFile -u $cred --insecure " . testDirUrl().$name;
    my $script = "/tmp/post_update_script.sh";
    open SC, ">$script" || die("Can not create script file");
    print SC "#!/bin/bash\n";
    print SC "$cmd\n";
    close SC;
    chmod 0755, $script;

    return $script;
}

sub getETagFromJournal($$)
{
    my ($name,$num) = @_;

    my $sql = "sqlite3 " . localDir() . ".csync_journal.db \"SELECT md5 FROM metadata WHERE path='$name';\"";
    open(my $fh, '-|', $sql) or die $!;
    my $etag  = <$fh>;
    close $fh;
    print "$num etag: $etag";

    return $etag;
}

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
chunkFileTest( "BIG1.file", 23251233 );

printInfo("Update the existing file and trigger reupload");
# change the existing file again -> update
chunkFileTest( "BIG2.file", 21762122 );

printInfo("Cause a precondition failed error");
# Now overwrite the existing file to change it
createLocalFile( localDir()."BIG3.file", 21832 );
sleep(2);
csync();
createLocalFile( localDir().'BIG3.file', 34323 );
sleep(2);
# and create a post update script
my $script = createPostUpdateScript('BIG3.file');
$ENV{'OWNCLOUD_POST_UPDATE_SCRIPT'} = $script;

# Save the etag before the sync
my $firstETag = getETagFromJournal('BIG3.file', 'First');
sleep(2);
csync(); # Sync, which ends in a precondition failed error
# get the etag again. It has to be unchanged because of the error.
my $secondETag = getETagFromJournal('BIG3.file', 'Second');

# Now the result is that there is a conflict file because since 1.7
# the sync is stopped on preconditoin failed and done again.
my $seen = 0;
opendir(my $dh, localDir() );
while(readdir $dh) {
  $seen = 1 if ( /BIG3_conflict.*\.file/ );
}
closedir $dh;
assert( $seen == 1, "No conflict file created on precondition failed!" );
unlink($script);
$ENV{'OWNCLOUD_POST_UPDATE_SCRIPT'} = "";

assertLocalAndRemoteDir( '', 1);


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

printInfo("Big file with many chunks");
chunkFileTest( "bigFileManyChunk.bin", 10*$ChunkSize);


printInfo("Big file with many chunks reuploaded twice (1)");
createLocalFile( "BIG4.file", 21762122 );
csync();
assertLocalAndRemoteDir( '', 1);


printInfo("Big file with many chunks reuploaded twice (2)");

createLocalFile( "BIG4.file", 21783424 );
csync();
assertLocalAndRemoteDir( '', 1);



# ==================================================================

cleanup();


# --
