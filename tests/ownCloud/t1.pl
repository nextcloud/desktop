#!/usr/bin/perl
#
# Test script for the ownCloud module of csync.
# This script requires a running ownCloud instance accessible via HTTP.
# It does quite some fancy tests and asserts the results.
#
# Copyright (C) by Klaas Freitag <freitag@owncloud.com>
#

use lib ".";

use Carp::Assert;
use Data::Dumper;
use HTTP::DAV;
use File::Copy;
use File::Glob ':glob';

use strict;

print "Hello, this is t1, a tester for csync with ownCloud.\n";

#
# Adjust data as needed here or better us a t1.cfg file with the following
# content:
#  user   => "joe",
#  passwd => "XXXXXX",
#  url    => "http://localhost/oc/remote.php/webdav/",
#  ld_libpath => "/home/kf/owncloud.com/buildcsync/modules",
#  csync => "/home/kf/owncloud.com/buildcsync/client/ocsync"


my $owncloud = "http://localhost/oc/remote.php/webdav/";
my $user = "joe";
my $passwd = 'XXXXX'; # Mind to be secure.
my $ld_libpath = "/home/kf/owncloud.com/buildcsync/modules";
my $csync = "/home/kf/owncloud.com/buildcsync/client/ocsync";


if( -r "./t1.cfg" ) {
    my %config = do 't1.cfg';
    warn "Could not parse t1.cfg: $!\n" unless %config;
    warn "Could not do t1.cfg: $@\n" if $@;

    $user   = $config{user} if( $config{user} );
    $passwd = $config{passwd} if( $config{passwd} );
    $owncloud = $config{url}  if( $config{url} );
    $ld_libpath = $config{ld_libpath} if( $config{ld_libpath} );
    $csync   = $config{csync} if( $config{csync} );
    print "Read t1.cfg: $config{url}\n";
}

$owncloud .= "/" unless( $owncloud =~ /\/$/ );


print "Connecting to ownCloud at ". $owncloud ."\n";


