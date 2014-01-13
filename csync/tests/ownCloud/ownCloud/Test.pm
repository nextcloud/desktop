#
# Copyright (c) 2013 Klaas Freitag <freitag@owncloud.com>
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
use LWP::UserAgent;
use HTTP::Request::Common;
use File::Basename;

use Encode qw(from_to);
use utf8;
if ($^O eq "darwin") {
  eval "require Encode::UTF8Mac";
}

use open ':encoding(utf8)';

use vars qw( @ISA @EXPORT @EXPORT_OK $d %config);

our $owncloud   = "http://localhost/oc/remote.php/webdav/";
our $user       = "joe";
our $passwd     = 'XXXXX'; # Mind to be secure.
our $ld_libpath = "/home/joe/owncloud.com/buildcsync/modules";
our $csync      = "/home/joe/owncloud.com/buildcsync/client/ocsync";
our $remoteDir;
our $localDir;
our $infoCnt = 1;


@ISA        = qw(Exporter);
@EXPORT     = qw( initTesting createRemoteDir createLocalDir cleanup csync
                  assertLocalDirs assertLocalAndRemoteDir glob_put put_to_dir 
                  putToDirLWP localDir remoteDir localCleanup createLocalFile md5OfFile
                  remoteCleanup server initLocalDir initRemoteDir moveRemoteFile
                  printInfo remoteFileId);

sub server
{
  return $owncloud;
}
                  
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


sub initTesting(;$)
{
  my ($prefix) = @_;
  
  if( -r "./t1.cfg" ) {
    my %config = do 't1.cfg';
    warn "Could not parse t1.cfg: $!\n" unless %config;
    warn "Could not do t1.cfg: $@\n" if $@;

    $user       = $config{user} if( $config{user} );
    $passwd     = $config{passwd} if( $config{passwd} );
    $owncloud   = $config{url}  if( $config{url} );
    $ld_libpath = $config{ld_libpath} if( $config{ld_libpath} );
    $csync      = $config{csync} if( $config{csync} );
    print "Read t1.cfg: $config{url}\n";
  }

  $owncloud .= "/" unless( $owncloud =~ /\/$/ );


  print "Connecting to ownCloud at ". $owncloud ."\n";
  $d = HTTP::DAV->new();

  $d->credentials( -url=> $owncloud, -realm=>"ownCloud",
		  -user=> $user,
		  -pass=> $passwd );
  # $d->DebugLevel(3);
  $prefix = "t1" unless( defined $prefix );
  
  my $dirId = sprintf("%#.3o", rand(1000));
  my $dir = sprintf( "%s-%s/", $prefix, $dirId );
  
  $localDir = $dir;
  $localDir .= "/" unless( $localDir =~ /\/$/ );
  $remoteDir = $dir;
  
  initRemoteDir();
  initLocalDir();
  printf( "Test directory name is %s\n", $dir );
}

# Call this first to create the unique test dir stored in
# the global var $remoteDir;
sub initRemoteDir
{
  $d->open( $owncloud );
  $owncloud .= $remoteDir;
  
  my $re = $d->mkcol( $owncloud );
  if( $re == 0 ) {
    print "Failed to create test dir $owncloud\n";
    exit 1;
  }
  # $owncloud .= $remoteDir;
}

sub initLocalDir
{
  mkdir ($localDir, 0777 );
}

sub createRemoteDir(;$)
{
    my ($dir) = @_;

    my $url = $owncloud . $dir;

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

  print "\n################################################\n";
  printf( "    all cool - %d tests succeeded in %s.\n", $infoCnt-1, $remoteDir);
  print "#################################################\n";

  print "\nInterrupt before cleanup in 4 seconds...\n";
  sleep(4);

  remoteCleanup( '' );
  localCleanup( '' );

}

sub remoteCleanup($)
{
    my ($dir) = @_;
    $d->open( -url => $owncloud . $dir );

    print "Cleaning Remote!\n";

    my $re = $d->delete( $owncloud . $dir );

    if( $re == 0 ) {
	print "Failed to clenup directory <$owncloud $dir>\n";
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

    my $args = ""; # "--exclude-file=exclude.cfg -c";
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
	assert( $s1 == $s2, "$dir1/$_ <-> $dir2/$_" );
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
    assert( $localSize == $remoteSize, "File sizes differ" );
  }
}

sub registerSeen($$)
{
  my ($seenRef, $file) = @_;
  $seenRef->{$file} = 1;
}

