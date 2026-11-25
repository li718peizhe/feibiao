# 任务 4k 实施方案（B 版）：手动 R=M, L=U 拨轮上 2 s 联动 3508 滑块归零

- 日期：2026-11-25
- 任务号：4k
- 状态：B 版草稿（独立方案，待对比合稿）
- 基线说明：基于当前 `dart-fw/Modules/local/DartLauncher` 工作区（含 4j yaw 双端回中与实车调参，未执行 shell 查 commit hash；注：`xrobot-local/DartLauncher` 当前仍停在 4j 合并前，见第 6 节仓库同步风险）

---

## 1. 需求与目标

在手动模式（右拨杆中 `R=M`）下，左拨杆上（`L=U`，瞄准组 `ManualGroup::Aim`）且拨轮正向推到底（`wheel >= 0.95`）保持 `kHomeWheelTime`（2 s）时：
1. 启动瞄准组（`pitch` + `yaw`，按装配开关）归零；
2. **同时**启动 `3508` 两侧滑块（`slider_left` + `slider_right`，按装配开关）归零；
3. 保留 `R=M, L=M`（滑块组）拨轮正向 2 s 单独归零滑块、`R=M, L=D`（装弹组）拨轮正向 2 s 取镖/装填的既有行为；
4. 严格维持所有安全门控（特别是 `Safety::SliderMustHold`）与自动模式 / 比赛流程（`MatchFlow`）既有分步时序与隔离性。

---

## 2. 设计选择与对比

### 2.1 方案对比

| 维度 | 备选 1：仅改 `LauncherHardware::StartHome(Aim)` | 备选 2：仅在 `LauncherHardware::Tick` 按 `group==Aim` 补调滑块 | **推荐方案（B 版）：纯逻辑 `ModeCommand` 拆分 `home_aim` / `home_slider` + 适配层分别分发** |
| --- | --- | --- | --- |
| 自动模式兼容性 | **破坏**：`MatchFlow::AutoHomeStart` 与 `NeedsHoming` 依赖 `StartHome(Aim)` 只回零瞄准轴 | 兼容：`auto_home_aim` 不走 `home_request` | **兼容**：`auto_home_*` 与手动 `home_*` 语义完全对称且解耦 |
| 主机单元测试覆盖 | 差：`launcher_hw.cpp` 依赖 LibXR，无法进 `host_test.cpp` | 差：联动规则藏在适配层，主机测试测不到 `L=U` 联动滑块 | **好**：`ModeLogic` 与 `MatchFlow` 纯逻辑可直接由 `host_test.cpp` 全覆盖验证 |
| 语义清晰度 | 差：函数名与实际职责不符 | 中：`home_request` 隐含组耦合 | **好**：`ModeCommand` 显式表达目标轴组，与 `auto_home_aim` / `auto_home_slider` 命名一致 |
| 改动量 | 小但引入严重回归风险 | 最小（1 处），但纯逻辑不可测 | 小且可控（共 4 个源码文件 + 1 个测试文件 + 1 个文档） |

### 2.2 核心设计结论

1. **不在 `StartHome(ManualGroup::Aim, now)` 内耦合滑块回零**：`StartHome` 保持单组纯职责，确保 `MatchFlow::AutoHomeStart`（顺序先瞄准后滑块，且已回零轴不重回）和 `MatchFlow::NeedsHoming` 零回归。
2. **在纯逻辑层 `ModeCommand` 中将单字段 `home_request` 拆分为显式双字段 `home_aim`、`home_slider`**：
   - `R=M, L=U`（`ManualGroup::Aim`）拨轮正向保持 2 s：`out.home_aim = true; out.home_slider = true;`
   - `R=M, L=M`（`ManualGroup::Slider`）拨轮正向保持 2 s：`out.home_aim = false; out.home_slider = true;`
   - `R=M, L=D`（`ManualGroup::Loading`）拨轮正向保持 2 s：`out.home_aim = false; out.home_slider = false; out.fetch_request = true;`
   - `MatchFlow::Takeover(ModeCommand &c)` 同步清零 `c.home_aim = c.home_slider = false;`。
