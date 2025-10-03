# (c) 2014 Copyright ownCloud GmbH
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING* file.

find_program(PDFLATEX_EXECUTABLE NAMES pdflatex
  HINTS
  $ENV{PDFLATEX_DIR}
  PATH_SUFFIXES bin
  DOC "PDF LaTeX"
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(PdfLatex DEFAULT_MSG
  PDFLATEX_EXECUTABLE
)

mark_as_advanced(
  PDFLATEX_EXECUTABLE
)
