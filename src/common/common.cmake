# Just list files to build as part of the csync dynamic lib.
# Essentially they could be in the same directory but are separate to
# help keep track of the different code licenses.
set(common_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/utility.cpp
)
