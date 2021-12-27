On Error goto 0

Const HKEY_LOCAL_MACHINE = &H80000002

Const strObjRegistry = "winmgmts:\\.\root\default:StdRegProv"

Function RegistryDeleteKeyRecursive(regRoot, strKeyPath)
  Set objRegistry = GetObject(strObjRegistry)
  objRegistry.EnumKey regRoot, strKeyPath, arrSubkeys
  If IsArray(arrSubkeys) Then
    For Each strSubkey In arrSubkeys
      RegistryDeleteKeyRecursive regRoot, strKeyPath & "\" & strSubkey
    Next
  End If
  objRegistry.DeleteKey regRoot, strKeyPath
End Function

Function RegistryListSubkeys(regRoot, strKeyPath)
  Set objRegistry = GetObject(strObjRegistry)
  objRegistry.EnumKey regRoot, strKeyPath, arrSubkeys
  RegistryListSubkeys = arrSubkeys
End Function

Function GetUserSID()
  Dim objWshNetwork, objUserAccount
  
  Set objWshNetwork = CreateObject("WScript.Network")

  Set objUserAccount = GetObject("winmgmts://" & objWshNetwork.UserDomain & "/root/cimv2").Get("Win32_UserAccount.Domain='" & objWshNetwork.ComputerName & "',Name='" & objWshNetwork.UserName & "'")
  GetUserSID = objUserAccount.SID
End Function

Function RegistryCleanupSyncRootManager()
  strSyncRootManagerKeyPath = "SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\SyncRootManager"

  arrSubKeys = RegistryListSubkeys(HKEY_LOCAL_MACHINE, strSyncRootManagerKeyPath)
  
  If IsArray(arrSubkeys) Then
    arrSubkeys=Filter(arrSubkeys, Session.Property("APPNAME"))
  End If
  If IsArray(arrSubkeys) Then
    arrSubkeys=Filter(arrSubkeys, GetUserSID())
  End If

  If IsArray(arrSubkeys) Then
    For Each strSubkey In arrSubkeys
      RegistryDeleteKeyRecursive HKEY_LOCAL_MACHINE, strSyncRootManagerKeyPath & "\" & strSubkey
    Next
  End If
End Function

Function RegistryCleanup()
  RegistryCleanupSyncRootManager()
End Function
