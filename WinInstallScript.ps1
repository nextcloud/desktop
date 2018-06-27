# Run in an admin powershell:
# Set-ExecutionPolicy -ExecutionPolicy Unrestricted

#Requires -RunAsAdministrator

<#
.SYNOPSIS
    .
.DESCRIPTION
    .
.PARAMETER Path
    The path to the .
.PARAMETER LiteralPath
    Specifies a path to one or more locations. Unlike Path, the value of 
    LiteralPath is used exactly as it is typed. No characters are interpreted 
    as wildcards. If the path includes escape characters, enclose it in single
    quotation marks. Single quotation marks tell Windows PowerShell not to 
    interpret any characters as escape sequences.
#>


param(
    # path to a qt installer, can be online or offline
    [String]$qtInstaller,
    # path to qtKeychain zip
    [String]$qtKeychainZip,
    # path to qt (e.g. "C:\Qt\5.9.2\msvc2015")
    [String]$qtPath,
    # number of threads for compiling [default = 1]
    [String]$j,
    # show this help
    [switch]$h,
    # same as -h
    [switch]$help
)

if ($h -or $help) {
    $PSScriptName = $MyInvocation.MyCommand.Name;
    Get-Help "$PSScriptRoot\$PSScriptName" -detailed;
    return;
}

########### Versions #############
$qt ="5.9.2"
$msvc = "msvc2015"
$openSSL = "1_0_2L"
$qtKeychain = "0.8.0"


############# QT #################
$onlineUrl = "http://download.qt.io/official_releases/online_installers/qt-unified-windows-x86-online.exe"
if ($qtInstaller) {
    $outputQt = "$qtInstaller"
} else {
    $outputQt = "$PSScriptRoot\installer.exe"
}
$outputQtAuto = "$PSScriptRoot\install.qs"
$installDirQTx86 = "C:\Qt\$qt\$msvc"
$installDirQTx64 = "C:\Qt\$qt\$msvc_64"

############# QT Keychain ########
if ($qtKeychianZip) {
    $outputKeychain = $qtKeychainZip
} else {
    $qtKeychainUrl = "https://github.com/frankosterfeld/qtkeychain/archive/v$qtKeychain.zip"
    $outputKeychain = "$PSScriptRoot\keychain.zip"
}
$outputKeychainArchive = "$PSScriptRoot"

############# CMake ##############
$cmakeX68 = "https://cmake.org/files/v3.10/cmake-3.10.0-rc1-win32-x86.msi"
$cmakeAMD64 = "https://cmake.org/files/v3.10/cmake-3.10.0-rc1-win64-x64.msi"
$outputCmake = "$PSScriptRoot\cmake.msi"

##### Visual Studio Community ####
$vscUrl = "https://go.microsoft.com/fwlink/?LinkId=532606&clcid=0x409"
$outputVSC = "$PSScriptRoot\vsc.exe"
$outputVSAuto = "$PSScriptRoot\install.xml"

##### openSSL ####
$openSSLx86Url = "http://slproweb.com/download/Win32OpenSSL-$openSSL.exe"
$openSSLx64Url = "http://slproweb.com/download/Win64OpenSSL-$openSSL.exe"
$outputSSL = "$PSScriptRoot\openssl.exe"


##################################
# Set cmake url and check architecture
##################################
if ($ENV:PROCESSOR_ARCHITECTURE -eq "x86")
{
    $pathDev = $pathDevX86onX86
    $cmakeUrl = $cmakeX68
    $installDirQt = $installDirQTx86
    $arch = 'x86'
    $SSLUrl = $openSSLx86Url
} elseif ($ENV:PROCESSOR_ARCHITECTURE -eq "AMD64") {
    $cmakeUrl = $cmakeAMD64
    $installDirQt = $installDirQTx64
    $arch = "x64"
    $SSLUrl = $openSSLx64Url
} else {
    Write-Error "only x86 and x64 are supported"
}

