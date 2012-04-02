----------------------------------------------------------------
----------------------------------------------------------------
Processes (Processes.dll)
Version:	1.0.1.0
Release:	24.february.2005
Description:	Nullsoft Installer (NSIS) plug-in for managing?! 
		Windows processes.

Copyright:	© 2004-2005 Hardwired. No rights reserved.
		There is no restriction and no guaranty for using
		this software.

Author:		Andrei Ciubotaru [Hardwired]
		Lead Developer ICode&Ideas SRL (http://www.icode.ro/)
		hardwiredteks@gmail.com, hardwired@icode.ro

----------------------------------------------------------------
----------------------------------------------------------------
INTRODUCTION

	The Need For Plug-in - I need it for the one of my installers.
	
	Briefly: Use it when you need to find\kill a process when
installing\uninstalling some application. Also, use it when you 
need to test the presence of a device driver.


SUPPORT
	
	Supported platforms are: WinNT,Win2K,WinXP and Win2003 Server.


DESCRIPTION

	Processes::FindProcess <process_name>	;without ".exe"
	
		Searches the currently running processes for the given
		process name.
		
		return:	1	- the process was found
			0	- the process was not found
	
	Processes::KillProcess <process_name>	; without ".exe"
	
		Searches the currently running processes for the given
		process name. If the process is found then the it gets
		killed.
		
		return:	1	- the process was found and killed
			0	- the process was not found or the process
						cannot be killed (insuficient rights)
	
	Processes::FindDevice <device_base_name>
	
		Searches the installed devices drivers for the given
		device base name.
		(important: I said BASE NAME not FILENAME)
		
		return:	1	- the device driver was found
			0	- the device driver was not found
				

USAGE

	First of all, does not matter where you use it. Ofcourse, the
routines must be called inside of a Section/Function scope.

	Processes::FindProcess "process_name"
	Pop $R0
 
	StrCmp $R0 "1" make_my_day noooooo
	
	make_my_day:
		...
	
	noooooo:
		...
		
	
	Processes::KillProcess "process_name"
	Pop $R0
 
	StrCmp $R0 "1" dead_meat why_wont_you_die
	
	dead_meat:
		...
	
	why_wont_you_die:
		...
		

	Processes::FindDevice "device_base_name"
	Pop $R0
 
	StrCmp $R0 "1" blabla more_blabla
	
	blabla:
		...
	
	more_blabla:
		...
		
	
THANKS

	Sunil Kamath for inspiring me. I wanted to use its FindProcDLL
but my requirements made it imposible.

	Nullsoft for creating this very powerfull installer. One big,
free and full-featured (hmmm... and guiless for the moment) mean
install machine!:)

	ME for being such a great coder...
						... HAHAHAHAHAHAHA!
										
ONE MORE THING

	If you use the plugin or it's source-code, I would apreciate
if my name is mentioned.

----------------------------------------------------------------
----------------------------------------------------------------
