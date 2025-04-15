// writes a message to the MSI logs
function logInfo(message) {
    var record = Session.Installer.CreateRecord(0);
    record.stringData(0) = message
    // 0x40000000 = msiMessageTypeUser -- see https://learn.microsoft.com/en-gb/windows/win32/msi/session-message#parameters
    Session.Message(0x04000000, record)
}

function EnsureACL() {
    var shell = new ActiveXObject("WScript.Shell");
    var fs = new ActiveXObject("Scripting.FileSystemObject");

    var programFilesPath = fs.GetAbsolutePathName(shell.ExpandEnvironmentStrings("%PROGRAMFILES%"));
    var installPath = fs.GetAbsolutePathName(Session.Property("CustomActionData"));

    logInfo("programFilesPath: " + programFilesPath + "\r\n" + "installPath: " + installPath);

    if (installPath.toLowerCase().indexOf(programFilesPath.toLowerCase()) == 0) {
        // no need to adapt ACLs when installing to C:/Program Files
        return 0;
    }

    // using SIDs here (prefixed by *) to avoid potential issues with non-English installs
    // see also: https://learn.microsoft.com/en-us/windows/win32/secauthz/well-known-sids
    var grants = [
        "*S-1-5-32-544:(OI)(CI)F",    // DOMAIN_ALIAS_RID_ADMINS    => full access
        "*S-1-5-18:(OI)(CI)F",        // SECURITY_LOCAL_SYSTEM_RID  => full access
        "*S-1-5-32-545:(OI)(CI)RX"    // DOMAIN_ALIAS_RID_USERS     => read, execute
    ];
    var grantsOptions = "";
    for (var i = 0; i < grants.length; i++) {
        grantsOptions += ' /grant "' + grants[i] + '" ';
    }

    var icaclsCommand = 'icacls.exe "' + installPath + '" /inheritance:r ' + grantsOptions;
    logInfo("Command: " + icaclsCommand);
    var retval = shell.Run(icaclsCommand, 0, true);
    if (retval != 0) {
        logInfo("Return code: " + retval);
        return 1603; // fatal error
    }

    return 0;
}
