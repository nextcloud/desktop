;Nextcloud installer script.

!define APPLICATION_SHORTNAME "nextcloud"
!define APPLICATION_NAME "Nextcloud"
!define APPLICATION_VENDOR "$%APPLICATION_VENDOR%"
!define APPLICATION_EXECUTABLE "nextcloud.exe"
!define APPLICATION_CMD_EXECUTABLE "nextcloudcmd.exe"
!define APPLICATION_DOMAIN "nextcloud.com"
!define APPLICATION_LICENSE ""
!define WIN_SETUP_BITMAP_PATH "$%PROJECT_PATH%\desktop\admin\win\nsi"
!define CRASHREPORTER_EXECUTABLE "nextcloud_crash_reporter"

; Options, see: defaults.inc.bat
!define INSTALLER_OUTPUT_PATH "$%INSTALLER_OUTPUT_PATH%"
!define USE_CODE_SIGNING "$%USE_CODE_SIGNING%"
!define UPLOAD_BUILD "$%UPLOAD_BUILD%"

;-----------------------------------------------------------------------------
; Some installer script options (comment-out options not required)
;-----------------------------------------------------------------------------
!if "" != ""
  !define OPTION_LICENSE_AGREEMENT
!endif
!define OPTION_UAC_PLUGIN_ENHANCED
!define OPTION_SECTION_SC_SHELL_EXT
!define OPTION_SECTION_SC_START_MENU
!define OPTION_SECTION_SC_DESKTOP
!define OPTION_SECTION_SC_QUICK_LAUNCH
!define OPTION_FINISHPAGE
!define OPTION_FINISHPAGE_LAUNCHER
; !define OPTION_FINISHPAGE_RELEASE_NOTES

;-----------------------------------------------------------------------------
; Some paths.
;-----------------------------------------------------------------------------
!define QT_PATH  "$%QT_PATH%"
!define PROJECT_PATH "$%PROJECT_PATH%"
!define QT_LIBS_PATH "$%QT_LIBS_PATH%"
!define SOURCE_PATH "${PROJECT_PATH}\desktop"
!define SETUP_COLLECTION_PATH "${PROJECT_PATH}\collect\${BUILD_TYPE}"
;!define VCREDISTPATH "$%VCINSTALLDIR%\Redist\MSVC\14.14.26405"   ;now collected by windeployqt
!define CURRENT_PATH "$%CURRENT_PATH%"

!define CSYNC_LIBRARY_DIR ""
!define CSYNC_CONFIG_DIR ""

!define NSI_PATH "${SOURCE_PATH}/admin/win/nsi"

;-----------------------------------------------------------------------------
; !finalize helpers: calls to system() after the output EXE has been generated
;-----------------------------------------------------------------------------
; code signing now happens in sign.bat

;-----------------------------------------------------------------------------
; Installer version
;-----------------------------------------------------------------------------

; Safe to use Win64's exe version since we require both builds for this combined installer.
!getdllversion "${SETUP_COLLECTION_PATH}\Win64\nextcloud.exe" expv_
!define VER_MAJOR "${expv_1}"
!define VER_MINOR "${expv_2}"
!define VER_PATCH "${expv_3}"
!define VER_BUILD "${expv_4}"
!define VERSION "${expv_1}.${expv_2}.${expv_3}.${expv_4}"
Var InstallRunIfSilent
Var NoAutomaticUpdates

;-----------------------------------------------------------------------------
; Installer build timestamp.
;-----------------------------------------------------------------------------
!define /date BUILD_TIME "${MIRALL_VERSION_SUFFIX} Built from Git revision ${GIT_REVISION} on %Y/%m/%d at %I:%M %p"
!define /date BUILD_TIME_FILENAME "%Y%m%d"

;-----------------------------------------------------------------------------
; Initial installer setup and definitions.
;-----------------------------------------------------------------------------

!define INSTALLER_FILENAME "${APPLICATION_SHORTNAME}-${VERSION}-${MIRALL_VERSION_SUFFIX}-${BUILD_TIME_FILENAME}-${BUILD_TYPE}.exe"
Name "Nextcloud"
BrandingText "${APPLICATION_NAME} ${VERSION} - ${BUILD_TIME}"
;IS_INNER_SIGN_UNINSTALLER;OutFile "${PROJECT_PATH}\client-building\daily\${INSTALLER_FILENAME}"
InstallDir "$PROGRAMFILES64\Nextcloud"    ; use the correct path for Win64 (on Win32 this is identical to $PROGRAMFILES)
InstallDirRegKey HKCU "Software\${APPLICATION_VENDOR}\${APPLICATION_NAME}" ""
InstType Standard
InstType Full
InstType Minimal
CRCCheck On
SetCompressor /SOLID lzma
ReserveFile NSIS.InstallOptions.ini
ReserveFile "${NSISDIR}\Plugins\x86-unicode\InstallOptions.dll"

; https://nsis.sourceforge.io/Signing_an_Uninstaller
!ifdef IS_INNER_SIGN_UNINSTALLER
  !echo "Inner uninstaller signing invocation"    ; just to see what's going on
  OutFile "$%TEMP%\tempinstaller.exe"             ; not really important where this is
  RequestExecutionLevel user                      ; don't write uninstaller with admin permissions and prevent invoking UAC
