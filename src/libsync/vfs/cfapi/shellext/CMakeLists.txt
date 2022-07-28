# Windows SDK command-line tools require native paths
file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" MidleFileFolder)
set(GeneratedFilesPath "${MidleFileFolder}\\Generated")
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

# use midl.exe and cppwinrt.exe to generate files for CustomStateProvider (WinRT class)
add_custom_command(OUTPUT ${MidlOutputPathHeader}
   COMMAND ${midlExe} /winrt /h nul /tlb ${MidlOutputPathTlb} /winmd ${MidlOutputPathWinmd} /metadata_dir "${WindowsSDKReferencesPath}\\Windows.Foundation.FoundationContract\\${WindowsFoundationContractVersion}" /nomidl /reference "${WindowsSDKReferencesPath}\\Windows.Foundation.FoundationContract\\${WindowsFoundationContractVersion}\\Windows.Foundation.FoundationContract.winmd" /reference "${WindowsSDKReferencesPath}\\Windows.Storage.Provider.CloudFilesContract\\${WindowsStorageProviderCloudFilesContractVersion}\\Windows.Storage.Provider.CloudFilesContract.winmd" /I ${MidleFileFolder} customstateprovider.idl
   COMMAND ${cppWinRtExe} -in ${MidlOutputPathWinmd} -comp ${GeneratedFilesPath} -pch pch.h -ref ${WindowsSDKMetadataDirectory} -out ${GeneratedFilesPath} -verbose
   COMMENT "Creating generated files from customstateprovider.idl"
)

add_library(${CFAPI_SHELL_EXTENSIONS_LIB_NAME} MODULE
    dllmain.cpp
    cfapishellintegrationclassfactory.cpp
    thumbnailprovider.cpp
	thumbnailprovideripc.cpp
    vfsexplorercommandhanler.cpp
    customstateprovider.cpp
    CfApiShellIntegration.def
)

add_dependencies(${CFAPI_SHELL_EXTENSIONS_LIB_NAME} CustomStateProviderImpl)

target_link_libraries(${CFAPI_SHELL_EXTENSIONS_LIB_NAME} shlwapi Gdiplus onecore onecoreuap Qt5::Core Qt5::Network)

target_include_directories(${CFAPI_SHELL_EXTENSIONS_LIB_NAME} PRIVATE ${GeneratedFilesPath})

target_include_directories(${CFAPI_SHELL_EXTENSIONS_LIB_NAME} PRIVATE ${CMAKE_SOURCE_DIR})

target_include_directories(${CFAPI_SHELL_EXTENSIONS_LIB_NAME} PRIVATE ${GeneratedFilesPath})

target_compile_features(${CFAPI_SHELL_EXTENSIONS_LIB_NAME} PRIVATE cxx_std_17)

set_target_properties(${CFAPI_SHELL_EXTENSIONS_LIB_NAME}
    PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY
            ${BIN_OUTPUT_DIRECTORY}
        RUNTIME_OUTPUT_DIRECTORY
            ${BIN_OUTPUT_DIRECTORY}
)

install(TARGETS ${CFAPI_SHELL_EXTENSIONS_LIB_NAME} 
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
)
