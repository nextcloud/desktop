# SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later
include(GenerateIconsUtils)

unset(CMAKE_CXX_CLANG_TIDY)

# generate custom states icons
set(theme_dir ${CMAKE_SOURCE_DIR}/theme)
set(custom_state_icons_path "${theme_dir}/cfapishellext_custom_states")
set(CUSTOM_STATE_ICON_LOCKED_PATH "${custom_state_icons_path}/0-locked.svg")
set(CUSTOM_STATE_ICON_SHARED_PATH "${custom_state_icons_path}/1-shared.svg")

foreach(size IN ITEMS 24;32;40;48;64;128;256;512;1024)
  get_filename_component(output_icon_name_custom_state_locked ${CUSTOM_STATE_ICON_LOCKED_PATH} NAME_WLE)
  generate_sized_png_from_svg(${CUSTOM_STATE_ICON_LOCKED_PATH} ${size} OUTPUT_ICON_NAME ${output_icon_name_custom_state_locked} OUTPUT_ICON_PATH "${custom_state_icons_path}/")
endforeach()

foreach(size IN ITEMS 24;32;40;48;64;128;256;512;1024)
  get_filename_component(output_icon_name_custom_state_shared ${CUSTOM_STATE_ICON_SHARED_PATH} NAME_WLE)
  generate_sized_png_from_svg(${CUSTOM_STATE_ICON_SHARED_PATH} ${size} OUTPUT_ICON_NAME ${output_icon_name_custom_state_shared} OUTPUT_ICON_PATH "${custom_state_icons_path}/")
endforeach()

# offset is used for referencing icon within the binary's resources (indexing start with 0, while IDI_ICON{i} 'i' starts with 1)
if(NOT DEFINED CUSTOM_STATE_ICON_INDEX_OFFSET)
    set(CUSTOM_STATE_ICON_INDEX_OFFSET 1)
endif()

# indices used for referencing icon within the binary's resources and .rc file's IDI_ICON{i} entries 'i'
if(NOT DEFINED CUSTOM_STATE_ICON_LOCKED_INDEX)
    set(CUSTOM_STATE_ICON_LOCKED_INDEX 1)
endif()
if(NOT DEFINED CUSTOM_STATE_ICON_SHARED_INDEX)
    set(CUSTOM_STATE_ICON_SHARED_INDEX 2)
endif()

file(GLOB_RECURSE CUSTOM_STATE_ICONS_LOCKED "${custom_state_icons_path}/*-locked.png*")
get_filename_component(CUSTOM_STATE_ICON_LOCKED_NAME ${CUSTOM_STATE_ICON_LOCKED_PATH} NAME_WLE)
ecm_add_app_icon(CUSTOM_STATE_ICON_LOCKED_OUT ICONS "${CUSTOM_STATE_ICONS_LOCKED}" OUTFILE_BASENAME "${CUSTOM_STATE_ICON_LOCKED_NAME}" DO_NOT_GENERATE_RC_FILE TRUE)

file(GLOB_RECURSE CUSTOM_STATE_ICONS_SHARED "${custom_state_icons_path}/*-shared.png*")
get_filename_component(CUSTOM_STATE_ICON_SHARED_NAME ${CUSTOM_STATE_ICON_SHARED_PATH} NAME_WLE)
ecm_add_app_icon(CUSTOM_STATE_ICON_SHARED_OUT ICONS "${CUSTOM_STATE_ICONS_SHARED}" OUTFILE_BASENAME "${CUSTOM_STATE_ICON_SHARED_NAME}" DO_NOT_GENERATE_RC_FILE TRUE)

file(REMOVE "${CMAKE_CURRENT_BINARY_DIR}/${CFAPI_SHELL_EXTENSIONS_LIB_NAME}.rc.in")

file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/${CFAPI_SHELL_EXTENSIONS_LIB_NAME}.rc.in" "IDI_ICON${CUSTOM_STATE_ICON_LOCKED_INDEX}        ICON        DISCARDABLE    \"${CMAKE_CURRENT_BINARY_DIR}/${CUSTOM_STATE_ICON_LOCKED_NAME}.ico\"\n")
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/${CFAPI_SHELL_EXTENSIONS_LIB_NAME}.rc.in" "IDI_ICON${CUSTOM_STATE_ICON_SHARED_INDEX}        ICON        DISCARDABLE    \"${CMAKE_CURRENT_BINARY_DIR}/${CUSTOM_STATE_ICON_SHARED_NAME}.ico\"\n")

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${CFAPI_SHELL_EXTENSIONS_LIB_NAME}.rc"
  COMMAND ${CMAKE_COMMAND}
  ARGS -E copy "${CMAKE_CURRENT_BINARY_DIR}/${CFAPI_SHELL_EXTENSIONS_LIB_NAME}.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/${CFAPI_SHELL_EXTENSIONS_LIB_NAME}.rc"
  DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${CUSTOM_STATE_ICON_LOCKED_NAME}.ico" "${CMAKE_CURRENT_BINARY_DIR}/${CUSTOM_STATE_ICON_SHARED_NAME}.ico"
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)
message("CUSTOM_STATE_ICON_LOCKED_OUT: ${CUSTOM_STATE_ICON_LOCKED_OUT}")
message("CUSTOM_STATE_ICON_SHARED_OUT: ${CUSTOM_STATE_ICON_SHARED_OUT}")
#

