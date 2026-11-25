# 任务 4c：测试阶段两发分别触发

## 目标

第一发由操作手触发，随后自动换弹；第二发装填完成后等待操作手再次触发。比赛阶段和自动单发规则未改；比赛前须按实际装载状态调整当前测试库存。

现场起始库存由用户确认：发射座一发、左镖座一发；磁铁和右镖座为空。

## 范围

检查 `xrobot-local/DartLauncher/` 的库存、流程、遥控适配、主机测试和说明；固件模块同步后编译，更新根目录交接。

## 计划

- [x] 核对两个仓库状态及现有触发、库存路径；主机基线通过 30517 项。
- [x] 为两次触发之间的自动装填和等待添加主机回归；现有固件未等待第二次触发，测试按预期失败。
- [x] 按现场起始库存调整测试流程，最小修改触发和装填逻辑。
- [x] 运行主机回归、固件编译及变更核对，在当前分支提交并更新交接。

## 验收

第一发触发后仅发射一次，自动装填第二发并等待；等待期间不发射，第二次操作手触发后仅发射第二发。保留既有锁止、停机和磁铁安全规则，现场验证分开记录。

## 实施与验证（2026-11-15）

- 起始库存重设为发射座、左镖座各一发；自动流程先预取左侧镖，再发射第一发，自动装填第二发并停在 `等待手动发射`；再次中→下触发第二发。装填期间提前触发不会保留。
- 基线 `PASS: 30517 checks`；新增等待测试后旧代码报 `FAIL: second dart is loaded and waits for another manual trigger`。修改后源码及同步模块均为 `PASS: 30536 checks`，覆盖长时间等待、断力恢复不发射、两次拨杆边沿与无效遥控输入。
- 从固件 `compile_commands.json` 使用实际 ARM 编译参数，对库存、流程及硬件适配三个改动的 `.cpp` 独立编译通过；纯逻辑依赖检查通过。
- `xrobot module add 'local/DartLauncher@9174faf'` 最初报 `already requested`；移除旧请求后重新添加并执行 `xrobot setup --update local/DartLauncher`，成功锁定 `9174faf5f8a9888a1e48c78a71b01486f7e1e54f`。工具提示已安装 XRobot 1.0.0 与固定 1.0.1 不同。
- 同步模块首次主机编译因模块目录缺少 `build-host` 而报 `cannot open output file ... No such file or directory`；改用源码仓库已有的输出目录后通过。
- `python .../build-cmake/scripts/cmake_builder.py --source ... --list-presets --no-save-profile` 报 `ModuleNotFoundError: No module named 'tool_config'`；改用 `cmake --build --preset debug` 完整编译通过，只有既有 `enable_language` 和 Eigen `CMP0146` 警告。ELF 内核对到 `等待手动发射`、左侧 +2.0° 与发射步骤。
- ELF：`dart-fw/build/debug/DevC.elf`，SHA-256 `B17D71A9F60AEDEF36059690298C3329053E7CCAC391B5B2F3629E67778762A7`。4b 备份为 `dart-fw/build/DevC-c018dd6-before-4c.elf`，SHA-256 `04B2B06E5480AB955382A514C8ABA6337C445ADCD6A87699E0B7775848167B6E`。
- 源码提交 `9174faf`（`Pause between manually triggered test shots`）；固件提交 `f8e4f3a`（`锁定两发手动触发测试模块`）。本轮未烧录；现场待验证预取、每发触发、自动装填、落点与继电器时序。
- 2026-11-15 用户反馈“已经烧录”；待回零并检查串口状态，再验证两发流程。