!else
  !echo "Outer uninstaller signing invocation"
 
  ; Call makensis again against current file, defining INNER.  This writes an installer for us which, when
  ; it is invoked, will just write the uninstaller to some location, and then exit.
 
  ; Note: Keep the makensis /Defines in sync with build-installer-exe.bat !
  !makensis '/DIS_INNER_SIGN_UNINSTALLER /DBUILD_TYPE="${BUILD_TYPE}" /DMIRALL_VERSION_SUFFIX="${MIRALL_VERSION_SUFFIX}" /DMIRALL_VERSION_BUILD="${MIRALL_VERSION_BUILD}" /DGIT_REVISION="${GIT_REVISION}" "${__FILE__}"' = 0
 
  ; So now run that installer we just created as %TEMP%\tempinstaller.exe.  Since it
  ; calls quit the return value isn't zero.
 
  !system "$%TEMP%\tempinstaller.exe" = 2
 
  ; That will have written an uninstaller binary for us.  Now we sign it with your
  ; favorite code signing tool.
 
  ;!system "SIGNCODE <signing options> $%TEMP%\uninstall.exe" = 0
  !if ${USE_CODE_SIGNING} != 0
    !system '"${CURRENT_PATH}\sign.bat" "$%TEMP%\uninstall.exe"' = 0
  !endif

  ; Good.  Now we can carry on writing the real installer.
 
  OutFile "${INSTALLER_OUTPUT_PATH}\${INSTALLER_FILENAME}"
  RequestExecutionLevel user ;Now using the UAC plugin.
!endif

;-----------------------------------------------------------------------------
; Include some required header files.
;-----------------------------------------------------------------------------
!include LogicLib.nsh ;Used by APPDATA uninstaller.
!include MUI2.nsh ;Used by APPDATA uninstaller.
!include InstallOptions.nsh ;Required by MUI2 to support old MUI_INSTALLOPTIONS.
!include Memento.nsh ;Remember user selections.
!include WinVer.nsh ;Windows version detection.
!include WordFunc.nsh  ;Used by VersionCompare macro function.
!include FileFunc.nsh  ;Used to read out parameters
!include UAC.nsh ;Used by the UAC elevation to install as user or admin.
!include nsProcess.nsh ;Used to kill the running process
!include Library.nsh ;Used by the COM registration for shell extensions
!include x64.nsh ;Used to determine the right arch for the shell extensions

;-----------------------------------------------------------------------------
; Memento selections stored in registry.
;-----------------------------------------------------------------------------
!define MEMENTO_REGISTRY_ROOT HKLM
!define MEMENTO_REGISTRY_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPLICATION_NAME}"

;-----------------------------------------------------------------------------
; Modern User Interface (MUI) definitions and setup.
;-----------------------------------------------------------------------------
!define MUI_ABORTWARNING
!define MUI_ICON ${NSI_PATH}\installer.ico
!define MUI_UNICON ${NSI_PATH}\installer.ico
!define MUI_WELCOMEFINISHPAGE_BITMAP ${WIN_SETUP_BITMAP_PATH}\welcome.bmp
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP  ${WIN_SETUP_BITMAP_PATH}\page_header.bmp
!define MUI_COMPONENTSPAGE_SMALLDESC
; We removed this, h1 issue 191687
;!define MUI_FINISHPAGE_LINK "${APPLICATION_DOMAIN}"
;!define MUI_FINISHPAGE_LINK_LOCATION "https://${APPLICATION_DOMAIN}"
!define MUI_FINISHPAGE_NOREBOOTSUPPORT
!ifdef OPTION_FINISHPAGE_RELEASE_NOTES
   !define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
   !define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\NOTES.txt"
   !define MUI_FINISHPAGE_SHOWREADME_TEXT $MUI_FINISHPAGE_SHOWREADME_TEXT_STRING
!endif
!ifdef OPTION_FINISHPAGE_LAUNCHER
   !define MUI_FINISHPAGE_NOAUTOCLOSE
   !define MUI_FINISHPAGE_RUN
   !define MUI_FINISHPAGE_RUN_FUNCTION "LaunchApplication"
!endif

;-----------------------------------------------------------------------------
; Page macros.
;-----------------------------------------------------------------------------
!insertmacro MUI_PAGE_WELCOME
!ifdef OPTION_LICENSE_AGREEMENT
   !insertmacro MUI_PAGE_LICENSE "${APPLICATION_LICENSE}"
!endif
Page custom PageReinstall PageLeaveReinstall
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!ifdef OPTION_FINISHPAGE
   !insertmacro MUI_PAGE_FINISH
!endif
!ifdef IS_INNER_SIGN_UNINSTALLER
   !insertmacro MUI_UNPAGE_CONFIRM
   !insertmacro MUI_UNPAGE_INSTFILES
!endif

;-----------------------------------------------------------------------------
; Other MUI macros.
;-----------------------------------------------------------------------------
!insertmacro MUI_LANGUAGE "English"

!include ${source_path}\admin\win\nsi\l10n\languages.nsh
!include ${source_path}\admin\win\nsi\l10n\declarations.nsh

; Set version strings with english locale
VIProductVersion "${VERSION}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductName" "${APPLICATION_NAME}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "CompanyName" "${APPLICATION_VENDOR}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileVersion" "${VERSION}"

