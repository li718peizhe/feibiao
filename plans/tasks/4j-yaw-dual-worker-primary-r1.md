# 4j yaw primary r1：整合与验证

## 冻结身份及来源

- 主交付工作区：`C:/Users/Lenovo/orca/workspaces/DartLauncher/auto-4j-yaw-m2006`；实际分支 `auto/4j-yaw-m2006`。
- 基线及实际HEAD：`e17b490e8ffbefedc08c96d8d74396ba257487de`，没有新提交。
- 归档简化实现后完整替换为冻结候选；候选来源为同级 `auto-4j-yaw-m2006-2`，未写该工作区，未执行其脚本。
- 简化实现保存在主路径 `build-host/primary-archive/`：完整binary diff及5个原文件备份；diff SHA256 `a3a9b2de0840403a8047a24fe1f44580f5ec528abe0418f4d617fd11fe270362`，备份逐项SHA见 `build-host/primary-freeze.json`。
- 最终主路径 `build-host/primary-final.diff` SHA256：`4308cd4673d5ca6a23bf4c245ca66fd195acdaae4b883a24f9cca53de4002d49`。
- 8授权文件通过小块apply_patch替换，逐个SHA256与冻结候选一致：README.md、src/dart_params.hpp、src/motor_axis.hpp、src/motor_axis.cpp、src/hw/launcher_hw.hpp、src/hw/launcher_hw.cpp、src/launcher_io.hpp、test/host_test.cpp；逐项SHA存于主路径 `build-host/primary-freeze.json`。

## 实现及动态覆盖

双端模式默认关闭，yaw启用该模式；原始counts测两端算术均值、两次退离后回中稳定才Homed，学习动态边界；接触要求命令电流/转速/位置窗口及新反馈。Stop/无力取消未完成流程，离线使学习失效，重连不自恢复。Manual、SetYaw、Aimed采用输出轴rad，日志rev及快照阶段/故障/span/cmd/act/rpm，StopMotors保持yaw当前位置，未改变pitch/滑块/摆臂参数或4i门控。

动态测试包括中间/两端/非对称/负多圈起点、RotorCounter跨圈和10ms反馈、重测及旧offset/边界、回中速度/稳定计时、零rpm但位置移动、无新反馈、短/超长/无端超程、退离和回中阻塞、模拟总超时、五阶段Stop/无力/离线、ClearFault重测、动态边界各接口、释放保持、真实yaw轴反馈驱动MatchFlow延迟滑块启动，以及原普通轴回归。合法跨度内障碍不能保证识别。

## 主路径重新验证（全部退出0）

```powershell
$env:PATH='C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;'+$env:PATH
$src=(Get-ChildItem -LiteralPath src -Filter '*.cpp').FullName
g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test/host_test.cpp $src -o build-host/host_primary_4j_verify_r2.exe
.\build-host\host_primary_4j_verify_r2.exe
g++ -std=c++17 -Wall -Wextra -Werror -O1 -Itest/hx711_stubs -Isrc/hw test/hx711_port_test.cpp src/hw/hx711_port.cpp -o build-host/hx711_primary_4j_verify_r2.exe
.\build-host\hx711_primary_4j_verify_r2.exe
python build-host/arm_primary_precheck.py
python build-host/freeze_primary.py
git diff --check
```

输出：`PASS: 2335266 checks`、`PASS: 2193 HX711 checks`；ARM 25个DartLauncher编译单元全部退出0。顶层cpp/hpp新增HAL/LibXR/FreeRTOS依赖扫描为空。

ARM脚本经阅读审查后在主路径重新编写，读取原固件 `C:/Users/Lenovo/Desktop/xrobot/dart-fw/build/debug/compile_commands.json`，将模块输入/include前缀替换为主路径，移除-o及依赖输出参数，保留编译选项，输出独立 `build-host/arm_primary_*.o`。每项真实命令、主路径输入、exit、stdout/stderr、输入及对象SHA保存在 `build-host/arm_primary_evidence.json`，其SHA256为 `5d58decd57b4db65c7f0ebb3057f68231693b7e8765a79b7dcff466551d41c50`；没有使用候选对象冒充。

## 历史失败与剩余门禁

先前简化版本仅旧回归通过；旧HX711输出文件Permission denied后换唯一输出名，本次成功。归档初次PowerShell Byte写入失败后使用原始git diff重定向成功；Get-FileHash不可用，最终用Python hashlib完成全部字节哈希断言。旧基线git apply因已有简化改动不匹配后停止使用，最终全部使用apply_patch实现；重复补丁产生上下文失败，已按实际文件核对消除，最终八文件完全一致。

本轮主路径动态host、HX711及真实新源码ARM编译预检通过，冻结等待独立reviewer。没有新完整ELF：候选报告指出正式CMake固定Modules目录，未为整合更改工程配置或提交同步模块，完整链接作为后续集成门禁；本轮不能宣称完整固件已构建。未烧录、未覆盖原ELF、未提交/推送；原工程及USART1/YAML/Referee补丁未写入。方向、848是否能带动、实测跨度及真实行程中点仍待现场确认。
