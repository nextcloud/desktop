@echo off
cls

set EXEFILE=%1

echo "* Upload installer."
start "NSIS" /B /wait pscp -sftp -load %SSH_SESSION_NAME% %EXEFILE% %SFTP_USER%@%SFTP_SERVER%:/var/www/html/desktop/daily/Windows

