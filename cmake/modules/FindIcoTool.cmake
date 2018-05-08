# Copyright 2017 Vincent Pinon <vpinon@kde.org>
include(${CMAKE_CURRENT_LIST_DIR}/ECMFindModuleHelpersStub.cmake)
ecm_find_package_version_check(IcoTool)
find_program(IcoTool_EXECUTABLE NAMES icotool)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(IcoTool
    FOUND_VAR
        IcoTool_FOUND
    REQUIRED_VARS
        IcoTool_EXECUTABLE
)
mark_as_advanced(IcoTool_EXECUTABLE)

if (IcoTool_FOUND)
    if (NOT TARGET IcoTool::IcoTool)
        add_executable(IcoTool::IcoTool IMPORTED)
        set_target_properties(IcoTool::IcoTool PROPERTIES
            IMPORTED_LOCATION "${IcoTool_EXECUTABLE}"
        )
    endif()
endif()

include(FeatureSummary)
set_package_properties(IcoTool PROPERTIES
    URL "http://www.nongnu.org/icoutils/"
    DESCRIPTION "Executable that converts a collection of PNG files into a Windows icon file"
)
