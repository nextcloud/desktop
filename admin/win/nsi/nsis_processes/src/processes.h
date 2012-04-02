#pragma once





//-------------------------------------------------------------------------------------------
// PSAPI function pointers
typedef BOOL	(WINAPI *lpfEnumProcesses)			( DWORD *, DWORD, DWORD * );
typedef BOOL	(WINAPI *lpfEnumProcessModules)		( HANDLE, HMODULE *, DWORD, LPDWORD );
typedef DWORD	(WINAPI *lpfGetModuleBaseName)		( HANDLE, HMODULE, LPTSTR, DWORD );
typedef BOOL	(WINAPI *lpfEnumDeviceDrivers)		( LPVOID *, DWORD, LPDWORD );
typedef BOOL	(WINAPI *lpfGetDeviceDriverBaseName)( LPVOID, LPTSTR, DWORD );






//-------------------------------------------------------------------------------------------
// Internal use routines
bool	LoadPSAPIRoutines( void );
bool	FreePSAPIRoutines( void );

bool	FindProc( char *szProcess );
bool	KillProc( char *szProcess );

bool	FindDev( char *szDriverName );





//-------------------------------------------------------------------------------------------
// Exported routines
extern "C" __declspec(dllexport) void	FindProcess( HWND		hwndParent, 
													 int		string_size,
													 char		*variables, 
													 stack_t	**stacktop );

extern "C" __declspec(dllexport) void	KillProcess( HWND		hwndParent, 
													 int		string_size,
													 char		*variables, 
													 stack_t	**stacktop );

extern "C" __declspec(dllexport) void	FindDevice(  HWND		hwndParent, 
													 int		string_size,
													 char		*variables, 
													 stack_t	**stacktop );
