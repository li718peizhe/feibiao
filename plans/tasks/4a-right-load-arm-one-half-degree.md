# 任务 4a：右侧取镖后以 +1.5° 放镖

## 目标、范围与验证

用户反馈右侧取镖后放镖偏右，要求右侧放镖改为 +1.5°，左侧保持 +2.0°。沿用完整装填、2150 µs 下放及 0.6 s 等待。取镖来源在吸住时记录，交接库存后用于选择放镖目标，取消重做仍沿用该来源。

文件：DartLauncher 的 `src/launch_sequence.hpp/.cpp`、`src/dart_inventory.hpp/.cpp`、`test/host_test.cpp`、`README.md`；固件 `Modules/modules.yaml`、工具生成的 `xrobot.lock`；本记录和交接。保留既有 `src/dart_params.hpp` 右取镖 46.35° 工作区修改，不纳入提交。不读 `plans/dart-fw-libxr.md`，不运行 graphify，不调用子代理，不烧录、不 push。

## 计划

1. 已完成：核对取镖来源与库存交接，更新左右放镖回归并复现来源丢失，最小修改后主机回归通过 30517 项。
2. 已完成：源码提交 `5021794`，通过 xrobot setup 同步模块，模块回归 30517 项及完整 CMake debug 编译通过。
3. 已完成：烧录、复位及 Flash 只读逐字节核验通过，固件提交 `a0a0db0`，交接更新；实际放镖落点待现场验证。

## 基线

源码分支 `codex/apply-load-position-calibration`、HEAD `dd79323`，仅 `src/dart_params.hpp` 存在既有右取镖角修改；固件分支 `dart-launcher`、HEAD `15656d0`，工作区和模块副本干净。当前装填共用 +2.0°，独立取镖交接库存时 `ResetHeld()` 将来源清为 0，需要保存来源。

用户随后要求编译并烧录，烧录结果及现场状态核对在下文记录。

## 实现与验证

- 吸住镖时将 Step 的 `side` 保存为 `fetch_held_side_`，独立装填交接至 `ResetHeld(side)`；完整取镖完成后虽已轮换下一侧，仍使用实际持镖来源。装填按右侧 +1.5°、左侧及开机手放来源 0 沿用 +2.0°。取消后重做保留库存来源。
- 测试先于实现更新，旧实现失败：`FAIL: accepted loading transfers the fetched dart into a single held inventory`。最终源码与同步模块均 `PASS: 30517 checks`，覆盖左右三种锁止初态、左右取消重做、释放中断及四发左右装填目标；2150 µs 与下放后 0.6 s 等待沿用。
- 主机命令：源码目录 `$src = (Get-ChildItem -LiteralPath src -Filter *.cpp).FullName; g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test\host_test.cpp $src -o build-host\4a-host-test.exe` 后执行该 EXE；模块使用 `Modules/local/DartLauncher` 同样编译，输出 `build/4a-module-host-test.exe`。
- `xrobot setup --update local/DartLauncher` 退出 0，同步模块 HEAD `5021794` 且干净；提示工具安装版本 1.0.0 与配置 1.0.1 不同，未安装或升级软件。
- `cmake --build --preset debug` 退出 0，日志 `dart-fw/build/4a-right-load-build.log`；RAM 32560 B、CCMRAM 64 KB、FLASH 219380 B。仅既有 `enable_language` 配置警告，无模块编译警告。
- ELF：`dart-fw/build/debug/DevC.elf`，SHA-256 `AB42475473E7ECE233DD7B733F2322DB0B11EA5657EB8634C2D39106FD81EC5C`。
- 烧录前通过 COM46 抓取状态：`mode=0`、`latch=2`、左右滑块 +340.66/+341.11 mm、推杆 1325 µs；日志 `build/4a-before-flash-serial.log`。
- 使用 flash-openocd 脚本，先将现有 OpenOCD bin 临时加入本进程 PATH，指定 `--artifact build/debug/DevC.elf --interface cmsis-dap --target target/stm32f4x.cfg --no-detect`；工具实际执行 OpenOCD 的 `init; program C:/Users/Lenovo/Desktop/xrobot/dart-fw/build/debug/DevC.elf verify reset; exit`，报告烧录、校验、复位成功，随后保存配置异常（见下节）。
- `python build/4a-verify-flash.py` 退出 0，以 `init; dump_image ...; shutdown` 读取运行中 Flash，不停核、不复位；两段 ELF Flash 共 219380 字节逐字节相同，输出 `PASS: 2 ELF flash segments, 219380 bytes match board readback`。
- 烧录后 COM46 输出 `mode=0 fault=遥控器离线 home=0 latch=0`、`step=空闲`、推杆 1325 µs；日志 `build/4a-after-flash-serial.log`。现场先重新回零，再验证右侧 +1.5° 落点、左侧 +2.0°，以及既有下推、继电器、收杆锁止回位。

## 工具失败及处理

- `python C:/Users/Lenovo/.agents/skills/build-cmake/scripts/cmake_builder.py --source ../../dart-fw --list-presets --no-save-profile` 失败：`ModuleNotFoundError: No module named 'tool_config'`；改用已配置的 `cmake --list-presets` 和 `cmake --build --preset debug`，均成功，未安装依赖。
- 新增测试最初编译失败：`-Werror=range-loop-construct`，将循环变量改为 `const auto &`；随后连续两次取镖测试因重置模拟时间失败，第二次 RunSequence 改为从 500 s 开始，最终全套通过。
- 初次 OpenOCD 探测使用 `--openocd <路径>`（该工具将其解析为 `--openocd-command`）未发现可执行文件；将已有 bin 加入进程 PATH 后 `--detect` 成功识别 cmsis-dap。
- 烧录脚本写入校验复位成功后退出 1：`AttributeError: 'Namespace' object has no attribute 'no_save_profile'`，日志 `build/4a-flash.log`；未保存工具 profile。独立 Flash 读回比对通过。
- 额外 `init; verify_image ...; shutdown` 命令虽退出 0，输出 `Warn : target not halted`、`Error: error executing cortex_m crc algorithm`，未作为验证成功；改为上述不停核 `dump_image` 逐字节比对并通过。
- 烧录后串口命令 `--duration 3` 抓到一组状态后未及时退出，发送中断并确认 COM46 关闭；工具因日志含 `遥控器离线` 返回 1。未操作遥控器，未执行现场装填。
- 探索时读取不存在的 `scripts/build.py`、`build-host/CMakeCache.txt` 失败，使用文档中实际的 `cmake_builder.py` 和 README 的主机编译命令；PowerShell 下直接传给 rg 的 `3u*`/`3v*` 文件参数报 `os error 123`，未用此结果作为证据。

## Git 与交接

源码提交 `50217946ac72625dbdbdf0b40aefe39663e17cc3`，`按取镖来源将右侧放镖角设为1.5度`；原分支不变，仅保留既有 `src/dart_params.hpp` 46.35° 修改。固件提交 `a0a0db01643362538521b0dde17e75635a14301a`，`锁定右侧1.5度放镖标定`；固件和同步模块干净，Referee 既有三处修改保留。未 push，根目录记录直接保存。
