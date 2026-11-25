# 4j yaw 双端回中 worker r1

日期：2026-11-24；item5；首次实现；未提交、未push、未烧录。

## 冻结身份

- 工作区：`C:/Users/Lenovo/orca/workspaces/DartLauncher/auto-4j-yaw-m2006-2`
- 实际分支：`xiumu/auto/4j-yaw-m2006-2`。Orca创建的名称与定稿`auto/4j-yaw-m2006`不同；曾询问协调者，收到协调者已停止、由worker自行决定的系统消息后选择沿用已授权隔离工作区，未自行换checkout。
- 基线及实际HEAD：`e17b490e8ffbefedc08c96d8d74396ba257487de`；没有新提交。
- 冻结diff：上述工作区 `build-host/4j-r1.diff`，命令`git diff --binary --no-ext-diff`的原始字节。
- diff SHA256：`4308cd4673d5ca6a23bf4c245ca66fd195acdaae4b883a24f9cca53de4002d49`。
- 精确执行依据：`plans/tasks/4j-yaw-dual-end-final-v2.md`，没有执行旧手动版/单端版。

## 实现

仅修改8个授权跟踪文件：`src/dart_params.hpp`、`src/motor_axis.hpp`、`src/motor_axis.cpp`、`src/hw/launcher_hw.hpp`、`src/hw/launcher_hw.cpp`、`src/launcher_io.hpp`、`test/host_test.cpp`、`README.md`。

MotorAxis增加默认关闭的双端模式、只读阶段/故障/span/边界；负向寻端、退离、正向寻端、退离、回中，外部Seeking/Retreating契约维持，累计原始counts计算中点，成功保持active目标0。接触必须有新反馈，速度、电流、累计计数位移观察同时满足；反馈较慢时只在新样本到达推进观察时间。各阶段Stop/无力中止，离线使已完成学习失效；不自动恢复。重测清旧span和窗口但原始counts计算不受旧offset污染。

yaw启用，输出轴rad配置，600转子rpm、848回零电流、2829正常电流、4~90输出轴圈、退离0.5圈、边距1圈、1000s/30s及0.3s稳定按定稿。Manual直接Jog输出轴rad/s；SetYaw/Aimed与日志改输出轴坐标，快照增加phase/fault/span/cmd/act/rpm，act只表示驱动原始反馈逆算。StopMotors仿照pitch保留yaw当前位置目标。普通轴默认行为保留，4i摆臂/推杆仍false。

## 主机门禁

输出目录已确认在隔离工作区`build-host`。使用Webots原有g++环境：

```powershell
$env:PATH = 'C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;' + $env:PATH
$src = (Get-ChildItem -LiteralPath src -Filter '*.cpp').FullName
g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test/host_test.cpp $src -o build-host/host_test_4j.exe
.\build-host\host_test_4j.exe
g++ -std=c++17 -Wall -Wextra -Werror -O1 -Itest/hx711_stubs -Isrc/hw test/hx711_port_test.cpp src/hw/hx711_port.cpp -o build-host/hx711_port_test_4j.exe
.\build-host\hx711_port_test_4j.exe
```

最终两次编译退出0，执行输出：`PASS: 2335266 checks`、`PASS: 2193 HX711 checks`。
`git diff --check`退出0。`python build-host/freeze_evidence.py`检查src顶层全部cpp/hpp include，无新增HAL/LibXR/FreeRTOS依赖，输出`TOP LEVEL DEPENDENCY VIOLATIONS []`。

开发中第一次动态测试错误把RampTo当前斜坡目标当最终目标，输出`FAIL: learned RampTo clamp`；按接口含义修正断言后通过，没有改首轮常量。加入编码器跨圈反馈测试时忘初始化RotorCounter参考，输出`FAIL: dual raw arithmetic midpoint`；初始化计数器后通过，没有改变算法去凑测试。曾对同一补丁重复调用导致工具报上下文不匹配，最终文件经重新编译验证；无非零编译结果，编译后才运行新二进制。

动态覆盖：电流驱动惯性+硬端点模型，中间/两端/非对称/负多圈起点、RotorCounter跨8192圈界及10ms反馈、重复回零、旧offset/边界清除、回中未稳/转速超阈值不Homed及稳定重计时、零rpm但位置持续移动不认接触、无新反馈不认接触、短跨度/超长/无端超程、两次退离阻塞/回中阻塞精确故障、模拟1000s总超时、五个阶段分别Stop/无力/离线后不自启动、ClearFault后完整重测、动态边界SetTarget/RampTo/Jog/HoldPosition、手动释放保持/Settled；真实yaw MotorAxis反馈及状态驱动MatchFlow普通4f协调，yaw回中前不请求滑块，完成才请求滑块。原host_test其余测试全部保留运行，包含pitch、滑块、摆臂原回归。

## ARM门禁与证据

只读输入：`C:/Users/Lenovo/Desktop/xrobot/dart-fw/build/debug/compile_commands.json`。
实际命令：`python build-host/arm_precheck.py`，退出0。该脚本逐项替换DartLauncher源码和include前缀为隔离目录，删原-o/-MF/-MT/-MQ/-MD/-MMD/-MP输出/依赖参数，原选项不变；原编译目录只作为cwd，所有新obj输出到本工作区build-host。

25个DartLauncher单元全部退出0：dart_inventory、dbus_frame、dbus_framer、hw/hx711_port、hw/launcher_hw、hw/magnet_port、hw/motor_port、hw/referee_port、hw/remote_port、hw/servo_port、hw/tension_port、launch_sequence、magnet_guard、match_flow、mode_logic、motor_axis、pid、referee_tracker、rotor_counter、safety、servo_ramp、slider_pair、stall_guard、tension_modbus、yaw_geometry。

完整每项实际命令、输入路径、exit、stdout/stderr、输入及object SHA256均在`build-host/arm_evidence.json`，SHA256 `daef8e759d8333b92e8540f03a1544edbf052125b82a659709647da7a6a1b3b0`。脚本也保留供复核。这是新源码ARM编译预检，不是完整链接ELF。

最终host exe SHA256 `73ef64de60918d4e382055330f0353f82f334e76ea4b9653ce293c921311bdc4`；HX711 exe SHA256 `2e69fd26bb691d161fadb650a8e3df77f3bf12c9bdd3fc5884a7f7e6cba55dee`。

## 完整ELF与未验证

没有构建新完整ELF。原正式`cmake/LibXR.CMake:6`用普通set固定`XROBOT_MODULES_DIR`为原工程Modules，不能用cache变量覆盖到隔离源码；本轮没有为此修改工程配置、提交/同步模块或用旧ELF冒充。完整ELF列为用户授权模块同步后的后续集成门禁。

只读核对原固件HEAD仍`fa0838b6b405db715d3be8eb552784da49f3cc35`，status仍`M Core/Src/main.c`、`M User/RobotConfig/artisans_dart.yaml`、`?? build-4g-final.log`；本轮未写原源码、固件或Referee，USART1/YAML/拉力及Referee补丁保留。MatchFlow普通及比赛回零等待未发现小于yaw总时限的外层计时器，无越清单修改。

本轮主机与真实新源码ARM预检通过，待独立reviewer明确APPROVED。未现场验证方向、848是否能带动、实测span、接触真实性与行程中点；窗口不能识别所有合法跨度内机械障碍，测试未声称能全部识别。未烧录、未覆盖原ELF、未提交。