3. **适配层 `LauncherHardware::Tick()` 按 `home_aim` / `home_slider` 独立调用 `StartHome`**：
   - 两条轴组互不阻断：即使滑块因 `SliderMustHold` 被拒，瞄准组仍正常启动回零；
   - 对“同时请求 `home_aim && home_slider`”的场景做装配开关聚合判断，避免某一侧未安装（例如 `kSliderInstalled == false` 或 `!kPitchInstalled && !kYawInstalled`）时误打 `"该机构未安装"` 警告；仅当三轴全部未安装时才调用 `WarnNotInstalled(now)`。

---

## 3. 安全边界与兼容性分析

### 3.1 `Safety::SliderMustHold` 边界（最高优先级）
- 判定条件（`src/safety.cpp:81-84`）：`slider_homed && latch_state != LatchState::Latched && std::isfinite(slider) && slider < kSliderPark - 0.01`。
- 当滑块正托着未确认锁住的发射机构时，操作手触发 `R=M, L=U` 拨轮上 2 s：
  1. `StartHome(ManualGroup::Aim, now)` 正常启动 `pitch` / `yaw` 回零，**不修改 `stop_hold_`**；
  2. `StartHome(ManualGroup::Slider, now)` 命中 `Safety::SliderMustHold(state_)`，打印 `XR_LOG_WARN("滑块托着发射机构，拒绝回零")` 并返回 `BlockedBySliderHold`，**不清除 `stop_hold_`**、不调用 `slider_*.Axis().StartHome()`；
  3. 本拍及后续 `Manual()` 中（`L=U` 下 `command.slider_rate == 0`），`speed == 0 && Safety::SliderMustHold(state_)` 继续维持 `stop_hold_ = true`，滑块稳态托持，杜绝低电流回零被皮筋弹起。

### 3.2 并行回零交互与运行中行为
- **总线与电流**：CAN1 `0x200` 帧同时承载 `slider_left`（0x202）、`slider_right`（0x201）、`pitch`（0x203）；CAN2 承载 `yaw`（0x202）。回零期间均为低电流限幅（滑块 30% ≈ 3098、pitch 50% ≈ 869、yaw 848），且 `MatchFlow::NeedsHoming()` 在比赛中本就同拍并行发起 `auto_home_slider` 与 `auto_home_aim`，底层 `MotorPort` / `SliderPair` 与电源链路均已支持并行回零。
- **手动循环保护**：`LauncherHardware::Manual()`（`launcher_hw.cpp:795`）已有 `!Homing(slider_left_.Axis()) && !Homing(slider_right_.Axis())` 保护，滑块回零期间（含一侧先 `Homed`、另一侧仍在 `Retreating` 的过渡拍）不会被 `Jog(0)` 覆盖；回零完成后停在 `kSliderTop - kHomeRetreat`（`0.367 m`），高于 `kSliderPark - 0.01`（`0.356 m`），不误触发 `SliderMustHold`。
- **已回零状态重触发**：
  - `latch_state == LatchState::Unknown`（刚上电未回零）：`SliderMustHold` 为假，正常向上回零；
  - `latch_state == LatchState::Latched`（已锁止）或发射机构已停顶放开：`SliderMustHold` 为假，允许重回零；
  - 唯一拒绝条件是 `SliderMustHold` 为真，与 `L=M` 单组回零一致。
- **中途取消 / 切无力 / 故障**：
  - 切无力（`R=D`）或切自动（`R=U`）触发 `leave_manual = true` → `StopMotors(Safety::SliderMustHold(state_))`，同时中止未完成的瞄准和滑块回零；
  - 任一轴回零失败（`HomeState::Failed`，如 `pitch` 走过 `kPitchHomeSeekLimit` 或 `yaw` 跨度异常）会在 `RefreshState()` 锁存故障，触发全局无力并按 `SliderMustHold` 保护滑块。

### 3.3 自动模式与裁判系统兼容性
- **`!competition` 或 `Stage::NotStarted`（`R=U, L: M→D` 自动归零准备）**：走 `MatchFlow::AutoHomeStart`，依然使用 `auto_home_aim` → `auto_home_slider` 顺序推进，不调用手动 `home_aim` / `home_slider`，完全不受影响。
- **`Stage::InMatch` / `Stage::Settlement`**：`MatchFlow::Takeover()` 清零 `home_aim` 与 `home_slider`，遥控器拨轮仍完全被屏蔽；兜底回零继续走 `NeedsHoming()` 的 `auto_home_*`。
- **`Stage::Preparing` / `SelfCheck` / `Countdown` / `Unknown`**：`MatchFlow::RefuseLaunch()` 仅拦截上膛/发射，不拦截 `home_aim` / `home_slider`，准备阶段可用 `R=M, L=U` 一次完成瞄准+滑块回零。

