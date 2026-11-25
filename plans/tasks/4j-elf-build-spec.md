# 4j-yaw-m2006 本地提交与完整固件 ELF 构建

用户已授权在隔离分支本地提交并同步 `dart-fw` 编译完整固件 `DevC.elf`。

## 严格边界
- 严禁 `git push`、开 PR 或烧录固件（禁止运行 openocd 烧录）。
- 严禁覆盖或回退 `dart-fw` 既有保留改动：
  - `dart-fw/Core/Src/main.c`（USART1=9600）
  - `dart-fw/User/RobotConfig/artisans_dart.yaml`（`tension_uart: usart1`、`yaw_motor` CAN2 / `feedback_id: 514`）
  - `dart-fw/build-4g-final.log`
  - `dart-fw/Modules/QDU-Robomaster/Referee` 的 3 处本地补丁

## 执行步骤
1. **隔离分支本地提交**：
   - 在 `C:/Users/Lenovo/orca/workspaces/DartLauncher/auto-4j-yaw-m2006`（分支 `auto/4j-yaw-m2006`），仅暂存并提交 8 个授权文件（`README.md`、`src/dart_params.hpp`、`src/motor_axis.hpp`、`src/motor_axis.cpp`、`src/hw/launcher_hw.hpp`、`src/hw/launcher_hw.cpp`、`src/launcher_io.hpp`、`test/host_test.cpp`）。
   - 不提交 `build-host/` 等未跟踪文件；记录新 commit SHA。
2. **同步到 `dart-fw` 并构建完整 ELF**：
   - 先检查 `C:/Users/Lenovo/Desktop/xrobot/dart-fw` 当前 `git status` 与现有 `build/debug/DevC.elf`（若存在且未备份，可备份为 `build/DevC-before-4j.elf`）。
   - 将隔离工作区中这 8 个授权文件同步到 `C:/Users/Lenovo/Desktop/xrobot/dart-fw/Modules/local/DartLauncher/` 对应路径（保持 LF/原有行尾，不改动其他模块）。
   - 在 `C:/Users/Lenovo/Desktop/xrobot/dart-fw` 执行 `cmake --build --preset debug`（若未配置先 `cmake --preset debug`），确认完整编译与链接 0 错误。
3. **核验与报告**：
   - 运行 `arm-none-eabi-size` 检查 `build/debug/DevC.elf` 的 `.text`/`.data`/`.bss`，计算 `DevC.elf` 的 SHA-256 与文件大小。
   - 将 commit SHA、`dart-fw` `git status --short`、`DevC.elf` 路径/大小/SHA-256/`size` 输出写入 `C:/Users/Lenovo/Desktop/xrobot/plans/tasks/4j-yaw-elf-build-r1.md`。
   - 完成后通过 `orca orchestration send --type worker_done --outcome succeeded` 汇报协调器。