!macro SETLANG un
   Function ${un}SetLang
      # load the selected language file
      !include "${source_path}/admin/win/nsi/l10n\English.nsh"
      StrCmp $LANGUAGE ${LANG_GERMAN} German 0
      StrCmp $LANGUAGE ${LANG_DUTCH} Dutch 0
      StrCmp $LANGUAGE ${LANG_FINNISH} Finnish 0
      StrCmp $LANGUAGE ${LANG_JAPANESE} Japanese 0
      StrCmp $LANGUAGE ${LANG_SLOVENIAN} Slovenian 0
      StrCmp $LANGUAGE ${LANG_SPANISH} Spanish 0
      StrCmp $LANGUAGE ${LANG_ITALIAN} Italian 0
      StrCmp $LANGUAGE ${LANG_ESTONIAN} Estonian 0
      StrCmp $LANGUAGE ${LANG_GREEK} Greek 0
      StrCmp $LANGUAGE ${LANG_BASQUE} Basque 0
      StrCmp $LANGUAGE ${LANG_GALICIAN} Galician 0
      StrCmp $LANGUAGE ${LANG_POLISH} Polish 0
      StrCmp $LANGUAGE ${LANG_TURKISH} Turkish 0
      StrCmp $LANGUAGE ${LANG_NORWEGIAN} Norwegian 0
      StrCmp $LANGUAGE ${LANG_PORTUGUESEBR} Brazilian EndLanguageCmp
      German:
      !include "${source_path}/admin/win/nsi/l10n\German.nsh"
      Goto EndLanguageCmp
      Dutch:
      !include "${source_path}/admin/win/nsi/l10n\Dutch.nsh"
      Goto EndLanguageCmp
      Finnish:
      !include "${source_path}/admin/win/nsi/l10n\Finnish.nsh"
      Goto EndLanguageCmp
      Japanese:
      !include "${source_path}/admin/win/nsi/l10n\Japanese.nsh"
      Goto EndLanguageCmp
      Slovenian:
      !include "${source_path}/admin/win/nsi/l10n\Slovenian.nsh"
      Goto EndLanguageCmp
      Spanish:
      !include "${source_path}/admin/win/nsi/l10n\Spanish.nsh"
      Goto EndLanguageCmp
      Italian:
      !include "${source_path}/admin/win/nsi/l10n\Italian.nsh"
      Goto EndLanguageCmp
      Estonian:
      !include "${source_path}/admin/win/nsi/l10n\Estonian.nsh"
      Goto EndLanguageCmp
      Greek:
      !include "${source_path}/admin/win/nsi/l10n\Greek.nsh"
      Goto EndLanguageCmp
      Basque:
      !include "${source_path}/admin/win/nsi/l10n\Basque.nsh"
      Goto EndLanguageCmp
      Galician:
      !include "${source_path}/admin/win/nsi/l10n\Galician.nsh"
      Goto EndLanguageCmp
      Polish:
      !include "${source_path}/admin/win/nsi/l10n\Polish.nsh"
      Goto EndLanguageCmp
      Turkish:
      !include "${source_path}/admin/win/nsi/l10n\Turkish.nsh"
      Goto EndLanguageCmp
      Brazilian:
      !include "${source_path}/admin/win/nsi/l10n\PortugueseBR.nsh"
      Goto EndLanguageCmp
      Norwegian:
      !include "${source_path}/admin/win/nsi/l10n\Norwegian.nsh"
      EndLanguageCmp:

   FunctionEnd
!macroend

!insertmacro SETLANG ""
!insertmacro SETLANG "un."

; Usage: ${If} ${HasSection} SectionName
!macro _HasSection _a _b _t _f
   ReadRegDWORD $_LOGICLIB_TEMP "${MEMENTO_REGISTRY_ROOT}" "${MEMENTO_REGISTRY_KEY}" "MementoSection_${_b}"
   IntCmpU $_LOGICLIB_TEMP 0 ${_f} ${_t}
!macroend
!define HasSection `"" HasSection`

##############################################################################
#                                                                            #
#   FINISH PAGE LAUNCHER FUNCTIONS                                           #
#                                                                            #
##############################################################################

Function LaunchApplication
   !insertmacro UAC_AsUser_ExecShell "" "$INSTDIR\${APPLICATION_EXECUTABLE}" "" "" ""
FunctionEnd

##############################################################################
#                                                                            #
#   PROCESS HANDLING FUNCTIONS AND MACROS                                    #
#                                                                            #
##############################################################################

!macro CheckForProcess processName gotoWhenFound gotoWhenNotFound
   ${nsProcess::FindProcess} ${processName} $R0
   StrCmp $R0 0 ${gotoWhenFound} ${gotoWhenNotFound}
!macroend

!macro ConfirmEndProcess processName
   MessageBox MB_YESNO|MB_ICONEXCLAMATION \
     $ConfirmEndProcess_MESSAGEBOX_TEXT \
     /SD IDYES IDYES process_${processName}_kill IDNO process_${processName}_ended
   process_${processName}_kill:
      DetailPrint $ConfirmEndProcess_KILLING_PROCESSES_TEXT
      ${nsProcess::KillProcess} ${processName} $R0
      Sleep 1500
      StrCmp $R0 "1" process_${processName}_ended
      DetailPrint $ConfirmEndProcess_KILL_NOT_FOUND_TEXT
   process_${processName}_ended:
!macroend

!macro CheckAndConfirmEndProcess processName
   !insertmacro CheckForProcess ${processName} 0 no_process_${processName}_to_end
   !insertmacro ConfirmEndProcess ${processName}
   no_process_${processName}_to_end:
!macroend

Function EnsureOwncloudShutdown
   !insertmacro CheckAndConfirmEndProcess "${APPLICATION_EXECUTABLE}"
FunctionEnd

Function InstallRedistributables
   ${If} ${RunningX64}
      ExecWait '"$OUTDIR\vcredist_x64.exe" /install /quiet /norestart'
   ${Else}
      ExecWait '"$OUTDIR\vcredist_x86.exe" /install /quiet /norestart'
   ${EndIf}
   Delete "$OUTDIR\vcredist_x86.exe"
   Delete "$OUTDIR\vcredist_x64.exe"
FunctionEnd

##############################################################################
#                                                                            #
#   RE-INSTALLER FUNCTIONS                                                   #
#                                                                            #
##############################################################################

