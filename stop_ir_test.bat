@echo off
chcp 65001 >nul
echo =======================================
echo   停止红外老化测试
echo =======================================
adb shell "killall ir_test.sh"
if errorlevel 1 (
    echo   停止失败！尝试强制结束...
    adb shell "pkill -9 -f ir_test.sh"
)
echo   已停止！
echo =======================================
pause