sub remoteDir( $$ )
{
    my ($d, $dir) = @_;

    my $url = $owncloud . $dir ;

    $d->open( $owncloud );
    print $d->message . "\n";

    my $re = $d->mkcol( $url );
    if( $re == 0 ) {
	print "Failed to create directory <$url>: $re\n";
	exit 1;
    }
    $d->open( $url );
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


    my $url = $owncloud;
    $url =~ s#^http://##;    # Remove the leading http://
    $url = "owncloud://$user:$passwd@". $url . $remote;
    print "CSync URL: $url\n";

    my $args = "--exclude-file=exclude.cfg -c";
    my $cmd = "LD_LIBRARY_PATH=$ld_libpath $csync $args $local $url";
    print "Starting: $cmd\n";

    system( $cmd ) == 0 or die("CSync died!\n");
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

sub assertFile($$)
{
  my ($localFile, $res) = @_;

   print "Asserting $localFile and " . $res->get_property("rel_uri") . "\n";

  my $remoteModTime = $res->get_property( "lastmodifiedepoch" ) ;
  my @info = stat( $localFile );
  my $localModTime = $info[9];
  assert( $remoteModTime == $localModTime, "Modfied-Times differ: remote: $remoteModTime <-> local: $localModTime" );
  print "local versuse Remote modtime: $localModTime <-> $remoteModTime\n";
  # check for the same file size
  my $localSize = $info[7];
  my $remoteSize = $res->get_property( "getcontentlength" );
  if( $remoteSize ) { # directories do not have a contentlength
    print "Local versus Remote size: $localSize <-> $remoteSize\n";
    assert( $localSize == $remoteSize, "File sizes differ!\n" );
  }
}

sub registerSeen($$)
{
  my ($seenRef, $file) = @_;

  $file =~ s/t1-\d+\//t1\//;
  $seenRef->{$file} = 1;
}

sub traverse( $$$$ )
{
    my ($d, $localDir, $remote, $acceptConflicts) = @_;
    printf("===============> $remote\n");
    my $url = $owncloud . $remote;
    my %seen;

    if( my $r = $d->propfind( -url => $url, -depth => 1 ) ) {

        if( $r->get_resourcelist ) {
	    foreach my $res ( $r->get_resourcelist->get_resources() ) {
		my $filename = $res->get_property("rel_uri");

		if( $res->is_collection ) {
		    # print "Checking " . $res-> get_uri()->as_string ."\n";
		    print "Traversing into directory: $filename\n";
		    my $dirname = $remote . $filename;
		    traverse( $d, $localDir, $dirname );
		    registerSeen( \%seen, $dirname );
		} else {
		    # Check files here.
		    print "Checking file: $remote\n";
		    my $localFile = $remote . $filename;
		    registerSeen( \%seen, $localFile );
		    $localFile =~ s/t1-\d+\//t1\//;

		    assertFile( $localFile, $res );
		}
	    }
	}
    } else {
        print "Propfind failed: " . $d->message() . "\n";
    }

    # Check the directory contents
    my $localpath = $remote;
    $localpath =~ s/t1-\d+\//t1\//;

    opendir(my $dh, $localpath ) || die;
    # print Dumper( %seen );
    while( readdir $dh ) {
	next if( /^\.+$/ );
	my $f = $localpath . $_;
	chomp $f;
	assert( -e $f );
	my $isHere = undef;
	if( exists $seen{$f} ) {
	    $isHere = 1;
	    $seen{$f} = 2;
	}
	if( !$isHere && exists $seen{$f . "/"} ) {
	    $isHere = 1;
	    $seen{$f."/"} = 3;
	}

	$isHere = 1 if( $acceptConflicts && !$isHere && $f =~ /_conflict/ );
	assert( $isHere, "Filename only local, but not remote: $f" );
    }

    # Check if there was something remote that we havent locally.
    foreach my $f ( keys %seen ) {
	assert( $seen{$f} > 1, "File on remote, but not locally: $f " . $seen{$f} );
    }
    # print Dumper %seen;
    print "<================ Done $remote\n";
    closedir $dh;
}

sub assertLocalAndRemoteDir( $$$$ )
{
    my ($d, $local, $remote, $acceptConflicts ) = @_;
    # %seen = ();
    traverse( $d, $local, $remote, $acceptConflicts );
}


sub glob_put( $$$ )
{
    my( $d, $globber, $target ) = @_;

    $d->open( $target );

    my @puts = bsd_glob( $globber );
    foreach my $lfile( @puts ) {
        if( $lfile =~ /.*\/(.+)$/g ) {
	    my $rfile = $1;
	    my $puturl = "$target"."$rfile";
	    print "   *** Putting $lfile to $puturl\n";
	    if( ! $d->put( -local=>$lfile, -url=> $puturl ) ) {
	      print "   ### FAILED to put: ". $d->message . '\n';
	    }
	}
    }
}

sub put_to_dir( $$$ )
{
    my ($d, $file, $dir) = @_;

    $d->open($dir);

    my $filename = $file;
    $filename =~ s/^.*\///;
    my $puturl = $dir. $filename;
    print "put_to_dir puts to $puturl\n";
    unless ($d->put( -local => $file, -url => $puturl )) {
      print "  ### FAILED to put a single file!\n";
    }
}

# ====================================================================

my $d = HTTP::DAV->new();

$d->credentials( -url=> $owncloud, -realm=>"ownCloud",
                 -user=> $user,
                 -pass=> $passwd );
# $d->DebugLevel(3);

my $remoteDir = sprintf( "t1-%#.3o/", rand(1000) );
print "Working in remote dir $remoteDir\n";

remoteDir( $d, $remoteDir );

# $d->open("localhost/oc/files/webdav.php/t1");
print "Copy some files to the remote location\n";
remoteDir( $d, $remoteDir . "remoteToLocal1" );
remoteDir( $d, $remoteDir . "remoteToLocal1/rtl1");
remoteDir( $d, $remoteDir . "remoteToLocal1/rtl1/rtl11");
remoteDir( $d, $remoteDir . "remoteToLocal1/rtl2");

# put some files remote.
glob_put( $d, 'toremote1/*', $owncloud . $remoteDir . "remoteToLocal1/" );
glob_put( $d, 'toremote1/rtl1/*', $owncloud . $remoteDir . "remoteToLocal1/rtl1/" );
glob_put( $d, 'toremote1/rtl1/rtl11/*', $owncloud . $remoteDir . "remoteToLocal1/rtl1/rtl11/" );
glob_put( $d, 'toremote1/rtl2/*', $owncloud . $remoteDir . "remoteToLocal1/rtl2/" );

my $localDir = "./t1";

print "Create the local sync dir $localDir\n";
createLocalDir( $localDir );

# call csync, sync local t1 to remote t1
csync( $localDir, $remoteDir );

# Check if the files from toremote1 are now in t1/remoteToLocal1
# they should have taken the way via the ownCloud.
print "Assert the local file copy\n";
assertLocalDirs( "./toremote1", "$localDir/remoteToLocal1" );

# Check if the synced files from ownCloud have the same timestamp as the local ones.
print "\nNow assert remote 'toremote1' with local \"$localDir/remoteToLocal1\" :\n";
assertLocalAndRemoteDir( $d, $localDir, $remoteDir . "remoteToLocal1/", 0);

# remove a local file.
print "\nRemove a local file\n";
unlink( "$localDir/remoteToLocal1/kernelcrash.txt" );
csync( $localDir, $remoteDir );
assertLocalAndRemoteDir( $d, $localDir, $remoteDir . "remoteToLocal1/", 0);

# add local files to a new dir1
print "\nAdd some more files to local:\n";
my $locDir = $localDir . "/fromLocal1";

mkdir( $locDir );
assert( -d $locDir );
foreach my $file ( <./tolocal1/*> ) {
    print "Copying $file to $locDir\n";
    copy( $file, $locDir );
}
csync( $localDir, $remoteDir );
print "\nAssert local and remote dirs.\n";
# assertLocalAndRemoteDir( $d, $locDir, $remoteDir . "fromLocal1" );
assertLocalAndRemoteDir( $d, $localDir, $remoteDir . "fromLocal1/", 0);

# move a local file
print "\nMove a file locally.\n";
move( "$locDir/kramer.jpg", "$locDir/oldtimer.jpg" );
csync( $localDir, $remoteDir );
# assertLocalAndRemoteDir( $d, $locDir, $remoteDir . "fromLocal1" );
assertLocalAndRemoteDir( $d, $localDir, $remoteDir . "fromLocal1/", 0);

# move a local directory.
print "\nMove a local directory.\n";
move( "$localDir/remoteToLocal1/rtl1", "$localDir/remoteToLocal1/rtlX");
csync( $localDir, $remoteDir );
# assertLocalAndRemoteDir( $d, $locDir, $remoteDir . "fromLocal1" );
assertLocalAndRemoteDir( $d, $localDir, $remoteDir . "fromLocal1/", 0);

# remove a local dir
print "\nRemove a local directory.\n";
localCleanup( "$localDir/remoteToLocal1/rtlX" );
csync( $localDir, $remoteDir );
# assertLocalAndRemoteDir( $d, $locDir, $remoteDir . "fromLocal1" );
assertLocalAndRemoteDir( $d, $localDir, $remoteDir . "fromLocal1/", 0);

# create a false conflict, only the mtimes are changed, by content are equal.
print "\nCreate a false conflict.\n";
put_to_dir( $d, 'toremote1/kernelcrash.txt', $owncloud . $remoteDir . "remoteToLocal1/" );
system( "sleep 2 && touch $localDir/remoteToLocal1/kernelcrash.txt" );
csync( $localDir, $remoteDir );
# assertLocalAndRemoteDir( $d, $locDir, $remoteDir . "fromLocal1" );
assertLocalAndRemoteDir( $d, $localDir, $remoteDir . "fromLocal1/", 0);

# create a true conflict.
print "\nCreate a conflict.\n";
system( "echo \"This is more stuff\" >> /tmp/kernelcrash.txt" );
put_to_dir( $d, '/tmp/kernelcrash.txt', $owncloud . $remoteDir . "remoteToLocal1/" );
system( "sleep 2 && touch $localDir/remoteToLocal1/kernelcrash.txt" );
csync( $localDir, $remoteDir );
# assertLocalAndRemoteDir( $d, $locDir, $remoteDir . "fromLocal1" );
assertLocalAndRemoteDir( $d, $localDir, $remoteDir . "remoteToLocal1/", 1);

# ==================================================================

print "\n###########################################\n";
print "    all cool - tests succeeded in $remoteDir.\n";
print "###########################################\n";

print "\nInterrupt before cleanup in 4 seconds...\n";
sleep(4);

remoteCleanup( $d, $remoteDir );
localCleanup( $localDir );


# end.
