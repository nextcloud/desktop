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