Function PageReinstall
   ReadRegStr $R0 HKLM "Software\${APPLICATION_VENDOR}\${APPLICATION_NAME}" ""
   StrCmp $R0 "" 0 +2
   Abort

   ;Detect version
   ReadRegDWORD $R0 HKLM "Software\${APPLICATION_VENDOR}\${APPLICATION_NAME}" "VersionMajor"
   IntCmp $R0 ${VER_MAJOR} minor_check new_version older_version
   minor_check:
      ReadRegDWORD $R0 HKLM "Software\${APPLICATION_VENDOR}\${APPLICATION_NAME}" "VersionMinor"
      IntCmp $R0 ${VER_MINOR} rev_check new_version older_version
   rev_check:
      ReadRegDWORD $R0 HKLM "Software\${APPLICATION_VENDOR}\${APPLICATION_NAME}" "VersionRevision"
      IntCmp $R0 ${VER_PATCH} build_check new_version older_version
   build_check:
      ReadRegDWORD $R0 HKLM "Software\${APPLICATION_VENDOR}\${APPLICATION_NAME}" "VersionBuild"
      IntCmp $R0 ${VER_BUILD} same_version new_version older_version

   new_version:
      !insertmacro INSTALLOPTIONS_WRITE "NSIS.InstallOptions.ini" "Field 1" "Text" $PageReinstall_NEW_Field_1
      !insertmacro INSTALLOPTIONS_WRITE "NSIS.InstallOptions.ini" "Field 2" "Text" $PageReinstall_NEW_Field_2
      !insertmacro INSTALLOPTIONS_WRITE "NSIS.InstallOptions.ini" "Field 3" "Text" $PageReinstall_NEW_Field_3
      !insertmacro MUI_HEADER_TEXT $PageReinstall_NEW_MUI_HEADER_TEXT_TITLE $PageReinstall_NEW_MUI_HEADER_TEXT_SUBTITLE
      StrCpy $R0 "1"
      Goto reinst_start

   older_version:
      !insertmacro INSTALLOPTIONS_WRITE "NSIS.InstallOptions.ini" "Field 1" "Text" $PageReinstall_OLD_Field_1
      !insertmacro INSTALLOPTIONS_WRITE "NSIS.InstallOptions.ini" "Field 2" "Text" $PageReinstall_NEW_Field_2
      !insertmacro INSTALLOPTIONS_WRITE "NSIS.InstallOptions.ini" "Field 3" "Text" $PageReinstall_NEW_Field_3
      !insertmacro MUI_HEADER_TEXT $PageReinstall_NEW_MUI_HEADER_TEXT_TITLE $PageReinstall_NEW_MUI_HEADER_TEXT_SUBTITLE
      StrCpy $R0 "1"
      Goto reinst_start

   same_version:
      !insertmacro INSTALLOPTIONS_WRITE "NSIS.InstallOptions.ini" "Field 1" "Text" $PageReinstall_SAME_Field_1
      !insertmacro INSTALLOPTIONS_WRITE "NSIS.InstallOptions.ini" "Field 2" "Text" $PageReinstall_SAME_Field_2
      !insertmacro INSTALLOPTIONS_WRITE "NSIS.InstallOptions.ini" "Field 3" "Text" $PageReinstall_SAME_Field_3
      !insertmacro MUI_HEADER_TEXT $PageReinstall_NEW_MUI_HEADER_TEXT_TITLE $PageReinstall_SAME_MUI_HEADER_TEXT_SUBTITLE
      StrCpy $R0 "2"

   reinst_start:
      !insertmacro INSTALLOPTIONS_DISPLAY "NSIS.InstallOptions.ini"
FunctionEnd

Function PageLeaveReinstall
   !insertmacro INSTALLOPTIONS_READ $R1 "NSIS.InstallOptions.ini" "Field 2" "State"
   StrCmp $R0 "1" 0 +2
   StrCmp $R1 "1" reinst_uninstall reinst_done
   StrCmp $R0 "2" 0 +3
   StrCmp $R1 "1" reinst_done reinst_uninstall
   reinst_uninstall:
      ReadRegStr $R1 ${MEMENTO_REGISTRY_ROOT} "${MEMENTO_REGISTRY_KEY}" "UninstallString"
      HideWindow
      ClearErrors
      ExecWait '$R1 _?=$INSTDIR'
      IfErrors no_remove_uninstaller
      IfFileExists "$INSTDIR\${APPLICATION_EXECUTABLE}" no_remove_uninstaller
      Delete $R1
      RMDir $INSTDIR
   no_remove_uninstaller:
      StrCmp $R0 "2" 0 +3
      Quit
      BringToFront
   reinst_done:
FunctionEnd

##############################################################################
#                                                                            #
#   INSTALLER SECTIONS                                                       #
#                                                                            #
##############################################################################
!ifndef IS_INNER_SIGN_UNINSTALLER
Section "${APPLICATION_NAME}" SEC_APPLICATION
   SectionIn 1 2 3 RO
   SetDetailsPrint listonly

   SetDetailsPrint textonly
   DetailPrint $SEC_APPLICATION_DETAILS
   SetDetailsPrint listonly
   SetOutPath "$INSTDIR"

; all the files, kindly collected by our friendly build-installer-collect script :)
   ${If} ${RunningX64}
      File /r "${SETUP_COLLECTION_PATH}\Win64\*.*"
   ${Else}
      File /r "${SETUP_COLLECTION_PATH}\Win32\*.*"
   ${Endif}

; to be executed after the installer is created
   !if ${USE_CODE_SIGNING} != 0
      !finalize '"${CURRENT_PATH}\sign.bat" "%1"'
   !endif
   !if ${UPLOAD_BUILD} != 0
      !finalize '"${CURRENT_PATH}\upload.bat" %1'  ; note: %1 quotes intentionally removed!
   !endif
SectionEnd
!endif ;IS_INNER_SIGN_UNINSTALLER

