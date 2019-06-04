@echo off

REM Based on an idea of:
REM https://superuser.com/questions/1287756/how-can-i-get-the-date-in-a-locale-independent-format-in-a-batch-file/1287820#1287820
REM for /f "usebackq skip=1 tokens=1-3" %%g in (`wmic Path Win32_LocalTime Get Day^,Month^,Year ^| findstr /r /v "^$"`) do (
REM   set _day=00%%g
REM   set _month=00%%h
REM   set _year=%%i
REM   )
REM wmic Path Win32_LocalTime Get Day^,Month^,Year

REM Day  Month  Year
REM 23   5      2019

for /f "usebackq tokens=1-2" %%g in (`sh "%~dp0/datetime.inc.callee.sh" ^| findstr /r /v "^$"`) do (
    set _date=%%g
    set _time=%%h
)
REM echo %_date%
REM echo %_time%
REM echo %_date:~0,4%%_date:~5,2%%_date:~8,2%
