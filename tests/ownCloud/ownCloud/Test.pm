#
# Copyright (c) 2013 Klaas Freitag <freitag@owncloud.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program (see the file COPYING); if not, write to the
# Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
#
################################################################
# Contributors:
#  Klaas Freitag <freitag@owncloud.com>
#
package ownCloud::Test;

use strict;
use Exporter;

use HTTP::DAV;
use Data::Dumper;
use File::Glob ':glob';
use Carp::Assert;
use Digest::MD5;
use Unicode::Normalize;
use Encode qw(from_to);
use utf8;
if ($^O eq "darwin") {
  eval "require Encode::UTF8Mac";
}

use vars qw( @ISA @EXPORT @EXPORT_OK $d %config);

our $owncloud   = "http://localhost/oc/remote.php/webdav/";
our $user       = "joe";
our $passwd     = 'XXXXX'; # Mind to be secure.
our $ld_libpath = "/home/kf/owncloud.com/buildcsync/modules";
our $csync      = "/home/kf/owncloud.com/buildcsync/client/ocsync";
our $remoteDir;
our $localDir   = "turbo";


@ISA        = qw(Exporter);
@EXPORT     = qw( initTesting createRemoteDir createLocalDir cleanup csync assertLocalDirs assertLocalAndRemoteDir
                  glob_put put_to_dir localDir remoteDir localCleanup createLocalFile);

sub fromFileName($)
{
  my ($file) = @_;
  if ( $^O eq "darwin" ) {
    my $fromFileName = NFC( Encode::decode('utf-8', $file) );
    return $fromFileName;
  } else {
    return $file;
  }
}


sub initTesting()
{
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
  $d = HTTP::DAV->new();

  $d->credentials( -url=> $owncloud, -realm=>"ownCloud",
		  -user=> $user,
		  -pass=> $passwd );
  # $d->DebugLevel(3);

  $remoteDir = sprintf( "t1-%#.3o/", rand(1000) );
  $localDir .= "/" unless( $localDir =~ /\/$/ );

  print "Working in remote dir $remoteDir\n";
  createLocalDir();
  createRemoteDir( $remoteDir );
  $owncloud .= $remoteDir;
  }

sub createRemoteDir($)
{
    my ($dir) = @_;

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

sub createLocalDir($)
{
    my ($dir) = (@_);

    $dir = $localDir . $dir;
    print "Creating local dir: $dir\n";
    mkdir( $dir, 0777 );
}

sub cleanup()
{
  # ==================================================================

  print "\n###########################################\n";
  print "    all cool - tests succeeded in $remoteDir.\n";
  print "###########################################\n";

  print "\nInterrupt before cleanup in 4 seconds...\n";
  sleep(4);

  remoteCleanup( );
  localCleanup( '' );

}

sub remoteCleanup( )
{
    $d->open( -url => $owncloud );

    print "Cleaning Remote!\n";

    my $re = $d->delete( $owncloud );

    if( $re == 0 ) {
	print "Failed to clenup directory <$owncloud>\n";
    }
    return $re;
}

sub localCleanup($)
{
    my ($dir) = @_;
    # don't play child games here:
    $dir = "$localDir/$dir";
    system( "rm -rf $dir" );
}

sub csync(  )
{
    my $url = $owncloud;
    $url =~ s#^http://##;    # Remove the leading http://
    $url = "owncloud://$user:$passwd@". $url;
    print "CSync URL: $url\n";

    my $args = "--exclude-file=exclude.cfg -c -d 11";
    my $cmd = "LD_LIBRARY_PATH=$ld_libpath $csync $args $localDir $url";
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
        next if( -d "$dir1/$_"); # don't compare directory sizes.
	my $s1 = -s "$dir1/$_";
	my $s2 = -s "$dir2/$_";
	assert( $s1 == $s2 );
    }
    closedir $dh;
}

sub localDir()
{
  return $localDir;
}

sub remoteDir()
{
  return $remoteDir;
}
#
# Check if a local and a remote dir have the same content
#

sub assertFile($$)
{
  my ($localFile, $res) = @_;

  print "Asserting $localFile and " . $res->get_property("rel_uri") . "\n";

  my $remoteModTime = $res->get_property( "lastmodifiedepoch" ) ;

  my $localFile2 = $localFile;
  if ($^O eq "darwin") {
    from_to($localFile2, 'utf-8-mac', 'utf-8');
  }
  my $stat_ok = stat( $localFile2 );
  print " *** STAT failed for $localFile2\n" unless( $stat_ok );
  my @info = stat( $localFile2 );
  my $localModTime = $info[9];
  assert( $remoteModTime == $localModTime, "Modified-Times differ: remote: $remoteModTime <-> local: $localModTime" );
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

sub traverse( $$ )
{
    my ($remote, $acceptConflicts) = @_;
    printf("===============> $remote\n");
    $remote .= '/' unless $remote =~ /\/$/;

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
		    traverse( $dirname, $acceptConflicts );
		    registerSeen( \%seen, $localDir . $dirname );
		} else {
		    # Check files here.
		    print "Checking file: $remote$filename\n";
		    my $localFile = $localDir . $remote . $filename;
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
    my $localpath = localDir().$remote;
    $localpath =~ s/t1-\d+\//t1\//;

    opendir(my $dh, $localpath ) || die;
    # print Dumper( %seen );
    while( readdir $dh ) {
	next if( /^\.+$/ );
	my $f = $localpath . fromFileName($_);
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
	$isHere = 1 if( $f =~ /\.csync/ );
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

sub assertLocalAndRemoteDir( $$ )
{
    my ($remote, $acceptConflicts ) = @_;
    # %seen = ();
    traverse( $remote, $acceptConflicts );
}

sub glob_put( $$ )
{
    my( $globber, $target ) = @_;

    $target = $owncloud . $target;

    $d->open( $target );

    my @puts = bsd_glob( $globber );
    foreach my $llfile( @puts ) {
	my $lfile = fromFileName($llfile);
        if( $lfile =~ /.*\/(.+)$/g ) {
	    my $rfile = $1;
	    my $puturl = "$target"."$rfile";
	    if( -d $lfile ) {
	      $d->mkcol( $puturl );
	    } else {
	      print "   *** Putting $lfile to $puturl\n";

	      if( ! $d->put( -local=>$lfile, -url=> $puturl ) ) {
		print "   ### FAILED to put: ". $d->message . '\n';
	      }
	    }
	}

    }
}

sub put_to_dir( $$ )
{
    my ($file, $dir) = @_;

    $dir .="/" unless $dir =~ /\/$/;
    $d->open($dir);

    my $filename = $file;
    $filename =~ s/^.*\///;
    my $puturl = $owncloud . $dir. $filename;
    print "put_to_dir puts to $puturl\n";
    unless ($d->put( -local => $file, -url => $puturl )) {
      print "  ### FAILED to put a single file!\n";
    }
}

sub createLocalFile( $$ )
{
  my ($fname, $size) = @_;
  $size = 1024 unless( $size );

  my $md5 = Digest::MD5->new;

  open(FILE, ">", $localDir . $fname) or die "Can't open $fname for writing ($!)";

  my $minimum = 32;
  my $range = 96;

  for (my $bytes = 0; $bytes < $size; $bytes += 4) {
    my $rand = int(rand($range ** 4));
    my $string = '';
    for (1..4) {
        $string .= chr($rand % $range + $minimum);
        $rand = int($rand / $range);
    }
    print FILE $string;
    $md5->add($string);
  }
  close FILE;
  return $md5->hexdigest;
}

#
