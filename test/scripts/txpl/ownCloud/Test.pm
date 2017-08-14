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

use HTTP::DAV 0.47;
use Data::Dumper;
use File::Glob ':glob';
use Digest::MD5;
use Unicode::Normalize;
use LWP::UserAgent;
use LWP::Protocol::https;
use HTTP::Request::Common qw( POST GET DELETE );
use File::Basename;
use IO::Handle;
use POSIX qw/strftime/;
use Carp;

use Encode qw(from_to);
use utf8;
if ($^O eq "darwin") {
  eval "require Encode::UTF8Mac";
}

use open ':encoding(utf8)';

use vars qw( @ISA @EXPORT @EXPORT_OK $d %config);

our $owncloud   = "http://localhost/oc/remote.php/webdav/";
our $owncloud_plain; # the server url without the uniq testing dir
our $user       = "joe";
our $passwd     = 'XXXXX'; # Mind to be secure.
our $ld_libpath = "/home/joe/owncloud.com/buildcsync/modules";
our $csync      = "/home/joe/owncloud.com/buildcsync/client/ocsync";
our $ocs_url;
our $share_user;
our $share_passwd;
our $remoteDir;
our $localDir;
our $infoCnt = 1;
our %config;

@ISA        = qw(Exporter);
@EXPORT     = qw( initTesting createRemoteDir removeRemoteDir createLocalDir cleanup csync
                  assertLocalDirs assertLocalAndRemoteDir glob_put put_to_dir
                  putToDirLWP localDir remoteDir localCleanup createLocalFile md5OfFile
                  remoteCleanup server initLocalDir initRemoteDir moveRemoteFile
                  printInfo remoteFileProp remoteFileId createShare removeShare assert
                  configValue testDirUrl getToFileLWP getToFileCurl);

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

sub setCredentials
{
  my ($dav, $user, $passwd) = @_;

  $dav->credentials(-url=> $owncloud, -realm=>"sabre/dav",
                    -user=> $user, -pass=> $passwd);
  $dav->credentials(-url=> $owncloud, -realm=>"ownCloud",
                    -user=> $user, -pass=> $passwd);
}


sub initTesting(;$)
{
  my ($prefix) = @_;

  my $cfgFile = "./t1.cfg";
  $cfgFile = "/etc/ownCloud/t1.cfg" if( -r "/etc/ownCloud/t1.cfg" );

  if( -r "$cfgFile" ) {
    %config = do $cfgFile;
    warn "Could not parse t1.cfg: $!\n" unless %config;
    warn "Could not do t1.cfg: $@\n" if $@;

    $user         = $config{user} if( $config{user} );
    $passwd       = $config{passwd} if( $config{passwd} );
    $owncloud     = $config{url}  if( $config{url} );
    $ld_libpath   = $config{ld_libpath} if( $config{ld_libpath} );
    $csync        = $config{csync} if( $config{csync} );
    $ocs_url      = $config{ocs_url} if( $config{ocs_url} );
    $share_user   = $config{share_user} if( $config{share_user} );
    $share_passwd = $config{share_passwd} if( $config{share_passwd} );

    print "Read config from $cfgFile: $config{url}\n";
  } else {
    print STDERR "Could not read a config file $cfgFile\n";
    exit(1);
  }

  $owncloud .= "/" unless( $owncloud =~ /\/$/ );

  $ENV{PERL_LWP_SSL_VERIFY_HOSTNAME} = 0;

  print "Connecting to ownCloud at ". $owncloud ."\n";

  # For SSL set the environment variable needed by the LWP module for SSL
  if( $owncloud =~ /^https/ ) {
    $ENV{PERL_LWP_SSL_VERIFY_HOSTNAME} = 0
  }

  my $ua = HTTP::DAV::UserAgent->new(keep_alive => 1 );
  $d = HTTP::DAV->new(-useragent => $ua);

  setCredentials($d, $user, $passwd);
  # $d->DebugLevel(3);
  $prefix = "t1" unless( defined $prefix );

  my $dirId = sprintf("%02d", rand(100));
  my $dateTime = strftime('%Y%m%d%H%M%S',localtime);
  my $dir = sprintf( "%s-%s-%s/", $prefix, $dateTime, $dirId );

  $localDir = $dir;
  $localDir .= "/" unless( $localDir =~ /\/$/ );
  $remoteDir = $dir;

  initRemoteDir();
  initLocalDir();
  printf( "Test directory name is %s\n", $dir );
}

