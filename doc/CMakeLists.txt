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