if ($ENV:PATH -contains "cmake") {
    echo "CMake already installed"
} else {
    ##################################
    echo "Downloading Cmake"
    Invoke-WebRequest -Uri $cmakeUrl -OutFile $outputCmake

    ##################################
    echo "Installing Cmake"
    Start-Process $outputCmake -Wait -ArgumentList '/passive'

    # add cmake to system path (permanent) #
    $oldpath = (Get-ItemProperty -Path ‘Registry::HKEY_LOCAL_MACHINE\System\CurrentControlSet\Control\Session Manager\Environment’ -Name PATH).path
    $newpath = "$oldpath;$env:ProgramFiles\Cmake\bin"
    Set-ItemProperty -Path ‘Registry::HKEY_LOCAL_MACHINE\System\CurrentControlSet\Control\Session Manager\Environment’ -Name PATH -Value $newPath
    #### add cmake to path (temporary) #####
    $env:Path = "$env:ProgramFiles\Cmake\bin;$env:Path"
}
##################################
# write automatic script file to hdd
'<?xml version="1.0" encoding="utf-8"?>
<AdminDeploymentCustomizations xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns="http://schemas.microsoft.com/wix/2011/AdminDeployment">
  <BundleCustomizations TargetDir="C:\Program Files (x86)\Microsoft Visual Studio 14.0" NoCacheOnlyMode="default" NoWeb="default" NoRefresh="default" SuppressRefreshPrompt="default" Feed="default" />
  <SelectableItemCustomizations>
    <SelectableItemCustomization Id="NativeLanguageSupport_VCV1" Hidden="no" Selected="yes" FriendlyName="Common Tools for Visual C++ 2015" />
    <SelectableItemCustomization Id="Win10_VSToolsV13" Hidden="no" Selected="yes" FriendlyName="Tools for Universal Windows Apps (1.4.1) and Windows 10 SDK (10.0.14393)" />
    <SelectableItemCustomization Id="NetFX4Hidden" Selected="yes" FriendlyName="NetFX4Hidden" />
    <SelectableItemCustomization Id="NetFX45Hidden" Selected="yes" FriendlyName="NetFX45Hidden" />
    <SelectableItemCustomization Id="NetFX451MTPackHidden" Selected="yes" FriendlyName="NetFX451MTPackHidden" />
    <SelectableItemCustomization Id="NetFX451MTPackCoreHidden" Selected="yes" FriendlyName="NetFX451MTPackCoreHidden" />
    <SelectableItemCustomization Id="NetFX452MTPackHidden" Selected="yes" FriendlyName="NetFX452MTPackHidden" />
    <SelectableItemCustomization Id="NetFX46MTPackHidden" Selected="yes" FriendlyName="NetFX46MTPackHidden" />
    <SelectableItemCustomization Id="MicroUpdateV3.5" Selected="no" FriendlyName="Update for Microsoft Visual Studio 2015 (KB3165756)" />
    <SelectableItemCustomization Id="WebToolsV1" Hidden="no" Selected="no" FriendlyName="Microsoft Web Developer Tools" />
    <SelectableItemCustomization Id="JavaScript_HiddenV12" Selected="no" FriendlyName="JavaScript Project System for Visual Studio" />
    <SelectableItemCustomization Id="Win10SDK_HiddenV3" Hidden="no" Selected="no" FriendlyName="Windows 10 SDK (10.0.10586)" />
    <SelectableItemCustomization Id="MDDJSDependencyHiddenV1" Selected="no" FriendlyName="MDDJSDependencyHidden" />
    <SelectableItemCustomization Id="AppInsightsToolsVisualStudioHiddenVSU3RTMV1" Selected="no" FriendlyName="Developer Analytics Tools v7.0.2" />
    <SelectableItemCustomization Id="Silverlight5_DRTHidden" Selected="no" FriendlyName="Silverlight5_DRTHidden" />
    <SelectableItemCustomization Id="BlissHidden" Selected="no" FriendlyName="BlissHidden" />
    <SelectableItemCustomization Id="HelpHidden" Selected="no" FriendlyName="HelpHidden" />
    <SelectableItemCustomization Id="JavaScript" Selected="no" FriendlyName="JavascriptHidden" />
    <SelectableItemCustomization Id="PortableDTPHidden" Selected="no" FriendlyName="PortableDTPHidden" />
    <SelectableItemCustomization Id="PreEmptiveDotfuscatorHidden" Selected="no" FriendlyName="PreEmptiveDotfuscatorHidden" />
    <SelectableItemCustomization Id="PreEmptiveAnalyticsHidden" Selected="no" FriendlyName="PreEmptiveAnalyticsHidden" />
    <SelectableItemCustomization Id="ProfilerHidden" Selected="no" FriendlyName="ProfilerHidden" />
    <SelectableItemCustomization Id="RoslynLanguageServicesHidden" Selected="no" FriendlyName="RoslynLanguageServicesHidden" />
    <SelectableItemCustomization Id="SDKTools3Hidden" Selected="yes" FriendlyName="SDKTools3Hidden" />
    <SelectableItemCustomization Id="SDKTools4Hidden" Selected="yes" FriendlyName="SDKTools4Hidden" />
    <SelectableItemCustomization Id="WCFDataServicesHidden" Selected="no" FriendlyName="WCFDataServicesHidden" />
    <SelectableItemCustomization Id="VSUV1PreReqV1" Selected="no" FriendlyName="Visual Studio 2015 Update 1 Prerequisite" />
    <SelectableItemCustomization Id="VSUV3RTMV1" Selected="no" FriendlyName="Visual Studio 2015 Update 3" />
    <SelectableItemCustomization Id="NativeLanguageSupport_MFCV1" Hidden="no" Selected="no" FriendlyName="Microsoft Foundation Classes for C++" />
    <SelectableItemCustomization Id="NativeLanguageSupport_XPV1" Hidden="no" Selected="no" FriendlyName="Windows XP Support for C++" />
    <SelectableItemCustomization Id="Win81SDK_HiddenV1" Hidden="no" Selected="no" FriendlyName="Windows 8.1 SDK and Universal CRT SDK" />
    <SelectableItemCustomization Id="FSharpV1" Hidden="no" Selected="no" FriendlyName="Visual F#" />
    <SelectableItemCustomization Id="PythonToolsForVisualStudioV8" Hidden="no" Selected="no" FriendlyName="Python Tools for Visual Studio (January 2017)" />
    <SelectableItemCustomization Id="ClickOnceV1" Hidden="no" Selected="no" FriendlyName="ClickOnce Publishing Tools" />
    <SelectableItemCustomization Id="SQLV1" Hidden="no" Selected="no" FriendlyName="Microsoft SQL Server Data Tools" />
    <SelectableItemCustomization Id="PowerShellToolsV1" Hidden="no" Selected="no" FriendlyName="PowerShell Tools for Visual Studio" />
    <SelectableItemCustomization Id="SilverLight_Developer_KitV1" Hidden="no" Selected="no" FriendlyName="Silverlight Development Kit" />
    <SelectableItemCustomization Id="Windows10_ToolsAndSDKV13" Hidden="no" Selected="no" FriendlyName="Tools (1.4.1) and Windows 10 SDK (10.0.14393)" />
    <SelectableItemCustomization Id="Win10_EmulatorV1" Selected="no" FriendlyName="Emulators for Windows 10 Mobile (10.0.10240)" />
    <SelectableItemCustomization Id="Win10_EmulatorV2" Selected="no" FriendlyName="Emulators for Windows 10 Mobile (10.0.10586)" />
    <SelectableItemCustomization Id="Win10_EmulatorV3" Hidden="no" Selected="no" FriendlyName="Emulators for Windows 10 Mobile (10.0.14393)" />
    <SelectableItemCustomization Id="XamarinVSCoreV7" Hidden="no" Selected="no" FriendlyName="C#/.NET (Xamarin v4.2.1)" />
    <SelectableItemCustomization Id="XamarinPT_V1" Selected="no" FriendlyName="Xamarin Preparation Tool" />
    <SelectableItemCustomization Id="MDDJSCoreV11" Hidden="no" Selected="no" FriendlyName="HTML/JavaScript (Apache Cordova) Update 10" />
    <SelectableItemCustomization Id="AndroidNDK11C_V1" Hidden="no" Selected="no" FriendlyName="Android Native Development Kit (R11C, 32 bits)" />
    <SelectableItemCustomization Id="AndroidNDK11C_32_V1" Hidden="no" Selected="no" FriendlyName="Android Native Development Kit (R11C, 32 bits)" />
    <SelectableItemCustomization Id="AndroidNDK11C_64_V1" Hidden="no" Selected="no" FriendlyName="Android Native Development Kit (R11C, 64 bits)" />
    <SelectableItemCustomization Id="AndroidNDKV1" Hidden="no" Selected="no" FriendlyName="Android Native Development Kit (R10E, 32 bits)" />
    <SelectableItemCustomization Id="AndroidNDK_32_V1" Hidden="no" Selected="no" FriendlyName="Android Native Development Kit (R10E, 32 bits)" />
    <SelectableItemCustomization Id="AndroidNDK_64_V1" Hidden="no" Selected="no" FriendlyName="Android Native Development Kit (R10E, 64 bits)" />
    <SelectableItemCustomization Id="AndroidSDKV1" Hidden="no" Selected="no" FriendlyName="Android SDK" />
    <SelectableItemCustomization Id="AndroidSDK_API1921V1" Hidden="no" Selected="no" FriendlyName="Android SDK Setup (API Level 19 and 21)" />
    <SelectableItemCustomization Id="AndroidSDK_API22V1" Hidden="no" Selected="no" FriendlyName="Android SDK Setup (API Level 22)" />
    <SelectableItemCustomization Id="AndroidSDK_API23V1" Hidden="no" Selected="no" FriendlyName="Android SDK Setup (API Level 23)" />
    <SelectableItemCustomization Id="AntV1" Hidden="no" Selected="no" FriendlyName="Apache Ant (1.9.3)" />
    <SelectableItemCustomization Id="L_MDDCPlusPlus_iOS_V7" Hidden="no" Selected="no" FriendlyName="Visual C++ iOS Development (Update 3)" />
    <SelectableItemCustomization Id="L_MDDCPlusPlus_Android_V7" Hidden="no" Selected="no" FriendlyName="Visual C++ Android Development (Update 3)" />
    <SelectableItemCustomization Id="L_MDDCPlusPlus_ClangC2_V6" Hidden="no" Selected="no" FriendlyName="Clang with Microsoft CodeGen (July 2016)" />
    <SelectableItemCustomization Id="L_IncrediBuild_V1" Selected="no" FriendlyName="IncrediBuild" />
    <SelectableItemCustomization Id="Node.jsV1" Hidden="no" Selected="no" FriendlyName="Joyent Node.js" />
    <SelectableItemCustomization Id="VSEmu_AndroidV1.1.622.2" Hidden="no" Selected="no" FriendlyName="Microsoft Visual Studio Emulator for Android (July 2016)" />
    <SelectableItemCustomization Id="WebSocket4NetV1" Hidden="no" Selected="no" FriendlyName="WebSocket4Net" />
    <SelectableItemCustomization Id="ToolsForWin81_WP80_WP81V1" Hidden="no" Selected="no" FriendlyName="Tools and Windows SDKs" />
    <SelectableItemCustomization Id="WindowsPhone81EmulatorsV1" Hidden="no" Selected="no" FriendlyName="Emulators for Windows Phone 8.1" />
    <SelectableItemCustomization Id="GitForWindowsx64V8" Hidden="no" Selected="no" FriendlyName="Git for Windows" />
    <SelectableItemCustomization Id="GitForWindowsx86V8" Hidden="no" Selected="no" FriendlyName="Git for Windows" />
    <SelectableItemCustomization Id="GitHubVSV1" Hidden="no" Selected="no" FriendlyName="GitHub Extension for Visual Studio" />
    <SelectableItemCustomization Id="VS_SDK_GroupV5" Hidden="no" Selected="no" FriendlyName="Visual Studio Extensibility Tools Update 3" />
    <SelectableItemCustomization Id="VS_SDK_Breadcrumb_GroupV5" Selected="no" FriendlyName="Visual Studio Extensibility Tools Update 3" />
    <SelectableItemCustomization Id="Win10SDK_HiddenV1" Hidden="no" Selected="no" FriendlyName="Windows 10 SDK (10.0.10240)" />
    <SelectableItemCustomization Id="Win10SDK_HiddenV2" Selected="no" FriendlyName="Windows 10 SDK (10.0.10586)" />
    <SelectableItemCustomization Id="Win10SDK_HiddenV4" Selected="no" FriendlyName="Windows 10 SDK (10.0.14393)" />
    <SelectableItemCustomization Id="Win10SDK_VisibleV1" Hidden="no" Selected="no" FriendlyName="Windows 10 SDK 10.0.10240" />
    <SelectableItemCustomization Id="UWPPatch_KB3073097_HiddenV3" Selected="no" FriendlyName="KB3073097" />
    <SelectableItemCustomization Id="AppInsightsToolsVSWinExpressHiddenVSU3RTMV1" Selected="no" FriendlyName="Developer Analytics Tools v7.0.2" />
    <SelectableItemCustomization Id="AppInsightsToolsVWDExpressHiddenVSU3RTMV1" Selected="no" FriendlyName="Developer Analytics Tools v7.0.2" />
    <SelectableItemCustomization Id="UWPStartPageV1" Selected="no" FriendlyName="Tools for Universal Windows Apps Getting Started Experience" />
  </SelectableItemCustomizations>
