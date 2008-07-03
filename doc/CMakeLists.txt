#
# Build the documentation
#
include("${CMAKE_MODULE_PATH}/UseDoxygen.cmake" OPTIONAL)

#if (DOXYGEN_FOUND)
#  set(html_FILES
#    html/block_title_bottom.png
#    html/block_title_mid.png
#    html/block_title_top.png
#    html/csync.css
#    html/print.css
#    html/top.jpg
#    html/top-left.jpg
#    html/top-right.jpg
#  )
#  foreach(_html_FILE ${html_FILES})
#    configure_file(${_html_FILE} ${CMAKE_CURRENT_BINARY_DIR}/${_html_FILE} COPYONLY)
#  endforeach(_html_FILE ${html_FILES})
#endif (DOXYGEN_FOUND)

#include(UseAsciidoc)
#macro_asciidoc2man(csync.txt csync.1)

file(GLOB _manpages *.txt)
add_custom_target(man
  COMMAND
    ${CMAKE_CURRENT_SOURCE_DIR}/makeman.sh ${_manpages}
  WORKING_DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}
)

install(
  FILES
    csync.1
  DESTINATION
    ${MAN_INSTALL_DIR}/man1
)