---

## 4. 分阶段实施方案与涉及文件

> 执行注意：先确认改动落点仓库（见第 6 节），以下路径以 `DartLauncher` 模块根目录为基准。

### 阶段 1：纯逻辑接口与状态映射（`src/mode_logic.*`、`src/match_flow.cpp`）

1. **`src/mode_logic.hpp`**
   - 在 `ModeCommand`（约 L37）将 `bool home_request = false;` 替换为：
     ```cpp
     bool home_aim = false, home_slider = false;
     ```
   - 同步更新结构体上方注释，明确 `home_aim` / `home_slider` 均为单拍事件。

2. **`src/mode_logic.cpp`**
   - 在 `ModeLogic::Update()`（约 L76-84）拨轮正向保持满 `kHomeWheelTime` 分支中：
     ```cpp
     out.home_aim = direction > 0 && out.group == ManualGroup::Aim;
     out.home_slider = direction > 0 &&
         (out.group == ManualGroup::Aim || out.group == ManualGroup::Slider);
     out.fetch_request = direction > 0 && out.group == ManualGroup::Loading;
     ```
   - 保持每次保持只触发一次（`wheel_sent_ = true`）、换向/回中/换组重置计时的既有逻辑不变。

3. **`src/match_flow.cpp`**
   - 在 `MatchFlow::Takeover(ModeCommand &c)`（约 L43）将 `c.home_request` 替换为 `c.home_aim = c.home_slider = false;`。

### 阶段 2：硬件适配层分发与未安装告警保护（`src/hw/launcher_hw.cpp`）

4. **`src/hw/launcher_hw.cpp`**
   - 在 `LauncherHardware::Tick()`（约 L282）替换原 `if (command_.home_request && !sequence_.Busy()) { StartHome(command_.group, now); }`，改为：
     ```cpp
     if (!sequence_.Busy() && (command_.home_aim || command_.home_slider)) {
         if (command_.home_aim && command_.home_slider) {
             bool any_installed = false;
             if constexpr (kPitchInstalled || kYawInstalled) {
                 any_installed = true;
                 StartHome(ManualGroup::Aim, now);
             }
             if constexpr (kSliderInstalled) {
                 any_installed = true;
                 StartHome(ManualGroup::Slider, now);
             }
             if (!any_installed) { WarnNotInstalled(now); }
         } else if (command_.home_aim) {
             StartHome(ManualGroup::Aim, now);
         } else {
             StartHome(ManualGroup::Slider, now);
         }
     }
     ```
   - **不修改** `LauncherHardware::StartHome(ManualGroup group, double now)` 本身，保持 `auto_home_aim` / `auto_home_slider` 调用语义不变。
   - 如需增强现场可观测性（可选），可在 `StartHome` 进入 `Aim` / `Slider` 时补充 `XR_LOG_INFO`，注意避免在 `auto_home_*` 路径重复打双行日志。

### 阶段 3：主机单元测试与文档更新（`test/host_test.cpp`、`README.md`）

5. **`test/host_test.cpp`**
   - 更新 `TestWheelAndManualCancel()`（约 L2077-2142）：
     - 验证 `R=M, L=U` 正向满 2 s 仅单拍同时置位 `home_aim == true && home_slider == true`；
     - 验证 `R=M, L=M` 正向满 2 s 仅单拍置位 `!home_aim && home_slider`；
     - 验证 `R=M, L=D` 正向满 2 s 仅单拍置位 `fetch_request && !home_aim && !home_slider`；
     - 验证回中重触发、反向重置计时、无力档忽略拨轮均覆盖 `home_aim` 与 `home_slider`。
   - 更新 `TestMatchRefuseLaunch()`（约 L3615-3622）：
     - 将 `command.home_request = true` 改为 `command.home_aim = command.home_slider = true`，断言准备/自检/倒计时/未知阶段保留两项回零请求。
   - 更新 `TestMatchTakeover()`（约 L3657-3674）：
     - 验证 `InMatch` 下 `L=U` 与 `L=M` 拨轮正向持续保持时，`!command.home_aim && !command.home_slider` 始终成立。
   - 保持既有 `AutoHomeStart` 与 `NeedsHoming` 全部回归用例不变并全部通过。

