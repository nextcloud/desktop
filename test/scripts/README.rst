Torture for ownCloud Client
===========================

This is a set of scripts comprising of two parts:

* ``torture_gen_layout.pl``: Generation of layout files (random)
* ``torture_create_files.pl``: Generation of a real file tree based on the
  layout files (deterministic)

These scripts allow to produce a data set with the following criteria:

* realistic in naming
* realistic in file size
* realistic in structural size

without checking in the actual data. Instead, a layout file that gets generated
once (reference.lay) is checked in. This makes it possible to produce
standardized benchmarks for mirall. It allows allows to check for files gone
missing in action and other kinds of corruption produced during sync run.

``torture_create_files.pl`` can be fine tuned via variables in the script
header. It sources its file names from ``dict`` wordlist, file extensions and
other parameters can be added as needed. The defaults should be reasonable
in terms of size.

The ``references/`` directory contains default folder layouts.

Usage
-----

In order to create a reference layout and create a tree from it::

  ./torture_gen_layout.pl > reference.lay
  ./torture_create_files.pl reference.lay <targetdir>

TODO
----

* Based on the layout file, write a validator that checks files for existence
  and size without requiring a full reference tree to be created via
  ``./torture_gen_layout.pl``.

* The current file naming is fairly tame (i.e. almost within ASCII range).
  Extending it randomly is dangerous, we first need to filter all
  characters forbidden by various OSes. Or maybe not, because we want to
  see what happens? :-). Anyway, you have been warned.


