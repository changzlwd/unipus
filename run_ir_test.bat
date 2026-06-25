@echo off
chcp 65001 >nul
echo =======================================
echo   红外老化测试 - 一键启动
echo =======================================

echo [1/3] 推送脚本到设备...
adb push "%~dp0ir_test.sh" /data/local/tmp/
if errorlevel 1 goto error

echo [2/3] 设置权限...
adb shell "chmod 755 /data/local/tmp/ir_test.sh"
if errorlevel 1 goto error

echo [3/3] 启动红外测试...
adb shell "nohup /data/local/tmp/ir_test.sh > /data/local/tmp/ir.log 2>&1 &"
if errorlevel 1 goto error

echo.
echo =======================================
echo   启动成功！红外测试正在后台运行
echo =======================================
echo   停止: 双击 stop_ir_test.bat
echo   查看日志: adb shell cat /data/local/tmp/ir.log
echo =======================================
echo.
echo   断开USB后红外测试继续运行
echo   任何时候都能重新插USB停止
echo.
pause
exit /b 0

:error
echo.
echo =======================================
echo   启动失败！请检查：
echo   1. USB连接是否正常
echo   2. adb驱动是否安装
echo   3. 设备是否授权USB调试
echo =======================================
pause
exit /b 1
