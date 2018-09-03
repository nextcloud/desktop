@echo off
cls

set PATHTOEXEFILE=%1
set EXEFILENAME=%~nx1

echo "* Upload installer."
start "NSIS" /B /wait scp -i %USERPROFILE%\.ssh\id_rsa %PATHTOEXEFILE% %SFTP_USER%@%SFTP_SERVER%:/var/www/html/desktop/daily/Windows/%EXEFILENAME% > last_daily_upload.log 2>&1

exit