# 同步清单（SYNC_MANIFEST）

- **同步日期**：2026-11-25
- **任务号**：4l
- **来源工作区**：`C:\Users\Lenovo\Desktop\xrobot`（未修改原工作区任何文件或 `.git`）
- **同步策略**：去除嵌套 `.git`（含目录及 `LibXR/.git` 文件指针），以普通目录形式完整保留当前源码与未提交改动，避免 GitHub 出现不可访问的 `160000` gitlink。

## 1. 保留的远端既有内容
- `.gitignore`（增量追加构建与缓存排除规则）
- `下位机开发协议文档.md`
- `遥控器控制左中右下控制皮筋（未进行高强度测试）/`
- `simulation/`（保留远端既有文件并同步本地新增 `dart_sim/arm_force.py`）

## 2. 排除目录与文件
- 构建产物与缓存：`dart-fw/build/`、`**/build-host/`、`build-*.log`、`*.elf/*.bin/*.hex/*.map/*.a/*.o`、`**/__pycache__/`、`LibXR/lib/doxygen-awesome-css/docs/`
- 环境与工具目录：`.venv/`、`graphify-out/`、`.backup/`、`.vscode/`、`**/.settings/`、`.agent/`、`.claude/`、`.codex/`、`.workbuddy/`、`%SystemDrive%/`、`rules/`、`四发完整流程 - 副本/`、`.tmp_gpt.json`
- 非飞镖相关笔记：`campus-50-launch.md`、`workbuddy-case.md`、`codex-report.md`、`plans/codexplusplus-relay-gpt61-gemini-compatibility.md`

## 3. 原工作区嵌套 Git 仓库快照（共 30 个）

| 相对路径 | 基线 HEAD | 未提交改动数 (dirty) | 备注 |
|---|---|---|---|
| `dart-fw` | `fa0838b` | 3 | 含 `Core/Src/main.c`、`User/RobotConfig/artisans_dart.yaml` 本地改动 |
| `dart-fw/Middlewares/Third_Party/LibXR` | `5161373d` | 0 | 原为 submodule 文件指针 |
| `dart-fw/Modules/local/DartLauncher` | `e17b490` | 11 | 含 4j yaw 调参及 4k 联动回零最新源码 |
| `dart-fw/Modules/QDU-Robomaster/Referee` | `79f8e5c` | 3 | 含 3 处本地补丁 |
| `dart-fw/Modules/QDU-Robomaster/CameraSync` | `6cb451d` | 0 | |
| `dart-fw/Modules/QDU-Robomaster/Chassis` | `32761d0` | 0 | |
| `dart-fw/Modules/QDU-Robomaster/CMD` | `37b7700` | 0 | |
| `dart-fw/Modules/QDU-Robomaster/Dart` | `249326b` | 0 | |
| `dart-fw/Modules/QDU-Robomaster/DMMotor` | `7f1dc12` | 0 | |
| `dart-fw/Modules/QDU-Robomaster/DR16` | `79c22de` | 0 | |
| `dart-fw/Modules/QDU-Robomaster/EventBinder` | `3d4adde` | 0 | |
| `dart-fw/Modules/QDU-Robomaster/Gimbal` | `502a9db` | 0 | |
| `dart-fw/Modules/QDU-Robomaster/HeroLauncher` | `254179d` | 0 | |
| `dart-fw/Modules/QDU-Robomaster/HostData` | `7fb5d78` | 0 | |
| `dart-fw/Modules/QDU-Robomaster/InfantryLauncher` | `857d188` | 0 | |
| `dart-fw/Modules/QDU-Robomaster/MiniGimbal` | `608447c` | 0 | |
| `dart-fw/Modules/QDU-Robomaster/Motor` | `b782749` | 0 | |
| `dart-fw/Modules/QDU-Robomaster/PowerControl` | `4097e1f` | 0 | |
| `dart-fw/Modules/QDU-Robomaster/RMMotor` | `e9d5fa3` | 0 | |
| `dart-fw/Modules/QDU-Robomaster/SentryProtocol` | `81b3a8c` | 0 | |
| `dart-fw/Modules/QDU-Robomaster/SuperPower` | `aaa8db0` | 0 | |
| `dart-fw/Modules/QDU-Robomaster/VT13` | `7927154` | 0 | |
| `dart-fw/Modules/xrobot-org/BlinkLED` | `0ca1620` | 0 | |
| `dart-fw/Modules/xrobot-org/BMI088` | `e4e64d7` | 0 | |
| `dart-fw/Modules/xrobot-org/BuzzerAlarm` | `a7c17cf` | 0 | |
| `dart-fw/Modules/xrobot-org/MadgwickAHRS` | `e98a223` | 0 | |
| `dart-fw/Modules/xrobot-org/SharedTopic` | `6005712` | 0 | |
| `dart-fw/Modules/xrobot-org/SharedTopicClient` | `157d8bc` | 0 | |
| `xrobot-local/ATSRuntime` | `a339f30` | 0 | |
| `xrobot-local/DartLauncher` | `e17b490` | 0 | 4j 之前基线，待 4k 实车验证后回并 |