sub configValue($;$)
{
    my ($configName, $default) = @_;

    if( $config{$configName} ) {
	return $config{$configName} ;
    } else {
        return $default;
    }
}

# Returns the full url to the testing dir, ie.
# http://localhost/owncloud/remote.php/webdav/t1-0543
sub testDirUrl()
{
    print "WARN: Remote dir still empty, first call initRemoteDir!\n" unless($remoteDir);
    return $owncloud . $remoteDir;
}

# Call this first to create the unique test dir stored in
# the global var $remoteDir;
sub initRemoteDir
{
  $d->open( $owncloud )
       or die("Couldn't open $owncloud: " .$d->message . "\n");

  my $url = testDirUrl();

  my $re = $d->mkcol( $url );
  if( $re == 0 ) {
    print "Failed to create test dir $url\n";
    exit 1;
  }

}

sub initLocalDir
{
  mkdir ($localDir, 0777 );
}

sub removeRemoteDir($;$)
{
    my ($dir, $optionsRef) = @_;

    my $url = testDirUrl() . $dir;
    if( $optionsRef && $optionsRef->{user} && $optionsRef->{passwd} ) {
	setCredentials($d, $optionsRef->{user}, $optionsRef->{passwd});
	if( $optionsRef->{url} ) {
	    $url = $optionsRef->{url} . $dir;
	}
    }

    $d->open( $owncloud );
    print $d->message . "\n";

    my $re = $d->delete( $url );
    if( $re == 0 ) {
	print "Failed to remove directory <$url>:" . $d->message() ."\n";
    }

    return $re;
}

