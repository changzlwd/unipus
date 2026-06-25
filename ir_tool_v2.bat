@echo off
chcp 65001 >nul
title 红外老化测试工具 v2.0

:MENU
cls
echo ========================================
echo    红外老化测试工具 v2.0（不推送文件）
echo ========================================
echo.
echo   [1] 启动红外测试
echo   [2] 停止红外测试
echo   [3] 查看运行状态
echo   [4] 退出
echo.
echo ========================================
set /p choice=请选择操作 (1-4):

if "%choice%"=="1" goto START
if "%choice%"=="2" goto STOP
if "%choice%"=="3" goto STATUS
if "%choice%"=="4" goto END
goto MENU

:START
echo.
echo 正在启动红外测试（不推送文件）...
adb shell "killall sh" 2>nul
adb shell "nohup sh -c 'while :; do echo -ne ""\\x30\\x02\\x00\\x00\\x9A\\x06\\x00\\x00\\x30\\x02\\x00\\x00"" > /dev/lirc0; sleep 1; done' > /data/local/tmp/ir.log 2>&1 &"
echo.
echo ========================================
echo   启动成功！
echo   断开USB后继续运行
echo ========================================
timeout /t 3 >nul
goto MENU

:STOP
echo.
echo 正在停止红外测试...
adb shell "killall sh" 2>nul
adb shell "pkill -9 -f 'while :'" 2>nul
echo   已停止！
echo ========================================
timeout /t 3 >nul
goto MENU

:STATUS
echo.
echo [进程状态]
adb shell "ps -A | grep -E 'sh|ir_test'" 2>nul
echo.
echo [设备信息]
adb devices
echo.
echo ========================================
pause
goto MENU

:END
exit
