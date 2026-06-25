@echo off
chcp 65001 >nul
title IR Aging Test Tool

:LOOP
cls
adb shell "ps -A | grep 'while :'" >nul 2>&1
if errorlevel 1 (
    echo ========================================
    echo    IR Aging Test Tool
    echo ========================================
    echo.
    echo   [1] Start IR Test
    echo   [2] Exit
    echo.
    echo ========================================
    set /p choice=Select option:
    if "%choice%"=="1" goto START
    if "%choice%"=="2" goto END
    goto LOOP
) else (
    echo ========================================
    echo    IR Aging Test Tool
    echo ========================================
    echo.
    echo   [1] Stop IR Test
    echo   [2] Exit
    echo.
    echo ========================================
    set /p choice=Select option:
    if "%choice%"=="1" goto STOP
    if "%choice%"=="2" goto END
    goto LOOP
)

:START
echo.
echo Starting IR test...
adb shell "while :; do echo -ne '\x30\x02\x00\x00\x9A\x06\x00\x00\x30\x02\x00\x00' > /dev/lirc0; sleep 1; done &"
echo.
echo   IR test started!
echo.
pause
goto LOOP

:STOP
echo.
echo Stopping IR test...
adb shell killall sh >nul 2>&1
echo.
echo   IR test stopped!
echo.
pause
goto LOOP

:END
exit