!ifdef OPTION_SECTION_SC_SHELL_EXT
   ${MementoSection} $OPTION_SECTION_SC_SHELL_EXT_SECTION SEC_SHELL_EXT
      SectionIn 1 2
      SetDetailsPrint textonly
      DetailPrint $OPTION_SECTION_SC_SHELL_EXT_DetailPrint
      ;File "${VCREDISTPATH}\vcredist_x86.exe"  ;now collected by windeployqt
      ;File "${VCREDISTPATH}\vcredist_x64.exe"  ;now collected by windeployqt
      Call InstallRedistributables
      CreateDirectory "$INSTDIR\shellext"
      !define LIBRARY_COM
      !define LIBRARY_SHELL_EXTENSION
      !define LIBRARY_IGNORE_VERSION
      ${If} ${RunningX64}
         !define LIBRARY_X64
         !insertmacro InstallLib DLL NOTSHARED REBOOT_PROTECTED "${SETUP_COLLECTION_PATH}\Win64\shellext\OCUtil.dll" "$INSTDIR\shellext\OCUtil.dll" "$INSTDIR\shellext"
         !insertmacro InstallLib REGDLL NOTSHARED REBOOT_PROTECTED "${SETUP_COLLECTION_PATH}\Win64\shellext\OCOverlays.dll" "$INSTDIR\shellext\OCOverlays.dll" "$INSTDIR\shellext"
         !insertmacro InstallLib REGDLL NOTSHARED REBOOT_PROTECTED "${SETUP_COLLECTION_PATH}\Win64\shellext\OCContextMenu.dll" "$INSTDIR\shellext\OCContextMenu.dll" "$INSTDIR\shellext"
         !undef LIBRARY_X64
     ${Else}
         !insertmacro InstallLib DLL NOTSHARED REBOOT_PROTECTED "${SETUP_COLLECTION_PATH}\Win32\shellext\OCUtil.dll" "$INSTDIR\shellext\OCUtil.dll" "$INSTDIR\shellext"
         !insertmacro InstallLib REGDLL NOTSHARED REBOOT_PROTECTED "${SETUP_COLLECTION_PATH}\Win32\shellext\OCOverlays.dll" "$INSTDIR\shellext\OCOverlays.dll" "$INSTDIR\shellext"
         !insertmacro InstallLib REGDLL NOTSHARED REBOOT_PROTECTED "${SETUP_COLLECTION_PATH}\Win32\shellext\OCContextMenu.dll" "$INSTDIR\shellext\OCContextMenu.dll" "$INSTDIR\shellext"
      ${Endif}
      !undef LIBRARY_COM
      !undef LIBRARY_SHELL_EXTENSION
      !undef LIBRARY_IGNORE_VERSION
   ${MementoSectionEnd}
!endif

SectionGroup $SectionGroup_Shortcuts

!ifdef OPTION_SECTION_SC_START_MENU
   ${MementoSection} $OPTION_SECTION_SC_START_MENU_SECTION SEC_START_MENU
      SectionIn 1 2 3
      SetDetailsPrint textonly
      DetailPrint $OPTION_SECTION_SC_START_MENU_DetailPrint
      SetDetailsPrint listonly
      SetShellVarContext all
      CreateShortCut "$SMPROGRAMS\${APPLICATION_NAME}.lnk" "$INSTDIR\${APPLICATION_EXECUTABLE}" "" "$INSTDIR\nextcloud.ico" 0
      SetShellVarContext current
   ${MementoSectionEnd}
!endif

!ifdef OPTION_SECTION_SC_DESKTOP
   ${MementoSection} $OPTION_SECTION_SC_DESKTOP_SECTION SEC_DESKTOP
      SectionIn 1 2
      SetDetailsPrint textonly
      DetailPrint $OPTION_SECTION_SC_DESKTOP_DetailPrint
      SetDetailsPrint listonly
      SetShellVarContext all
      CreateShortCut "$DESKTOP\${APPLICATION_NAME}.lnk" "$INSTDIR\${APPLICATION_EXECUTABLE}" "" "$INSTDIR\nextcloud.ico" 0
      SetShellVarContext current
   ${MementoSectionEnd}
!endif

!ifdef OPTION_SECTION_SC_QUICK_LAUNCH
   ${MementoSection} $OPTION_SECTION_SC_QUICK_LAUNCH_SECTION SEC_QUICK_LAUNCH
      SectionIn 1 2
      SetDetailsPrint textonly
      DetailPrint $OPTION_SECTION_SC_QUICK_LAUNCH_DetailPrint
      SetShellVarContext all
      SetDetailsPrint listonly
      CreateShortCut "$QUICKLAUNCH\${APPLICATION_NAME}.lnk" "$INSTDIR\${APPLICATION_EXECUTABLE}" "" "$INSTDIR\nextcloud.ico" 0
      SetShellVarContext current
   ${MementoSectionEnd}
!endif

SectionGroupEnd

${MementoSectionDone}

; Installer section descriptions
;--------------------------------
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
!insertmacro MUI_DESCRIPTION_TEXT ${SEC_APPLICATION} $OPTION_SECTION_SC_APPLICATION_Desc
!insertmacro MUI_DESCRIPTION_TEXT ${SEC_START_MENU} $OPTION_SECTION_SC_START_MENU_Desc
!insertmacro MUI_DESCRIPTION_TEXT ${SEC_DESKTOP} $OPTION_SECTION_SC_DESKTOP_Desc
!insertmacro MUI_DESCRIPTION_TEXT ${SEC_QUICK_LAUNCH} $OPTION_SECTION_SC_QUICK_LAUNCH_Desc
!insertmacro MUI_FUNCTION_DESCRIPTION_END


