# 任务 4k A 版方案：手动 `R=M, L=U` 拨轮上 2 s 联动 3508 滑块归零

- **日期**：2026-11-24
- **作者**：主对话（A 版）
- **对应事项**：Orca item 6
- **目标**：当操作手在手动模式（`R=M`）选定瞄准组（`L=U`）并将拨轮往上推到底保持 2 s 时，除归零 `pitch` + `yaw` 外，一并触发 `3508` 滑块归零；原有 `R=M, L=M` 单独滑块归零与 `SliderMustHold` 安全规则保持不变。

---

## 1. 现状核对
1. `src/mode_logic.cpp:40-84`：
   - `R=M, L=U` 映射为 `out.group = ManualGroup::Aim`；
   - 拨轮 `wheel >= 0.95` 保持 `kHomeWheelTime`（2 s）触发 `out.home_request = true`。
2. `src/hw/launcher_hw.cpp:282, 300, 748-778`：
   - 手动回零（282 行）与自动模式 `auto_home_aim`（300 行）均调用 `StartHome(ManualGroup::Aim, now)`；
   - 目前 `case ManualGroup::Aim:` 仅启动 `yaw_.Axis().StartHome()` 与 `pitch_.Axis().StartHome()`，不启动 `slider_left_` / `slider_right_`；
   - 滑块回零仅在 `case ManualGroup::Slider:` 中执行，且先检查 `Safety::SliderMustHold(state_)`，若滑块正托持未锁住发射机构则拒绝回零并打印告警。
3. `src/hw/launcher_hw.cpp:795-810`：
   - `Manual()` 中已通过 `!Homing(slider_left_.Axis()) && !Homing(slider_right_.Axis())` 保护：只要滑块处于回零状态，手动摇杆点动不会覆盖回零状态机。

---

## 2. A 版设计方案

### 2.1 改动范围与触发边界
- **仅针对手动 `R=M, L=U` 触发**（不改变 `MatchFlow` 自动模式 `auto_home_aim` → `auto_home_slider` 的既有分步状态机与 `host_test.cpp` 已有时序）：
  - 在 `LauncherHardware::Update` 处理 `command_.home_request && !sequence_.Busy()` 时：
    - 当 `command_.group == ManualGroup::Aim` 时，除调用 `StartHome(ManualGroup::Aim, now)` 外，同时尝试启动滑块回零（或将滑块联动封装在 `StartHome` 的 `include_slider` 参数中，默认 `false`，仅手动 `command_.home_request` 传 `true`）。
- **安全边界（`SliderMustHold`）**：
  - 联动启动滑块前必须检查 `Safety::SliderMustHold(state_)`：
    - 若未托持，置 `stop_hold_ = false` 并调用 `slider_left_.Axis().StartHome()` 与 `slider_right_.Axis().StartHome()`；
    - 若正在托持未锁住发射机构，**拒绝启动滑块回零**并打印 `XR_LOG_WARN("滑块托着发射机构，跳过滑块回零")`，`pitch` 和 `yaw` 仍可正常回零。
- **未安装轴判断**：
  - 若 `command_.group == ManualGroup::Aim` 且 `kPitchInstalled || kYawInstalled || kSliderInstalled` 任一为真，即不应报未安装警告。
- **保留 `R=M, L=M` 拨轮上 2 s**：
  - 保留 `ManualGroup::Slider` 单独归零滑块的功能，方便只单独重归零滑块。

### 2.2 涉及文件
1. `dart-fw/Modules/local/DartLauncher/src/hw/launcher_hw.hpp` / `src/hw/launcher_hw.cpp`：实现 `ManualGroup::Aim` 手动回零联动滑块（带 `SliderMustHold` 保护），不改变 `auto_home_aim` 行为。
2. `dart-fw/Modules/local/DartLauncher/README.md`：更新遥控器操作说明中 `R=M, L=U` 拨轮上 2 s 的回零范围。
3. `dart-fw/Modules/local/DartLauncher/test/host_test.cpp`：如提取纯逻辑辅助判断或保持与主机测试一致，补充对应断言（若全部改动在 `hw` 适配层且已有 `MotorAxis`/`Safety` 覆盖，至少跑通全部既有主机回归）。

---

## 3. 验证与验收标准
1. 主机测试 `host_test` 全量通过（无失败、无跳过）。
2. ARM 编译与 `dart-fw` 完整 `debug` 编译生成 `DevC.elf` 通过，无新增警告。
3. 实车边界确认：
   - 未上膛 / 未托持时，`R=M, L=U` 拨轮上 2 s：`pitch`、`yaw`、`3508` 滑块三者同时开始回零并正常停稳；
   - 滑块托持未锁住机构时，`R=M, L=U` 拨轮上 2 s：滑块保持原地不回零、不走火；
   - 回零途中拨动摇杆或切无力，安全停机规则不变。
