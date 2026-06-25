@echo off
chcp 65001 >nul
title IR Aging Test Tool

:INIT
cls
echo ========================================
echo    IR Aging Test Tool
echo ========================================
echo.
echo   Getting root access...
adb root 2>nul
adb wait-for-device
echo   Done.
echo.
timeout /t 1 >nul

:MENU
cls
echo ========================================
echo    IR Aging Test Tool
echo ========================================
echo.
echo   [1] Start IR Test
echo   [2] Stop IR Test
echo   [3] Exit
echo.
echo ========================================
set /p choice=Select option (1-3):

if "%choice%"=="1" goto START
if "%choice%"=="2" goto STOP
if "%choice%"=="3" goto END
goto MENU

:START
echo.
echo Starting IR test...
adb shell "while :; do echo -ne '\x30\x02\x00\x00\x9A\x06\x00\x00\x30\x02\x00\x00' > /dev/lirc0; sleep 1; done &"
echo.
echo   IR test is running...
echo.
pause
goto MENU

:STOP
echo.
echo Stopping IR test...
adb shell killall sh >nul 2>&1
echo.
echo   IR test stopped!
echo.
pause
goto MENU

:END
exit