Section -post

   ;Uninstaller file.
   SetDetailsPrint textonly
   DetailPrint $UNINSTALLER_FILE_Detail
   SetDetailsPrint listonly
   !ifndef IS_INNER_SIGN_UNINSTALLER
      ; this packages the signed uninstaller
      SetOutPath $INSTDIR
      File $%TEMP%\uninstall.exe
   !endif

   ;Registry keys required for installer version handling and uninstaller.
   SetDetailsPrint textonly
   DetailPrint $UNINSTALLER_REGISTRY_Detail
   SetDetailsPrint listonly

   ;Version numbers used to detect existing installation version for comparison.
   WriteRegStr HKLM "Software\${APPLICATION_VENDOR}\${APPLICATION_NAME}" "" $INSTDIR
   WriteRegDWORD HKLM "Software\${APPLICATION_VENDOR}\${APPLICATION_NAME}" "VersionMajor" "${VER_MAJOR}"
   WriteRegDWORD HKLM "Software\${APPLICATION_VENDOR}\${APPLICATION_NAME}" "VersionMinor" "${VER_MINOR}"
   WriteRegDWORD HKLM "Software\${APPLICATION_VENDOR}\${APPLICATION_NAME}" "VersionRevision" "${VER_PATCH}"
   WriteRegDWORD HKLM "Software\${APPLICATION_VENDOR}\${APPLICATION_NAME}" "VersionBuild" "${VER_BUILD}"

   ;Add or Remove Programs entry.
   WriteRegExpandStr ${MEMENTO_REGISTRY_ROOT} "${MEMENTO_REGISTRY_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
   WriteRegExpandStr ${MEMENTO_REGISTRY_ROOT} "${MEMENTO_REGISTRY_KEY}" "InstallLocation" "$INSTDIR"
   WriteRegStr ${MEMENTO_REGISTRY_ROOT} "${MEMENTO_REGISTRY_KEY}" "DisplayName" "${APPLICATION_NAME}"
   WriteRegStr ${MEMENTO_REGISTRY_ROOT} "${MEMENTO_REGISTRY_KEY}" "Publisher" "${APPLICATION_VENDOR}"
   WriteRegStr ${MEMENTO_REGISTRY_ROOT} "${MEMENTO_REGISTRY_KEY}" "DisplayIcon" "$INSTDIR\Uninstall.exe,0"
   WriteRegStr ${MEMENTO_REGISTRY_ROOT} "${MEMENTO_REGISTRY_KEY}" "DisplayVersion" "${VERSION}"
   WriteRegDWORD ${MEMENTO_REGISTRY_ROOT} "${MEMENTO_REGISTRY_KEY}" "VersionMajor" "${VER_MAJOR}"
   WriteRegDWORD ${MEMENTO_REGISTRY_ROOT} "${MEMENTO_REGISTRY_KEY}" "VersionMinor" "${VER_MINOR}.${VER_PATCH}.${VER_BUILD}"
   WriteRegStr ${MEMENTO_REGISTRY_ROOT} "${MEMENTO_REGISTRY_KEY}" "URLInfoAbout" "https://${APPLICATION_DOMAIN}/"
   WriteRegStr ${MEMENTO_REGISTRY_ROOT} "${MEMENTO_REGISTRY_KEY}" "HelpLink" "https://${APPLICATION_DOMAIN}/"
   WriteRegDWORD ${MEMENTO_REGISTRY_ROOT} "${MEMENTO_REGISTRY_KEY}" "NoModify" "1"
   WriteRegDWORD ${MEMENTO_REGISTRY_ROOT} "${MEMENTO_REGISTRY_KEY}" "NoRepair" "1"


   ;Respect user choices for the client's first launch.
   Var /GLOBAL configFileName
   StrCpy $configFileName "$APPDATA\${APPLICATION_NAME}\${APPLICATION_SHORTNAME}.cfg"

   !ifdef OPTION_SECTION_SC_SHELL_EXT
      Var /GLOBAL showInExplorerNavigationPane

      ${If} ${SectionIsSelected} ${SEC_SHELL_EXT}
         StrCpy $showInExplorerNavigationPane "true"
      ${Else}
         StrCpy $showInExplorerNavigationPane "false"
      ${EndIf}

      CreateDirectory "$APPDATA\${APPLICATION_NAME}"

      SetShellVarContext all
      DeleteINIStr "$configFileName" "General" "showInExplorerNavigationPane"
      WriteIniStr "$configFileName" "General" "showInExplorerNavigationPane" "$showInExplorerNavigationPane"
      SetShellVarContext current
   !endif


   SetDetailsPrint textonly
   DetailPrint $UNINSTALLER_FINISHED_Detail
SectionEnd

##############################################################################
#                                                                            #
#   UNINSTALLER SECTION                                                      #
#                                                                            #
##############################################################################

Function un.EnsureOwncloudShutdown
   !insertmacro CheckAndConfirmEndProcess "${APPLICATION_EXECUTABLE}"
FunctionEnd

!ifdef IS_INNER_SIGN_UNINSTALLER
  ; the normal uninstaller section (it isn't needed in the "outer" installer
  ; and will just cause warnings because there is no WriteUninstaller command)