</AdminDeploymentCustomizations>' | out-file -encoding ASCII $outputVSAuto

##################################
if (!(Test-Path("${env:ProgramFiles(x86)}\Microsoft Visual Studio 14.0\VC"))) {
    echo "Downloading Visual Studio Community"
    Invoke-WebRequest -Uri $vscUrl -OutFile $outputVSC

    ####### install VS Studio ########
    echo "Execute Visual Studio Community install"
    # Start-Process $outputVSC -Wait -ArgumentList "/passive /AdminFile $outputVSAuto"
    Start-Process $outputVSC -Wait -ArgumentList "/AdminFile $outputVSAuto"
}

##################################
if (Test-Path($outputQt)) {
    echo "QT Online Installer already downloaded"
} else {
    echo "Downloading QT Online Installer"
    Invoke-WebRequest -Uri $onlineUrl -OutFile $outputQt
}

##################################
# write automatic script file to hdd
'function Controller() {
    installer.autoRejectMessageBoxes();
    installer.installationFinished.connect(function() {
        gui.clickButton(buttons.NextButton);
    })
}

Controller.prototype.WelcomePageCallback = function() {
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.CredentialsPageCallback = function() {
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.IntroductionPageCallback = function() {
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.TargetDirectoryPageCallback = function()
{
//    gui.currentPageWidget().TargetDirectoryLineEdit.setText(installer.value("HomeDir") + "/Qt");
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.ComponentSelectionPageCallback = function() {
    var widget = gui.currentPageWidget();

    widget.deselectAll();
	if(systemInfo.currentCpuArchitecture.search("64") < 0) {	// 32 bit
		widget.selectComponent("qt.592.win32_msvc2015");
	}
	else {														// 64 bit
//		widget.selectComponent("qt.592.win32_msvc2015");
		widget.selectComponent("qt.592.win64_msvc2015_64");
	}
	widget.selectComponent("qt.592.qtwebengine");
//	widget.selectComponent("qt.tools.ifw.20");
//	widget.selectComponent("qt.tools.ifw.30");
	widget.selectComponent("qt.tools.qtcreatorcdbext");
	
    // widget.selectComponent("qt.592.qtnetworkauth");
    // widget.deselectComponent("qt.592.qtremoteobjects");

    gui.clickButton(buttons.NextButton);
}

Controller.prototype.LicenseAgreementPageCallback = function() {
    gui.currentPageWidget().AcceptLicenseRadioButton.setChecked(true);
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.StartMenuDirectoryPageCallback = function() {
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.ReadyForInstallationPageCallback = function()
{
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.FinishedPageCallback = function() {
var checkBoxForm = gui.currentPageWidget().LaunchQtCreatorCheckBoxForm
if (checkBoxForm && checkBoxForm.launchQtCreatorCheckBox) {
    checkBoxForm.launchQtCreatorCheckBox.checked = false;
}
    gui.clickButton(buttons.FinishButton);
}' | out-file -encoding ASCII $outputQtAuto

##################################
echo "Execute QT install"
Start-Process $outputQt -Wait -ArgumentList "--script $outputQtAuto --no-force-installations"

#### add QT to path (temporary) #####
$env:Path = "C:\Qt\Tools\QtCreator\bin;$env:Path"


########## PATH ENV ##############
############## VS ################
pushd "${env:ProgramFiles(x86)}\Microsoft Visual Studio 14.0\VC"
cmd /c "vcvarsall.bat $arch&set" |
foreach {
  if ($_ -match "=") {
    $v = $_.split("="); set-item -force -path "ENV:\$($v[0])"  -value "$($v[1])"
  }
}
popd

############## QT ################
pushd "$installDirQT\bin"
cmd /c "qtenv2.bat&set" |
foreach {
  if ($_ -match "=") {
    $v = $_.split("="); set-item -force -path "ENV:\$($v[0])"  -value "$($v[1])"
  }
}
popd

##################################
echo "Downloading QTKeychain source"
Invoke-WebRequest -Uri $qtKeychainUrl -OutFile $outputKeychain
echo "Extracting QTKeychain source"
Expand-Archive $outputKeychain -DestinationPath $PSScriptRoot

# compile QTKeychain
cd $PSScriptRoot\qtkeychain-0.8.0
cmake -G"CodeBlocks - NMake Makefiles" .\CMakeLists.txt
nmake

# copy into qt folder
cp *.dll $installDirQT\bin
cp *.lib $installDirQT\lib
cp *.qm $installDirQt\translations
cp keychain.h $installDirQt\include
cp qkeychain_export.h $installDirQt\include
cp qt_*.pri $installDirQt\mkspecs
mkdir $installDirQt\lib\cmake\Qt5Keychain -Force
cp Qt*.cmake $installDirQt\lib\cmake\Qt5Keychain
cp .\CMakeFiles\Export\lib\cmake\Qt5Keychain\* $installDirQt\lib\cmake\Qt5Keychain

# back to root dir
cd $PSScriptRoot

# openSSL with installers
Invoke-WebRequest -Uri $SSLUrl -OutFile $outputSSL
Start-Process $outputSSL -Wait -ArgumentList '/silent'

cd $PSScriptRoot
# nextcloud client
Start-Process cmake -Wait -ArgumentList '-G"CodeBlocks - NMake Makefiles JOM" .\CMakeLists.txt -DNO_SHIBBOLETH=1'
if (-not($j)) {
    $j = "1" # nr of threads for compiling
}
Start-Process jom -Wait -ArgumentList '/J $j'

##################################
# cleanup
Remove-Item $outputVSC
Remove-Item $outputQt
Remove-Item $outputCmake
Remove-Item $outputKeychain
Remove-Item $outputQtAuto
Remove-Item $outputVSAuto
Remove-Item $outputSSL
Remove-Item $PSScriptRoot\qtkeychain*