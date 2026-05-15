# 测试红外功能

# 1. 检查是否有红外测试 App，或者使用命令行测试
# 方法1：检查红外设备节点
ls -la /dev/lirc0

# 2. 检查红外支持是否被系统识别
dumpsys package android.hardware.consumerir

# 3. 如果有红外测试 App，可以打开测试
# 或者检查 App 的 hasSystemFeature