Section Uninstall
   IfFileExists "$INSTDIR\${APPLICATION_EXECUTABLE}" owncloud_installed
      MessageBox MB_YESNO $UNINSTALL_MESSAGEBOX /SD IDYES IDYES owncloud_installed
      Abort $UNINSTALL_ABORT
   owncloud_installed:

   ; Delete Navigation Pane entries added for Windows 10.
   ; On 64bit Windows, the client will be writing to the 64bit registry.
   ${If} ${RunningX64}
      SetRegView 64
   ${EndIf}
   StrCpy $0 0
   loop:
      ; Look at every registered explorer namespace for HKCU and check if it was added by our application
      ; (we write to a custom "ApplicationName" value there).
      EnumRegKey $1 HKCU "Software\Microsoft\Windows\CurrentVersion\Explorer\Desktop\NameSpace" $0
      StrCmp $1 "" done

      ReadRegStr $R0 HKCU "Software\Microsoft\Windows\CurrentVersion\Explorer\Desktop\NameSpace\$1" "ApplicationName"
      StrCmp $R0 "${APPLICATION_NAME}" deleteClsid
      ; Increment the index when not deleting the enumerated key.
      IntOp $0 $0 + 1
      goto loop

      deleteClsid:
         DetailPrint "Removing Navigation Pane CLSID $1"
         ; Should match FolderMan::updateCloudStorageRegistry
         DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Explorer\Desktop\NameSpace\$1"
         DeleteRegKey HKCU "Software\Classes\CLSID\$1"
         DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Explorer\HideDesktopIcons\NewStartPanel" $1
         goto loop
   done:
   ; Go back to the 32bit registry.
   SetRegView lastused

   ;Delete registry keys.
   DeleteRegValue HKLM "Software\${APPLICATION_VENDOR}\${APPLICATION_NAME}" "VersionBuild"
   DeleteRegValue HKLM "Software\${APPLICATION_VENDOR}\${APPLICATION_NAME}" "VersionMajor"
   DeleteRegValue HKLM "Software\${APPLICATION_VENDOR}\${APPLICATION_NAME}" "VersionMinor"
   DeleteRegValue HKLM "Software\${APPLICATION_VENDOR}\${APPLICATION_NAME}" "VersionRevision"
   DeleteRegValue HKLM "Software\${APPLICATION_VENDOR}\${APPLICATION_NAME}" ""
   DeleteRegKey HKLM "Software\${APPLICATION_VENDOR}\${APPLICATION_NAME}"

   DeleteRegKey HKCR "${APPLICATION_NAME}"

   ;Shell extension
   !ifdef OPTION_SECTION_SC_SHELL_EXT
      !define LIBRARY_COM
      !define LIBRARY_SHELL_EXTENSION
      !define LIBRARY_IGNORE_VERSION
      ${If} ${HasSection} SEC_SHELL_EXT
        DetailPrint "Uninstalling x64 overlay DLLs"
        !define LIBRARY_X64
        !insertmacro UnInstallLib REGDLL NOTSHARED REBOOT_PROTECTED "$INSTDIR\shellext\OCContextMenu.dll"
        !insertmacro UnInstallLib REGDLL NOTSHARED REBOOT_PROTECTED "$INSTDIR\shellext\OCOverlays.dll"
        !insertmacro UnInstallLib DLL NOTSHARED REBOOT_PROTECTED "$INSTDIR\shellext\OCUtil.dll"
        !undef LIBRARY_X64
        DetailPrint "Uninstalling x86 overlay DLLs"
        !insertmacro UnInstallLib REGDLL NOTSHARED REBOOT_PROTECTED "$INSTDIR\shellext\OCContextMenu.dll"
        !insertmacro UnInstallLib REGDLL NOTSHARED REBOOT_PROTECTED "$INSTDIR\shellext\OCOverlays.dll"
        !insertmacro UnInstallLib DLL NOTSHARED REBOOT_PROTECTED "$INSTDIR\shellext\OCUtil.dll"
      ${EndIf}
      !undef LIBRARY_COM
      !undef LIBRARY_SHELL_EXTENSION
      !undef LIBRARY_IGNORE_VERSION
  !endif

   ;Start menu shortcut
   !ifdef OPTION_SECTION_SC_START_MENU
      SetShellVarContext all
      ${If} ${HasSection} SEC_START_MENU
         Delete "$SMPROGRAMS\${APPLICATION_NAME}.lnk"
      ${EndIf}
      SetShellVarContext current
   !endif

   ;Desktop shortcut.
   !ifdef OPTION_SECTION_SC_DESKTOP
      ${If} ${HasSection} SEC_DESKTOP
         SetShellVarContext all
         ${If} ${FileExists} "$DESKTOP\${APPLICATION_NAME}.lnk"
            Delete "$DESKTOP\${APPLICATION_NAME}.lnk"
         ${EndIf}
         SetShellVarContext current
      ${EndIf}
   !endif

   ;Quick Launch shortcut.
   !ifdef OPTION_SECTION_SC_QUICK_LAUNCH
      ${If} ${HasSection} SEC_QUICK_LAUNCH
         SetShellVarContext all
         ${If} ${FileExists} "$QUICKLAUNCH\${APPLICATION_NAME}.lnk"
            Delete "$QUICKLAUNCH\${APPLICATION_NAME}.lnk"
         ${EndIf}
         SetShellVarContext current
      ${EndIf}
   !endif

   ;Remove all the Program Files.
   RMDir /r $INSTDIR

   DeleteRegKey ${MEMENTO_REGISTRY_ROOT} "${MEMENTO_REGISTRY_KEY}"

   SetDetailsPrint textonly
   DetailPrint $UNINSTALLER_FINISHED_Detail
SectionEnd
!endif ;IS_INNER_SIGN_UNINSTALLER

##############################################################################
#                                                                            #
#   NSIS Installer Event Handler Functions                                   #
#                                                                            #
##############################################################################

