#!/usr/bin/env perl
use strict;
use File::Path qw(make_path);
use File::Basename qw(dirname);

if (scalar @ARGV < 2) {
  print "Usage: $0 input.lay <offsetdir>\n";
  exit;
}

my ($file, $offset_dir) = @ARGV;

open FILE, "<", $file or die $!;
while (<FILE>) {
  my ($fillfile, $size) = split(/:/, $_);
  $fillfile = $offset_dir . '/' . $fillfile;
  my $dir = dirname $fillfile;
  if (!-d $dir) { make_path $dir; }
  open FILLFILE, ">", $fillfile;
  print "writing $fillfile with $size bytes\n...";
  print FILLFILE 0x01 x $size;
  close FILLFILE;
}
