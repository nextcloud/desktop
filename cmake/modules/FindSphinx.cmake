# SPDX-FileCopyrightText: 2014 ownCloud GmbH
# SPDX-License-Identifier: BSD-3-Clause
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING* file.

find_program(SPHINX_EXECUTABLE NAMES sphinx-build
  HINTS
  $ENV{SPHINX_DIR}
  PATH_SUFFIXES bin
  DOC "Sphinx documentation generator"
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(Sphinx DEFAULT_MSG
  SPHINX_EXECUTABLE
)

mark_as_advanced(
  SPHINX_EXECUTABLE
)
