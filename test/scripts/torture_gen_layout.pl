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
use Data::Random::WordList;

############################################################################

# Which extensions to randomly assign
my @exts = ('txt', 'pdf', 'html', 'docx', 'xlsx', 'pptx', 'odt', 'ods', 'odp');
# Maximum depth of the target structure
my $depth = 4;
# Maximum amount of subfolders within a folder
my $max_subfolder = 10;
# Maximum amount of files within a folder
my $max_files_per_folder = 100;
# Maximum file size 
my $max_file_size = 1024**2;

############################################################################

sub gen_entries($)
{
  my ($count) = @_;
  my $wl = new Data::Random::WordList( wordlist => '/usr/share/dict/words' );
  my @rand_words = $wl->get_words($count);
  foreach(@rand_words) {
    $_ =~ s/\'//g;
  }
  $wl->close();
  return @rand_words;
}

sub create_subdir($)
{
  my ($depth) = @_;
  $depth--;
  my %dir_tree = ( );

  my @dirs = gen_entries(int(rand($max_subfolders)));
  my @files = gen_entries(int(rand($max_files_per_folder)));

  foreach my $file(@files) {
    $dir_tree{$file} = int(rand($max_file_size));
  }

  if ($depth > 0) {
    foreach my $dir(@dirs) {
      $dir_tree{$dir} = create_subdir($depth);
    }
  }

  return \%dir_tree;
}

sub create_dir_listing(@)
{
  my ($tree, $prefix) = @_;
  foreach my $key(keys %$tree) {
     my $entry = $tree->{$key};
     #print "$entry:".scalar $entry.":".ref $entry."\n";
     if (ref $entry eq "HASH") {
       create_dir_listing($tree->{$key}, "$prefix/$key");
     } else {
       my $ext = @exts[rand @exts];
       print "$prefix/$key.$ext:$entry\n";
     }
  }
}

srand();
my $dir = create_subdir($depth);
create_dir_listing($dir, '.');