sub createRemoteDir(;$$)
{
    my ($dir, $optionsRef) = @_;

    my $url = testDirUrl() . $dir;

    if( $optionsRef && $optionsRef->{user} && $optionsRef->{passwd} ) {
	setCredentials($d, $optionsRef->{user}, $optionsRef->{passwd});
	if( $optionsRef->{url} ) {
	    $url = $optionsRef->{url} . $dir;
	}
    }

    $d->open( $owncloud );
    print $d->message . "\n";

    my $re = $d->mkcol( $url );
    if( $re == 0 ) {
	print "Failed to create directory <$url>: " . $d->message() ."\n";
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
    my $url = testDirUrl().$dir;
    $d->open( -url => $url );

    print "Cleaning Remote!\n";

    my $re = $d->delete( $url );

    if( $re == 0 ) {
	print "Failed to cleanup directory <$url>\n";
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

# parameter: the expected return code
sub csync( ;$ )
{
    my $expected = $_[0] // 0;

    my $url = testDirUrl();
    if( $url =~ /^https:/ ) {
	$url =~ s#^https://##;    # Remove the leading http://
	$url = "ownclouds://$user:$passwd@". $url;
    } elsif( $url =~ /^http:/ ) {
	$url =~ s#^http://##;
	$url = "owncloud://$user:$passwd@". $url;
    }

    print "CSync URL: $url\n";

    my $args = "--trust --exclude exclude.cfg"; # Trust crappy SSL certificates
    my $cmd = "LD_LIBRARY_PATH=$ld_libpath $csync $args $localDir $url";
    print "Starting: $cmd\n";

    my $result = system( $cmd );
    $result == ($expected << 8) or die("Wrong csync return code or crash! $result\n");
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
    assert( -e "$dir2/$_", " $dir2/$_  do not exist" );
        next if( -d "$dir1/$_"); # don't compare directory sizes.
	my $s1 = -s "$dir1/$_";
	my $s2 = -s "$dir2/$_";
	assert( $s1 == $s2, "$dir1/$_ <-> $dir2/$_   size not equal ($s1 != $s2)" );
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
  assert($stat_ok, "Stat failed for file $localFile");

  my @info = stat( $localFile2 );
  my $localModTime = $info[9];
  assert( $remoteModTime == $localModTime, "Modified-Times differ: remote: $remoteModTime <-> local: $localModTime" );
  print "local versuse Remote modtime: $localModTime <-> $remoteModTime\n";
  # check for the same file size
  my $localSize = $info[7];
  my $remoteSize = $res->get_property( "getcontentlength" );
  if( $remoteSize ) { # directories do not have a contentlength
    print "Local versus Remote size: $localSize <-> $remoteSize\n";
    # assert( $localSize == $remoteSize, "File sizes differ" ); # FIXME enable this again but it causes trouble on Jenkins all the time.
  }
}

sub registerSeen($$)
{
  my ($seenRef, $file) = @_;
  $seenRef->{$file} = 1;
}

sub traverse( $$;$ )
{
    my ($remote, $acceptConflicts, $aurl) = @_;
    $remote .= '/' unless $remote =~ /(^|\/)$/;

    my $url = testDirUrl() . $remote;
    if( $aurl ) {
	$url = $aurl . $remote;
    }
    printf("===============> $url\n");
    my %seen;


    setCredentials($d, $user, $passwd);
    $d->open( $owncloud );

    if( my $r = $d->propfind( -url => $url, -depth => 1 ) ) {

        if( $r->get_resourcelist ) {
	    foreach my $res ( $r->get_resourcelist->get_resources() ) {
		my $filename = $res->get_property("rel_uri");

		if( $res->is_collection ) {
		    # print "Checking " . $res-> get_uri()->as_string ."\n";
		    print "Traversing into directory: $filename\n";
		    my $dirname = $remote . $filename;
		    traverse( $dirname, $acceptConflicts, $aurl );
		    my $localDirName = $localDir . $dirname;
		    $localDirName =~ s/Shared\///g;
		    registerSeen( \%seen, $localDirName); #  . $dirname
		} else {
		    # Check files here.
		    print "Checking file: $remote$filename\n";
		    my $localFile = $localDir . $remote . $filename;
		    $localFile =~ s/Shared\///g;
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
    $localpath =~ s/Shared\///g;
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
	$isHere = 1 if( $f =~ /\._sync_/ );
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

sub assertLocalAndRemoteDir( $$;$ )
{
    my ($remote, $acceptConflicts, $aurl ) = @_;
    traverse( $remote, $acceptConflicts, $aurl );
}

#
# the third parameter is an optional hash ref that can contain
# the keys user, passwd and url for alternative connection settings
#
sub glob_put( $$;$ )
{
    my( $globber, $target, $optionsRef ) = @_;

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
	      put_to_dir($lfile, $puturl, $optionsRef);

	      # if( ! $d->put( -local=>$lfile, -url=> $puturl ) ) {
	      #print "   ### FAILED to put: ". $d->message . '\n';
	      # s}
	    }
	}

    }
}

sub put_to_dir( $$;$ )
{
    my ($file, $dir, $optionsRef) = @_;

    $dir .="/" unless $dir =~ /\/$/;
    my $targetUrl = testDirUrl();

    if( $optionsRef && $optionsRef->{user} && $optionsRef->{passwd} ) {
	setCredentials($d, $optionsRef->{user}, $optionsRef->{passwd});
	if( $optionsRef->{url} ) {
	    $targetUrl = $optionsRef->{url};
	}
    }
    $d->open($targetUrl . $dir);

    my $filename = $file;
    $filename =~ s/^.*\///;
    $filename =~ s/#/%23/g;  # poor man's URI encoder
    my $puturl = $targetUrl . $dir. $filename;

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
    my $puturl = testDirUrl() . $dir. $basename;
    # print "putToDir LWP puts $filename to $puturl\n";
    die("Could not open $filename: $!") unless( open FILE, "$filename" );
    binmode FILE,  ":utf8";;
    my $string = <FILE>;
    close FILE;

    my $ua  = LWP::UserAgent->new( ssl_opts => { verify_hostname => 0 });
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

# does a simple GET of a file in the testdir to a local file.

sub getToFileCurl( $$ )
{
    my ($file, $localFile) = @_;
    my $geturl = testDirUrl() . $file;
    print "GETting $geturl to $localFile\n";

    my @args = ("curl", "-k", "-u", "$user:$passwd", "$geturl", "-o", "$localFile");
    system( @args );
}

# FIXME: This does not work because I can not get an authenticated GET request
# that writes its content to a file. Strange.
sub getToFileLWP( $$ )
{
    my ($file, $localFile) = @_;
    my $geturl = testDirUrl() . $file;
    print "GETting $geturl to $localFile\n";

    my $ua  = LWP::UserAgent->new( ssl_opts => { verify_hostname => 0 });
    $ua->agent( "ownCloudTest_$localDir");
    $ua->credentials( server(), "foo", $user, $passwd);
    my $req = $ua->get($geturl, ":content_file" => $localFile);
    # my $req = HTTP::Request->new( GET => $geturl, ':content_file' => $localFile);
    # $req->authorization_basic("$user", "$passwd");
    # my $response = $ua->request($req);

    if ($req->is_success()) {
      print "OK: ", $req->content;
    } else {
      die( "HTTP GET failed: " . $req->as_string );
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

  for (my $bytes = 0; $bytes < $size-1; $bytes += 4) {
    my $rand = int(rand($range ** 4));
    my $string = '';
    for (1..4) {
        $string .= chr($rand % $range + $minimum);
        $rand = int($rand / $range);
    }
    print FILE $string;
    $md5->add($string);
  }
  my $s = "\n";
  print FILE $s;
  $md5->add($s);
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

sub moveRemoteFile($$;$)
{
  my ($from, $to, $no_testdir) = @_;

  setCredentials($d, $user, $passwd);

  my $fromUrl = testDirUrl(). $from;
  my $toUrl = testDirUrl() . $to;

  if( $no_testdir ) {
     $fromUrl = $from;
     $toUrl = $to;
  }

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

sub remoteFileProp($$)
{
  my ($fromDir, $file) = @_;
  my $fromUrl = testDirUrl() . $fromDir;
  my $result;

  if( my $r = $d->propfind( -url => $fromUrl, -depth => 1 ) ) {
    if ( $r->is_collection ) {
      # print "Collection\n";

      foreach my $res ( $r->get_resourcelist->get_resources() ) {
        my $filename = $res->get_property("rel_uri");
        # print "OOOOOOOOOOOOOO $filename " . $res->get_property('id') . "\n";
        if( $file eq $filename || $filename eq $file . "/" ) {
          $result = $res;
        }
      }
    } else {
      # print "OOOOOOOOOOOOOOOOOOO " . $r->get_property("rel_uri");
      $result = $r;
    }
  }
  return $result;
}

sub remoteFileId($$)
{
  my ($fromDir, $file) = @_;
  my $id;
  if( my $res = remoteFileProp($fromDir, $file) ) {
    $id = $res->get_property('id') || "";
  }
  print "## ID of $file: $id\n";
  return $id;
}

# Creates a read write share from the config file user 'share_user' to the
# config file user 'user'
# readWrite: permission flag. 31 for all permissions (read/write/create etc)
# and 1 for read only
sub createShare($$)
{
    my ($dir, $readWrite) = @_;

    my $dd = HTTP::DAV->new();

    setCredentials($dd, $share_user, $share_passwd);
    $dd->open( $owncloud);

    # create a remote dir
    my $url = $owncloud . $dir;

    my $re = $dd->mkcol( $url );
    if( $re == 0 ) {
	print "Failed to create test dir $url\n";
    }

    my $ua  = LWP::UserAgent->new(ssl_opts => { verify_hostname => 0 } );
    $ua->agent( "ownCloudTest_sharing");
    # http://localhost/ocm/ocs/v1.php/apps/files_sharing/api/v1/shares
    my $puturl = $ocs_url . "apps/files_sharing/api/v1/shares";

    my $string = "path=$dir&shareType=0&shareWith=$user&publicUpload=false&permissions=$readWrite";
    print ">>>>>>>>>> $puturl $string\n";

    my $req = POST $puturl, Content => $string;
    $req->authorization_basic($share_user, $share_passwd);
    my $response = $ua->request($req);

    my $id = 0;
    if ($response->is_success()) {
      # print "OK: ", $response->content;
	print $response->decoded_content;
	if( $response->decoded_content =~ /<id>(\d+)<\/id>/m) {
	    $id = $1;
	}
    } else {
      die( "Create sharing failed: " . $response->as_string );
    }
    return $id;
}

sub removeShare($$)
{
    my ($shareId, $dir) = @_;

    my $dd = HTTP::DAV->new();

    setCredentials($dd, $share_user, $share_passwd);
    $dd->open( $owncloud);

    my $ua  = LWP::UserAgent->new(ssl_opts => { verify_hostname => 0 });
    $ua->agent( "ownCloudTest_sharing");

    my $url = $ocs_url . "ocs/v1.php/apps/files_sharing/api/v1/shares/" . $shareId;

    my $req = DELETE $url;
    $req->authorization_basic($share_user, $share_passwd);
    my $response = $ua->request($req);

    if ($response->is_success()) {
	print $response->decoded_content;
	if( $response->decoded_content =~ /<status_code>(\d+)<\/status_code>/m) {
	    my $code = $1;
	    assert( $code == 100 );
	}
    } else {
      die( "Create sharing failed: " . $response->as_string );
    }

    # remove the share dir
    my $req = DELETE $owncloud . $dir;
    $req->authorization_basic($share_user, $share_passwd);
    my $response = $ua->request($req);
}

sub assert($;$)
{
    unless( $_[0] ) {
      print Carp::confess(@_);
      exit(1);
    }
}

#
