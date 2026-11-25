# 任务 4d：对齐自动流程与手动触发取镖

## 目标与范围

自动发射预取时取镖偏高，左下、右中、拨轮向上保持 2 秒触发的取镖正常。两条流程的左右推杆目标相同，自动流程吸持等待 0.15 s，手动取镖等待 0.5 s，且自动流程较晚关闭磁铁。统一取镖前的磁铁状态和吸持等待，保留现有左右位置、慢速抬起及库存规则。

允许修改 `xrobot-local/DartLauncher/src/launch_sequence.cpp`、`test/host_test.cpp`、`README.md`，同步模块版本，并更新本任务与根目录交接。禁止运行 graphify、烧录或推送。

## 计划

- [x] 检查仓库状态，比较两条取镖流程和实车记录。
- [x] 添加主机回归，旧流程报 `FAIL: automatic fetch waits as long as the manual fetch before lifting`。
- [x] 最小修改自动取镖；主机回归覆盖左右、预取和直接装填，以及既有完整流程。
- [x] 更新文档，同步模块，完成固件构建、提交及交接。

## 验收

左右取镖目标与手动流程一致；开始下探前磁铁断电；下探结束后至少等待 `kFetchGripTime` 再慢速抬起；库存转换、装填和发射步骤维持现有约定。实车检查取镖是否吸牢、无碰撞。

## 实施与验证（2026-11-15）

- 用户反馈 4c 自动流程取镖偏高、未取到镖；右拨杆中、左拨杆下、拨轮向上保持 2 秒取镖正常。两条路径左右推杆目标均为 2288 / 2395 µs；自动流程原在下探步骤后等待 0.15 s，手动流程等待 0.5 s，自动流程原在下探后才断电吸持。
- 自动取镖改为收杆与下探前切换磁铁断电；下探步骤到位条件满足后等待 `kFetchGripTime=0.5 s`，然后按原速慢抬。左右目标和库存交接未改。源码 README 的右侧摆臂角修正为实际编译值 46.35°。
- 源码基线 `PASS: 30536 checks`；新增等待测试在旧代码报 `FAIL: automatic fetch waits as long as the manual fetch before lifting`；修改后源码及同步模块均 `PASS: 30834 checks`，覆盖左右、预取及直接装填入口的磁铁、目标和等待，以及完整发射流程。
- `python C:\Users\Lenovo\.agents\skills\build-cmake\scripts\cmake_builder.py --source C:\Users\Lenovo\Desktop\XROBOT\dart-fw --list-presets --no-save-profile` 失败：`ModuleNotFoundError: No module named 'tool_config'`；改用完整的 `C:\Users\Lenovo\.codex\skills\embed-ai-tool-master\skills\build-cmake\scripts\cmake_builder.py` 列出预设并执行 debug 构建成功。同步模块后 `cmake --build --preset debug` 再次通过；`xrobot format --check` 通过。
- `xrobot setup --offline --update local/DartLauncher` 失败：`--update cannot be combined with --frozen or --offline`；随后 `xrobot setup --update local/DartLauncher` 成功，提示已安装 XRobot 1.0.0 与固定版本 1.0.1 不同。
- 新 ELF `dart-fw/build/debug/DevC.elf` SHA-256 `E98DE8C4516CF3836F66AF591E8CC0F530169B259FA6CB99832EA9236F273052`；原 4c 保存在 `dart-fw/build/DevC-f8e4f3a-before-4d.elf`，SHA-256 `B17D71A9F60AEDEF36059690298C3329053E7CCAC391B5B2F3629E67778762A7`。本次未烧录，现场需确认取镖吸牢、无碰撞。
- 源码提交 `322e39b`（`Align automatic fetch timing`）；固件提交 `2c76594`（`Lock aligned automatic fetch module`）；两个主仓库干净，Referee 模块保留原有修改。