Function .onInit
   !ifdef IS_INNER_SIGN_UNINSTALLER
      ; If INNER is defined, then we aren't supposed to do anything except write out
      ; the uninstaller.  This is better than processing a command line option as it means
      ; this entire code path is not present in the final (real) installer.
      SetSilent silent
      WriteUninstaller "$%TEMP%\uninstall.exe"
      Quit  ; just bail out quickly when running the "inner" installer
   !endif

   SetOutPath $INSTDIR

   ${GetParameters} $R0
   ${GetOptions} $R0 "/launch" $R0
   ${IfNot} ${Errors}
      StrCpy $InstallRunIfSilent "yes"
   ${EndIf}

   ${GetParameters} $R0
   ${GetOptions} $R0 "/noautoupdate" $R0
   ${IfNot} ${Errors}
      StrCpy $NoAutomaticUpdates "yes"
   ${EndIf}


   ;Define App Section name based on arch
   ${If} ${RunningX64}
      SectionSetText $R0 "${APPLICATION_NAME} (64-bit)"
   ${Else}
      SectionSetText $R0 "${APPLICATION_NAME} (32-bit)"
   ${EndIf}


   !insertmacro INSTALLOPTIONS_EXTRACT "NSIS.InstallOptions.ini"

   ; uncomment this line if you want to see the language selection
   ;!insertmacro MUI_LANGDLL_DISPLAY

   Call SetLang

   ; Remove Quick Launch option from Windows 7, as no longer applicable - usually.
   ${IfNot} ${AtMostWinVista}
      SectionSetText ${SEC_QUICK_LAUNCH} $INIT_NO_QUICK_LAUNCH
      SectionSetFlags ${SEC_QUICK_LAUNCH} ${SF_RO}
      SectionSetInstTypes ${SEC_QUICK_LAUNCH} 0
   ${EndIf}

   ; Some people might have a shortcut called 'ownCloud' pointing elsewhere, see #356
   ; Unselect item and adjust text
   ${If} ${FileExists} "$DESKTOP\${APPLICATION_NAME}.lnk"
      SectionSetText ${SEC_DESKTOP} $INIT_NO_DESKTOP
      Push $0
      SectionSetFlags ${SEC_DESKTOP} 0
      SectionSetInstTypes ${SEC_DESKTOP} 0
      Pop $0
   ${EndIf}

   ${MementoSectionRestore}

   UAC_TryAgain:
      !insertmacro UAC_RunElevated
      ${Switch} $0
      ${Case} 0
          ${IfThen} $1 = 1 ${|} Quit ${|} ;we are the outer process, the inner process has done its work, we are done
          ${IfThen} $3 <> 0 ${|} ${Break} ${|} ;we are admin, let the show go on
          ${If} $1 = 3 ;RunAs completed successfully, but with a non-admin user
             MessageBox mb_YesNo|mb_ICONEXCLAMATION|MB_TOPMOST|MB_SETFOREGROUND $UAC_INSTALLER_REQUIRE_ADMIN /SD IDNO IDYES UAC_TryAgain IDNO 0
          ${EndIf}
          ;fall-through and die
      ${Case} 1223
         MessageBox MB_ICONSTOP|MB_TOPMOST|MB_SETFOREGROUND $UAC_INSTALLER_REQUIRE_ADMIN
         Quit
      ${Case} 1062
         MessageBox MB_ICONSTOP|MB_TOPMOST|MB_SETFOREGROUND $UAC_ERROR_LOGON_SERVICE
         Quit
      ${Default}
         MessageBox MB_ICONSTOP "$UAC_ERROR_ELEVATE $0"
         Abort
         Quit
      ${EndSwitch}

   ;Prevent multiple instances.
   System::Call 'kernel32::CreateMutexA(i 0, i 0, t "${APPLICATION_SHORTNAME}Installer") i .r1 ?e'
   Pop $R0
   StrCmp $R0 0 +3
      MessageBox MB_OK|MB_ICONEXCLAMATION $INIT_INSTALLER_RUNNING
      Abort

   ;Use available InstallLocation when possible. This is useful in the uninstaller
   ;via re-install, which would otherwise use a default location - a bug.
   ReadRegStr $R0 ${MEMENTO_REGISTRY_ROOT} "${MEMENTO_REGISTRY_KEY}" "InstallLocation"
   StrCmp $R0 "" SkipSetInstDir
   StrCpy $INSTDIR $R0
   SkipSetInstDir:

   ;Shutdown ${APPLICATION_NAME} in case Add/Remove re-installer option used.
   Call EnsureOwncloudShutdown

   ReadRegStr $R0 ${MEMENTO_REGISTRY_ROOT} "${MEMENTO_REGISTRY_KEY}" "InstallLocation"
   ${If} ${Silent}
   ${AndIf} $R0 != ""
       ExecWait '"$R0\Uninstall.exe" /S _?=$R0'
   ${EndIf}
FunctionEnd

Function .onInstSuccess
   ${MementoSectionSave}

   ${If} $NoAutomaticUpdates == "yes"
      WriteRegDWORD HKLM "Software\${APPLICATION_VENDOR}\${APPLICATION_NAME}" "skipUpdateCheck" "1"
   ${EndIf}

   ; TODO: Only needed to when updating from 2.1.{0,1}. Remove in due time.
   Delete /REBOOTOK $INSTDIR\bearer\qgenericbearer.dll
   Delete /REBOOTOK $INSTDIR\bearer\qnativewifibearer.dll
   RMDir /REBOOTOK $INSTDIR\bearer

   ${If} ${Silent}
   ${AndIf} $InstallRunIfSilent == "yes"
     Call LaunchApplication
   ${EndIf}

   ; FIX: Return zero on success (previously returned 1223 here)
   SetErrorLevel 0
FunctionEnd

Function .onInstFailed
FunctionEnd

##############################################################################
#                                                                            #
#   NSIS Uninstaller Event Handler Functions                                 #
#                                                                            #
##############################################################################

Function un.onInit
   Call un.SetLang

   UAC_TryAgain:
      !insertmacro UAC_RunElevated
      ${Switch} $0
      ${Case} 0
          ${IfThen} $1 = 1 ${|} Quit ${|} ;we are the outer process, the inner process has done its work, we are done
          ${IfThen} $3 <> 0 ${|} ${Break} ${|} ;we are admin, let the show go on
          ${If} $1 = 3 ;RunAs completed successfully, but with a non-admin user
             MessageBox mb_YesNo|mb_ICONEXCLAMATION|MB_TOPMOST|MB_SETFOREGROUND $UAC_UNINSTALLER_REQUIRE_ADMIN /SD IDNO IDYES UAC_TryAgain IDNO 0
          ${EndIf}
          ;fall-through and die
      ${Case} 1223
         MessageBox MB_ICONSTOP|MB_TOPMOST|MB_SETFOREGROUND $UAC_UNINSTALLER_REQUIRE_ADMIN
         Quit
      ${Case} 1062
         MessageBox MB_ICONSTOP|MB_TOPMOST|MB_SETFOREGROUND $UAC_ERROR_LOGON_SERVICE
         Quit
      ${Default}
         MessageBox MB_ICONSTOP "$UAC_ERROR_ELEVATE $0"
         Abort
         Quit
      ${EndSwitch}

   ;Prevent multiple instances.
   System::Call 'kernel32::CreateMutexA(i 0, i 0, t "${APPLICATION_SHORTNAME}Uninstaller") i .r1 ?e'
   Pop $R0
   StrCmp $R0 0 +3
      MessageBox MB_OK|MB_ICONEXCLAMATION $INIT_UNINSTALLER_RUNNING
      Abort

   ;Shutdown ${APPLICATION_NAME} in order to remove locked files.
   Call un.EnsureOwncloudShutdown
FunctionEnd

Function un.onUnInstSuccess
FunctionEnd

Function un.onUnInstFailed
FunctionEnd
