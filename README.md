# RoboMaster 飞镖发射系统（feibiao）

本仓库整合飞镖发射架下位机固件工程、仿真模型、日常开发计划、项目交接总结及问题排查日志。

## 目录索引

- **`dart-fw/`**：STM32F4 下位机工程（基于 LibXR / XRobot），包含 `Modules/local/DartLauncher`（含 4j yaw 调参及 4k 手动 `R=M, L=U` 联动 3508 滑块归零最新源码）、`Modules/QDU-Robomaster/Referee` 本地补丁、`Core/`、`User/` 等。
- **`xrobot-local/`**：本地 XRobot 模块仓库备份（`DartLauncher/`、`ATSRuntime/`）。
- **`simulation/`**：MuJoCo 飞镖发射与装填仿真模型及控制脚本（`dart_sim/`、`dart.py`、`test_dart_sim.py`、`test_dart_console.py`）。
- **`plans/`**：日常开发计划、各阶段任务方案（`plans/tasks/2h-*` 至 `4l-*`）、规则与协议文档及诊断数据（`plans/tasks/diagnostics/`）。
- **`handoff.md`**：项目交接与阶段总览文档（当前状态、已验证参数、固件版本与待办事项）。
- **问题触发与日志分析**：
  - `飞镖日志汇总_21-05至21-29.md`
  - `DartLauncher_arm_filtered.txt`、`DartLauncher_arm_release_012751-013022.md`、`DartLauncher_calib_0401-0403.md`、`DartLauncher_release_015526-015551.md`、`DartLauncher_right_release_013745-013835.md`、`DartLauncher_standby_001126-001145.md`
- **`SYNC_MANIFEST.md`**：本地多仓库同步基线与排除项说明。
- **远端既有文档**：`下位机开发协议文档.md`、`遥控器控制左中右下控制皮筋（未进行高强度测试）/`。
