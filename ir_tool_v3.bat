@echo off
chcp 65001 >nul
title IR Aging Test Tool v3.0

:MENU
cls
echo ========================================
echo    IR Aging Test Tool v3.0
echo ========================================
echo.
echo   [1] Start IR Test
echo   [2] Stop IR Test
echo   [3] Check Status
echo   [4] Exit
echo.
echo ========================================
set /p choice=Select option (1-4):

if "%choice%"=="1" goto START
if "%choice%"=="2" goto STOP
if "%choice%"=="3" goto STATUS
if "%choice%"=="4" goto END
goto MENU

:START
echo.
echo Starting IR test...
adb shell killall sh >nul 2>&1
adb shell "while :; do echo -ne '\x30\x02\x00\x00\x9A\x06\x00\x00\x30\x02\x00\x00' > /dev/lirc0; sleep 1; done &"
echo.
echo ========================================
echo   Started successfully!
echo   IR test running in background
echo ========================================
timeout /t 3 >nul
goto MENU

:STOP
echo.
echo Stopping IR test...
adb shell killall sh >nul 2>&1
echo   Stopped!
echo ========================================
timeout /t 3 >nul
goto MENU

:STATUS
echo.
echo [Process Status]
adb shell "ps -A | grep sh" 2>nul
echo.
echo [Device List]
adb devices
echo.
echo ========================================
pause
goto MENU

:END
exit