6. **`README.md`**
   - 更新“遥控与安全”第 133 行：将“瞄准组回零 yaw/pitch，滑块组回零两侧滑块”更新为“瞄准组同时回零 yaw/pitch 与两侧滑块（滑块托持未锁住发射机构时仅拒绝滑块并保持，瞄准组照常回零），滑块组单独回零两侧滑块”。

---

## 5. 验证命令与验收标准

### 5.1 主机单元测试（在对应 `DartLauncher` 模块目录下执行）
```powershell
$env:PATH = "C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;" + $env:PATH
$src = (Get-ChildItem src\*.cpp).FullName
g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test\host_test.cpp $src -o build-host\host_test.exe
.\build-host\host_test.exe
```

### 5.2 固件构建（在 `C:\Users\Lenovo\Desktop\xrobot\dart-fw` 下执行）
```powershell
cmake --build --preset debug
```
（若先提交到 `xrobot-local/DartLauncher` 再同步，则在构建前按模块流程执行 `xrobot setup --update local/DartLauncher` 与 `xrobot gen -c User/RobotConfig/artisans_dart.yaml`。）

### 5.3 验收标准
1. **编译与单测**：`host_test.exe` 在 `-Wall -Wextra -Werror` 下零警告编译并通过全部检查；`DevC.elf` 构建零警告零错误。
2. **台架/实车功能验收（无镖、发射方向无人、先将 pitch 置于离上端 1~2 cm 内）**：
   - 上电解除故障后，直接切 `R=M, L=U`，拨轮往上推到底保持 2 s：
     - 串口日志可见 `pitch`、`yaw`、两侧 `3508 滑块` 同时进入回零；
     - 全部完成后 `home=1/1/1/1/0`、`sl≈363~367mm`、`step=空闲`；
     - 随后切 `L=M` 拨轮下 2 s 可直接上膛，不再报“轴未全部回零，拒绝上膛”。
   - `R=M, L=M` 拨轮上 2 s：仅滑块回零，不触发 `pitch`/`yaw` 回零。
   - **安全回归（`SliderMustHold`）**：在滑块已回零、未锁止且低于 `356 mm` 托持发射机构的状态下，切 `R=M, L=U` 拨轮上 2 s：
     - 串口打印 `滑块托着发射机构，拒绝回零`，滑块保持原位不松劲；
     - `pitch`/`yaw` 正常完成回零。
   - **急停/取消回归**：并行回零途中切 `R=D`（无力），四轴立即停机且不异常抖动。
   - **自动模式回归**：自动模式 `R=U, L: M→D` 既有顺序归零逻辑与行为无变化。

---

## 6. 风险点与需人工确认事项

1. **双目录分叉风险（需人工确认）**：当前 `dart-fw/Modules/local/DartLauncher` 包含 4j（`kYawInstalled=true`、双端回中及最新实车参数），而 `xrobot-local/DartLauncher` 尚未合入 4j。若直接在 `xrobot-local` 改 4k 并跑 `xrobot setup --update`，会覆盖 4j 改动。建议明确本次是先改 `dart-fw/Modules/local/DartLauncher` 验证再统一合回 `xrobot-local`，还是先将 4j 同步到 `xrobot-local` 后再做 4k。
2. **机械线束干涉风险（需实车首次观察）**：此前未在实车上验证过“yaw 43.375 圈全行程左右扫动 + pitch 顶端回零 + 滑块回顶”同拍并行动作，首次台架测试需目视确认线束与结构无刮蹭、无拉扯；若存在干涉，应改回先瞄准后滑块的串行编排。
3. **语义微调确认**：当滑块处于 `SliderMustHold` 时，本方案采取“拒绝滑块并保持、瞄准组照常回零”的部分执行策略；若业务上希望“任一轴被安全拒绝则整次 `L=U` 回零全部拒绝”，需在实施前确认。

---

> 卡住就停下汇报，不要自己乱改。
