#!/usr/bin/env perl
#
#  Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
#  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
#  for more details.
#

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