sub traverse( $$ )
{
    my ($remote, $acceptConflicts) = @_;
    $remote .= '/' unless $remote =~ /(^|\/)$/;
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
		    traverse( $dirname, $acceptConflicts );
		    registerSeen( \%seen, $localDir . $dirname );
		} else {
		    # Check files here.
		    print "Checking file: $remote$filename\n";
		    my $localFile = $localDir . $remote . $filename;
		    registerSeen( \%seen, $localFile );
		    # $localFile =~ s/t1-\d+\//t1\//;

		    assertFile( $localFile, $res );
		}
	    }
	}
    } else {
        print "Propfind failed: " . $d->message() . "\n";
    }

    # Check the directory contents
    my $localpath = localDir();
    $localpath .= $remote if( $remote ne "/" );
  print "#### localpath = " . $localpath . "\n";
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
	assert( $isHere, "Filename local, but not remote: $f" );
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

    # $target = $owncloud . $target;
    
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
	    $lfile = $llfile;
	    $puturl = $target;
	      print "   *** Putting $lfile to $puturl\n";
	      # putToDirLWP( $lfile, $puturl );
	      put_to_dir($lfile, $puturl);
	      
	      # if( ! $d->put( -local=>$lfile, -url=> $puturl ) ) {
	      #print "   ### FAILED to put: ". $d->message . '\n';
	      # s}
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

# The HTTP DAV module often does a PROPFIND before it really PUTs. That 
# is not neccessary if we know that the directory is really there.
# Use this function in this case:
sub putToDirLWP($$)
{
    my ($file, $dir) = @_;

    $dir .="/" unless $dir =~ /\/$/;

    my $filename = $file;
    my $basename = basename $filename;

    $dir =~ s/^\.\///;
    my $puturl = $owncloud . $dir. $basename;
    # print "putToDir LWP puts $filename to $puturl\n";
    die("Could not open $filename: $!") unless( open FILE, "$filename" );
    binmode FILE,  ":utf8";;
    my $string = <FILE>;
    close FILE;

    my $ua  = LWP::UserAgent->new();
    $ua->agent( "ownCloudTest_$localDir");
    my $req = PUT $puturl, Content_Type => 'application/octet-stream',
		  Content => $string;
    $req->authorization_basic($user, $passwd);
    my $response = $ua->request($req);
    
    if ($response->is_success()) {
      # print "OK: ", $response->content;
    } else {
      die( "HTTP PUT failed: " . $response->as_string );
    }
}

sub createLocalFile( $$ ) 
{
  my ($fname, $size) = @_;
  $size = 1024 unless( $size );
  
  my $md5 = Digest::MD5->new;

  open(FILE, ">", $fname) or die "Can't open $fname for writing ($!)";
  
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

sub md5OfFile( $ ) 
{
  my ($file) = @_;
  
  open FILE, "$file";

  my $ctx = Digest::MD5->new;
  $ctx->addfile (*FILE);
  my $hash = $ctx->hexdigest;
  close (FILE);
  
  return $hash;
}

sub moveRemoteFile($$)
{
  my ($from, $to) = @_;
    
  my $fromUrl = $owncloud . $from;
  my $toUrl = $owncloud . $to;
  
  $d->move($fromUrl, $toUrl);
  
}

sub printInfo($)
{
  my ($info) = @_;
  my $tt = 6+length( $info );
  
  print "#" x $tt;
  printf( "\n# %2d. %s", $infoCnt, $info );
  print "\n" unless $info =~ /\n$/;
  print "#" x $tt;
  print "\n";
  
  $infoCnt++;
}

sub remoteFileId($$)
{
  my ($fromDir, $file) = @_;
  my $fromUrl = $owncloud . $fromDir;
  my $id;

  if( my $r = $d->propfind( -url => $fromUrl, -depth => 1 ) ) {
    if ( $r->is_collection ) {
      # print "Collection\n";

      foreach my $res ( $r->get_resourcelist->get_resources() ) {
	my $filename = $res->get_property("rel_uri");
	# print "OOOOOOOOOOOOOO $filename " . $res->get_property('id') . "\n";
	if( $file eq $filename || $filename eq $file . "/" ) {
	  $id = $res->get_property('id') || "";
	}
      }
    } else {
      # print "OOOOOOOOOOOOOOOOOOO " . $r->get_property("rel_uri");
      $id = $r->get_property('id') || "";
    }
  }
  print "## ID of $file: $id\n";
  return $id;
}

#
