#!/usr/bin/perl
#
# Test script for the ownCloud module of csync. 
# This script requires a running ownCloud instance accessible via HTTP.
# It does quite some fancy tests and asserts the results.
#
# Copyright (C) by Klaas Freitag <freitag@owncloud.com>
#

use Carp::Assert;
use HTTP::DAV;
use Data::Dumper;
use File::Copy;

use strict;

print "Hello, this is t1, a tester for csync with ownCloud.\n";

#
# Adjust data as needed here:
my $owncloud = "http://localhost/oc/files/webdav.php/";
my $user = "joe";
my $passwd = 'XXXXX'; # Mind to be secure.


print "Connecting to ownCloud at ". $owncloud ."\n";


sub remoteDir( $$ )
{
    my ($d, $dir) = @_;

    my $url = $owncloud . $dir ;

    $d->open( -url => $owncloud );
    print $d->message . "\n";

    my $re = $d->mkcol( $url );
    if( $re == 0 ) {
	print "Failed to create directory <$dir>\n";
    }
    return $re;
}

sub createLocalDir( $  )
{
    my ($dir) = (@_);

    mkdir( $dir, 0777 );
}

sub remoteCleanup( $;$ )
{
    my ($d, $dir) = @_;

    $dir = "t1" unless $dir;
    my $url = $owncloud . $dir;
    $d->open( -url => $owncloud );

    print "Cleaning Remote!\n";
    
    my $re = $d->delete( $dir );

    if( $re == 0 ) {
	print "Failed to clenup directory <$dir>\n";
    }
    return $re;
}

sub localCleanup( $ )
{
    my ($dir) = @_;
    # don't play child games here:
    system( "rm -rf $dir" );
}

sub csync( $$ )
{
    my ($local, $remote) = @_;

    my $ld_libpath = "/home/kf/owncloud.com/buildcsync/modules";
    my $csync = "/home/kf/owncloud.com/buildcsync/client/csync";

    my $url = $owncloud;
    $url =~ s#^http://##;    # Remove the leading http://
    $url = "owncloud://$user:$passwd@". $url . $remote;
    print "CSync URL: $url\n";
    
    my $cmd = "LD_LIBRARY_PATH=$ld_libpath $csync $local $url";
    print "Starting: $cmd\n";

    system( $cmd );
}

#
# Check local directories if they have the same content. 
#
sub assertLocalDirs( $$ )
{
    my ($dir1, $dir2) = @_;
    print "Asserting $dir1 <-> $dir2\n";

    opendir(my $dh, $dir1 ) || die;
    while(readdir $dh) {
	assert( -e "$dir2/$_" );

	my $s1 = -s "$dir1/$_";
	my $s2 = -s "$dir2/$_";
	assert( $s1 == $s2 );
    }
    closedir $dh;
}

#
# Check if a local and a remote dir have the same content 
#
sub assertLocalAndRemoteDir( $$$ )
{
    my ($d, $local, $remote) = @_;
    my %seen;

    if( my $r = $d->propfind( -url => $owncloud . $remote, -depth => 1 ) ) {
	if( $r->is_collection ) {
	    print "\nXX" . $r->get_resourcelist->as_string ."\n";
	    
	    foreach my $res ( $r->get_resourcelist->get_resources() ) {
		print "Checking " . $res-> get_uri()->as_string ."\n";
		my $filename = $res->get_property("rel_uri");
		# check if the file exists.
		assert( -e "$local/$filename" );

		# check for equal mod times
		my $remoteModTime = $res->get_property( "lastmodifiedepoch" ) ;
		my @info = stat( "$local/$filename" );
		my $localModTime = $info[8];
		assert( $remoteModTime == $localModTime, "Modfied-Times differ: $remoteModTime <-> $localModTime" );
		
		# check for the same file size
		my $localSize = $info[7];
		my $remoteSize = $res->get_property( "getcontentlength" );
		assert( $localSize == $remoteSize );

		# remember the files seen on the server.
		$seen{$filename} = 1;
            }
	}
	# Now loop over the local directory content and check if all files in the dir
	# were seen on the server.
	    
        print "\n* Cross checking with local dir: \n";

	opendir(my $dh, $local ) || die;
	while( readdir $dh ) {
	    next if( /^\.+$/ );
	    assert( -e "$local/$_" );
	    assert( $seen{$_} == 1, "Filename only local, but not remte: $_\n" );
	}
    closedir $dh;
	
    }

}
# ====================================================================

my $d = HTTP::DAV->new();


$d->credentials( -url=> $owncloud, -realm=>"ownCloud",
                 -user=> $user,
                 -pass=> $passwd );
$d->DebugLevel(1);

my $remoteDir = "t1/";

remoteDir( $d, $remoteDir );

# $d->open("localhost/oc/files/webdav.php/t1");
remoteDir( $d, $remoteDir . "remoteToLocal1" );

# put some files remote.
$d->put( -local=>"toremote1/*", -url=> $owncloud . $remoteDir . "remoteToLocal1" );

# 
my $localDir = "./t1";

createLocalDir( $localDir );

# call csync, sync local t1 to remote t1
csync( $localDir, $remoteDir );

print "\nNow assertions:\n";

# Check if the files from toremote1 are now in t1/remoteToLocal1
# they should have taken the way via the oncCloud.
assertLocalDirs( "./toremote1", "$localDir/remoteToLocal1" );

# Check if the synced files from ownCloud have the same timestamp as the local ones.
assertLocalAndRemoteDir( $d, "$localDir/remoteToLocal1", $remoteDir . "remoteToLocal1" );

# remove a local file.
unlink( "$localDir/remoteToLocal1/kernelcrash.txt" );
csync( $localDir, $remoteDir );
assertLocalAndRemoteDir( $d, "$localDir/remoteToLocal1", $remoteDir . "remoteToLocal1" );

# add local files to a new dir1
my $locDir = $localDir . "/fromLocal1";

mkdir( $locDir );
assert( -d $locDir );
foreach my $file ( <./tolocal1/*> ) {
    print "Copying $file to $locDir\n";
    copy( $file, $locDir );
}
csync( $localDir, $remoteDir );
assertLocalAndRemoteDir( $d, $locDir, $remoteDir . "fromLocal1" );


print "\nInterrupt before cleanup in 4 seconds...\n";
sleep(4);

remoteCleanup( $d, $remoteDir );
localCleanup( $localDir );


# end.