# Windows SDK command-line tools require native paths
file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" MidleFileFolder)
set(GeneratedFilesPath "${CMAKE_CURRENT_BINARY_DIR}\\Generated")
set(MidlOutputPathHeader "${GeneratedFilesPath}\\CustomStateProvider.g.h")
set(MidlOutputPathTlb "${GeneratedFilesPath}\\CustomStateProvider.tlb")
set(MidlOutputPathWinmd "${GeneratedFilesPath}\\CustomStateProvider.winmd")

add_custom_target(CustomStateProviderImpl
   DEPENDS ${MidlOutputPathHeader}
)

if(NOT DEFINED ENV{WindowsSdkDir})
    message("Getting WindowsSdkDir from Registry")
    get_filename_component(WindowsSdkDir "[HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]" ABSOLUTE)
else()
    set(WindowsSdkDir $ENV{WindowsSdkDir})
    message("Setting WindowsSdkDir from ENV{WindowsSdkDir")
endif()

# we need cmake path to work with subfolders
file(TO_CMAKE_PATH "${WindowsSdkDir}" WindowsSdkDir)

MACRO(SUBDIRLIST result curdir)
  FILE(GLOB children RELATIVE ${curdir} ${curdir}/*)
  SET(dirlist "")
  FOREACH(child ${children})
    IF(IS_DIRECTORY ${curdir}/${child})
      LIST(APPEND dirlist ${child})
    ENDIF()
  ENDFOREACH()
  SET(${result} ${dirlist})
ENDMACRO()

SUBDIRLIST(WindowsSdkList "${WindowsSdkDir}/bin")

# pick only dirs that start with 10.0
list(FILTER WindowsSdkList INCLUDE REGEX "10.0.")
# sort the list of subdirs and choose the latest
list(SORT WindowsSdkList ORDER ASCENDING)
list(GET WindowsSdkList -1 WindowsSdkLatest)
message("WindowsSdkLatest has been set to: ${WindowsSdkLatest}")

if(NOT WindowsSdkLatest)
    message( FATAL_ERROR "Windows SDK not found")
endif()

SUBDIRLIST(listFoundationContracts "${WindowsSdkDir}/References/${WindowsSdkLatest}/Windows.Foundation.FoundationContract")
list(FILTER listFoundationContracts INCLUDE REGEX "[0-9]+\.")
list(SORT listFoundationContracts ORDER ASCENDING)
list(GET listFoundationContracts -1 WindowsFoundationContractVersion)
message("WindowsFoundationContractVersion has been set to: ${WindowsFoundationContractVersion}")

if(NOT WindowsFoundationContractVersion)
    message( FATAL_ERROR "Windows Foundation Contract is not found in ${WindowsSdkLatest} SDK.")
endif()

SUBDIRLIST(listCloudFilesContracts "${WindowsSdkDir}/References/${WindowsSdkLatest}/Windows.Storage.Provider.CloudFilesContract")
list(FILTER listCloudFilesContracts INCLUDE REGEX "[0-9]+\.")
list(SORT listCloudFilesContracts ORDER ASCENDING)
list(GET listCloudFilesContracts -1 WindowsStorageProviderCloudFilesContractVersion)
message("WindowsStorageProviderCloudFilesContractVersion has been set to: ${WindowsStorageProviderCloudFilesContractVersion}")

if(NOT WindowsStorageProviderCloudFilesContractVersion)
    message( FATAL_ERROR "Windows Storage Provider Cloud Files Contract is not found in ${WindowsSdkLatest} SDK.")
endif()

# we no longer need to work with sub folders, so convert the WindowsSdkDir to native path
file(TO_NATIVE_PATH ${WindowsSdkDir} WindowsSdkDir)
message("WindowsSdkDir has been set to: ${WindowsSdkDir}")
message("WindowsSdkList has been set to: ${WindowsSdkList}")
message("WindowsSdkLatest has been set to: ${WindowsSdkLatest}")

set(TargetPlatform "x64")
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(TargetPlatform "x64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(TargetPlatform "x86")
endif()

set(WindowsSDKReferencesPath "${WindowsSdkDir}\\References\\${WindowsSdkLatest}")
set(WindowsSDKBinPathForTools "${WindowsSdkDir}\\bin\\${WindowsSdkLatest}\\${TargetPlatform}")
set(WindowsSDKMetadataDirectory "${WindowsSdkDir}\\UnionMetadata\\${WindowsSdkLatest}")

IF(NOT EXISTS "${WindowsSDKReferencesPath}" OR NOT IS_DIRECTORY "${WindowsSDKReferencesPath}")
    message( FATAL_ERROR "Please install Windows SDK ${WindowsSdkLatest}")
ENDIF()
IF(NOT EXISTS "${WindowsSDKBinPathForTools}" OR NOT IS_DIRECTORY "${WindowsSDKBinPathForTools}")
    message( FATAL_ERROR "Please install Windows SDK ${WindowsSdkLatest}")
ENDIF()
IF(NOT EXISTS "${WindowsSDKMetadataDirectory}" OR NOT IS_DIRECTORY "${WindowsSDKMetadataDirectory}")
    message( FATAL_ERROR "Please install Windows SDK ${WindowsSdkLatest}")
ENDIF()
set(midlExe "${WindowsSDKBinPathForTools}\\midl.exe")
set(cppWinRtExe "${WindowsSDKBinPathForTools}\\cppwinrt.exe")

message("cppWinRtExe: ${cppWinRtExe}")
message("midlExe: ${midlExe}")

configure_file(CfApiShellIntegrationVersion.h.in ${CMAKE_CURRENT_BINARY_DIR}/CfApiShellIntegrationVersion.h)

# use midl.exe and cppwinrt.exe to generate files for CustomStateProvider (WinRT class)
add_custom_command(OUTPUT ${MidlOutputPathHeader}
   COMMAND ${midlExe} /winrt /h nul /tlb ${MidlOutputPathTlb} /winmd ${MidlOutputPathWinmd} /metadata_dir "${WindowsSDKReferencesPath}\\Windows.Foundation.FoundationContract\\${WindowsFoundationContractVersion}" /nomidl /reference "${WindowsSDKReferencesPath}\\Windows.Foundation.FoundationContract\\${WindowsFoundationContractVersion}\\Windows.Foundation.FoundationContract.winmd" /reference "${WindowsSDKReferencesPath}\\Windows.Storage.Provider.CloudFilesContract\\${WindowsStorageProviderCloudFilesContractVersion}\\Windows.Storage.Provider.CloudFilesContract.winmd" /I ${MidleFileFolder} customstateprovider.idl
   COMMAND ${cppWinRtExe} -in ${MidlOutputPathWinmd} -comp ${GeneratedFilesPath} -pch pch.h -ref ${WindowsSDKMetadataDirectory} -out ${GeneratedFilesPath} -verbose
   COMMENT "Creating generated files from customstateprovider.idl"
)

add_library(CfApiShellExtensions MODULE
    dllmain.cpp
    cfapishellintegrationclassfactory.cpp
    customstateprovideripc.cpp
    ipccommon.cpp
    thumbnailprovider.cpp
	thumbnailprovideripc.cpp
    ${CMAKE_SOURCE_DIR}/src/common/shellextensionutils.cpp
    customstateprovider.cpp
    CfApiShellIntegration.def
    CfApiShellIntegration.rc
)

message("CUSTOM_STATE_ICON_LOCKED_OUT: ${CUSTOM_STATE_ICON_LOCKED_OUT}")
message("CUSTOM_STATE_ICON_SHARED_OUT: ${CUSTOM_STATE_ICON_SHARED_OUT}")

if (CUSTOM_STATE_ICON_LOCKED_OUT AND CUSTOM_STATE_ICON_SHARED_OUT)
    message("Adding ${CMAKE_CURRENT_BINARY_DIR}/${CFAPI_SHELL_EXTENSIONS_LIB_NAME}.rc...")
    target_sources(CfApiShellExtensions PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/${CFAPI_SHELL_EXTENSIONS_LIB_NAME}.rc")
else()
    message(WARNING "Could not add ${CMAKE_CURRENT_BINARY_DIR}/${CFAPI_SHELL_EXTENSIONS_LIB_NAME}.rc to CfApiShellExtensions. Custom states for Windows Virtual Files won't work.")
endif()


add_dependencies(CfApiShellExtensions CustomStateProviderImpl)

target_link_libraries(CfApiShellExtensions shlwapi Gdiplus onecoreuap Nextcloud::csync Qt::Core Qt::Network)

target_include_directories(CfApiShellExtensions PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(CfApiShellExtensions PRIVATE ${GeneratedFilesPath})

target_include_directories(CfApiShellExtensions PRIVATE ${CMAKE_SOURCE_DIR})

target_compile_features(CfApiShellExtensions PRIVATE cxx_std_17)

set_target_properties(CfApiShellExtensions
    PROPERTIES
        LIBRARY_OUTPUT_NAME
            ${CFAPI_SHELL_EXTENSIONS_LIB_NAME}
        RUNTIME_OUTPUT_NAME
            ${CFAPI_SHELL_EXTENSIONS_LIB_NAME}
        LIBRARY_OUTPUT_DIRECTORY
            ${BIN_OUTPUT_DIRECTORY}
        RUNTIME_OUTPUT_DIRECTORY
            ${BIN_OUTPUT_DIRECTORY}
)

install(TARGETS CfApiShellExtensions 
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES $<TARGET_PDB_FILE:CfApiShellExtensions> DESTINATION bin OPTIONAL)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configvfscfapishellext.h.in ${CMAKE_CURRENT_BINARY_DIR}/configvfscfapishellext.h)
