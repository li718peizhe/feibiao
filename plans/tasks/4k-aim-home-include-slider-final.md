# 任务 4k 定稿方案：手动 `R=M, L=U` 拨轮上 2 s 联动 3508 滑块归零

- **日期**：2026-11-24
- **状态**：已规划
- **对应事项**：Orca item 6
- **依据方案**：
  - A 版（主对话）：`plans/tasks/4k-aim-home-include-slider-A.md`
  - B 版（`planner`）：`plans/tasks/4k-proposal-planner.md`
- **目标**：手动模式（`R=M`）选定瞄准组（`L=U`）并将拨轮往上推到底保持 2 s 时，在归零 `pitch` + `yaw` 的同时一并启动 `3508` 两侧滑块归零；原有 `R=M, L=M` 单独滑块归零、`R=M, L=D` 取镖/装填、`MatchFlow` 自动模式分步归零与 `Safety::SliderMustHold` 安全规则均保持不变。

---

## 1. 方案对比与采纳

| 分歧点 | A 版（主对话） | B 版（`planner`） | 采纳结论与理由 |
|---|---|---|---|
| **联动放在哪一层** | 保持 `ModeCommand::home_request` 不变，仅在 `LauncherHardware` 适配层判断 `command_.home_request && command_.group == ManualGroup::Aim` 时补调滑块回零 | 在纯逻辑 `ModeCommand` 将 `home_request` 拆为显式 `home_aim` 与 `home_slider`（`R=M, L=U` 置位两项，`L=M` 仅置位 `home_slider`），`MatchFlow::Takeover` 同步清零 | **采纳 B 版**。原因：① 联动规则进入纯逻辑层，`host_test.cpp` 可直接单测 `R=M, L=U` 同时触发 `home_aim && home_slider`；② `ModeCommand` 语义与 `auto_home_aim` / `auto_home_slider` 完全对称；③ `StartHome(ManualGroup)` 保持单组纯职责，对 `MatchFlow::AutoHomeStart` / `NeedsHoming` 零侵入。没采纳 A 版的适配层硬编码，因为 `launcher_hw.cpp` 无法进入主机单测 |
| **未安装轴告警聚合** | `ManualGroup::Aim` 按 `!(kPitchInstalled \|\| kYawInstalled \|\| kSliderInstalled)` 跳过未安装警告 | `Tick()` 中对 `home_aim && home_slider` 显式用 `any_installed` 聚合，仅三轴全未装才 `WarnNotInstalled(now)` | **采纳 B 版（细节更严密）**。原因：在 `kPitchInstalled && kYawInstalled && kSliderInstalled` 为真时两者等价，但 `any_installed` 写法的语义更直观，不会在某一组未装时误报未安装警告 |
| **`SliderMustHold` 托持处理** | 托持时跳过滑块回零并打印告警，`pitch`/`yaw` 照常回零 | 同 A 版：独立调用 `StartHome(Aim)` 与 `StartHome(Slider)`；滑块命中 `SliderMustHold` 返回 `BlockedBySliderHold` 并保持，`pitch`/`yaw` 照常回零 | **一致采纳**。既守住“滑块托着未锁住发射机构时绝不回零、防走火”红线，又不影响 `pitch`/`yaw` 独立归零 |
| **源码改动基线** | 直接在 `dart-fw/Modules/local/DartLauncher`（含 4j 最新实车无超调调参）增量修改并编译 `DevC.elf` | 同样指出 `dart-fw/Modules/local/DartLauncher` 与 `xrobot-local/DartLauncher` 存在 4j 分叉，建议先确认落点 | **采纳 A 版（先在 `dart-fw/Modules/local/DartLauncher` 增量实施，不跑 `xrobot setup --update`）**。原因：`xrobot-local` 尚未合入 4j 实车调参，严禁覆盖当前车上已验证的 4j 改动；4k 验证后再统一回并 `xrobot-local` |

---

## 2. 阶段划分与改动文件

> 所有源码路径均位于 `C:\Users\Lenovo\Desktop\xrobot\dart-fw\Modules\local\DartLauncher\` 下（**严禁运行 `xrobot setup --update` 覆盖未提交的 4j 调参**）。

### 阶段 1：纯逻辑接口与状态映射
1. **`src/mode_logic.hpp`**：
   - 将 `ModeCommand` 中的 `bool home_request = false;` 替换为 `bool home_aim = false, home_slider = false;`；同步更新注释说明均为单拍事件。
2. **`src/mode_logic.cpp`**：
   - `ModeLogic::Update()` 中拨轮正向保持满 `kHomeWheelTime`（2 s）分支改为：
     - `out.home_aim = direction > 0 && out.group == ManualGroup::Aim;`
     - `out.home_slider = direction > 0 && (out.group == ManualGroup::Aim || out.group == ManualGroup::Slider);`
     - `out.fetch_request = direction > 0 && out.group == ManualGroup::Loading;`
   - 保留 `wheel_sent_ = true` 单次触发、换向/回中/换组重置计时规则不变。
3. **`src/match_flow.cpp`**：
   - `MatchFlow::Takeover(ModeCommand &c)` 中将 `c.home_request` 替换为 `c.home_aim = c.home_slider = false;`。

### 阶段 2：适配层分发（保持 `StartHome` 单组纯职责）
4. **`src/hw/launcher_hw.cpp`**：
   - 替换 `Tick()` 中的 `command_.home_request` 分支：
     - 当 `!sequence_.Busy() && (command_.home_aim || command_.home_slider)` 时：
       - 若 `command_.home_aim && command_.home_slider`：按 `kPitchInstalled || kYawInstalled` 调 `StartHome(ManualGroup::Aim, now)`，按 `kSliderInstalled` 调 `StartHome(ManualGroup::Slider, now)`；仅三轴均未安装时调用 `WarnNotInstalled(now)`。
       - 否则按单个标志分别调用 `StartHome(ManualGroup::Aim, now)` 或 `StartHome(ManualGroup::Slider, now)`。
   - **不修改** `StartHome(ManualGroup, double)` 与 `auto_home_aim` / `auto_home_slider` 路径，确保自动模式与托持保护（`SliderMustHold` 拒绝滑块且维持 `stop_hold_ = true`）零变化。

### 阶段 3：主机单元测试与文档同步
5. **`test/host_test.cpp`**：
   - 更新 `TestWheelAndManualCancel()`：验证 `R=M, L=U` 正向满 2 s 仅单拍同时置位 `home_aim && home_slider`；`R=M, L=M` 仅置位 `!home_aim && home_slider`；`R=M, L=D` 仅置位 `fetch_request && !home_aim && !home_slider`；以及回中重触发、反向重置、无力档忽略。
   - 更新 `TestMatchRefuseLaunch()`、`TestMatchTakeover()` 中对 `home_request` 的引用为 `home_aim` / `home_slider`。
   - 保持 `AutoHomeStart` 与 `NeedsHoming` 全部既有断言不变。
6. **`README.md`**：
   - 更新第 133 行附近遥控说明：写明手动 `R=M, L=U` 拨轮上 2 s 同时回零 `yaw/pitch` 与两侧 `3508` 滑块（滑块托持未锁住发射机构时仅拒绝滑块并保持，`yaw/pitch` 照常回零）；`R=M, L=M` 仍保留单独回零滑块。

### 阶段 4：编译与主机回归验证
- 主机测试（在 `dart-fw/Modules/local/DartLauncher` 目录）：
  ```powershell
  $env:PATH = "C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;" + $env:PATH
  $src = (Get-ChildItem src\*.cpp).FullName
  g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test\host_test.cpp $src -o build-host\host_test.exe
  .\build-host\host_test.exe
  ```
- 固件编译（在 `C:\Users\Lenovo\Desktop\xrobot\dart-fw` 目录）：
  ```powershell
  cmake --build --preset debug
  ```

---

## 3. 验收标准
1. **编译与回归**：`host_test.exe` 在 `-Wall -Wextra -Werror` 下零警告编译且全量通过；`dart-fw` `debug` 构建生成 `build/debug/DevC.elf` 零错误零新增警告，并记录最新 SHA-256。
2. **代码边界**：未触碰 `Safety::SliderMustHold` 判定、`MatchFlow::AutoHomeStart` 时序、4g RS485 与 4j yaw 实车调参，未执行 `git push` 或开 PR。
3. **实车验收预期（烧录后现场确认）**：
   - 未上膛/未托持态，`R=M, L=U` 拨轮上 2 s：`pitch`、`yaw`、`3508` 滑块同时开始回零，结束后 `home=1/1/1/1/0`、`sl≈363~367 mm`、`step=空闲`，随后 `L=M` 拨轮下 2 s 可直接上膛；
   - `R=M, L=M` 拨轮上 2 s：仅滑块回零；
   - `SliderMustHold` 托持态下 `R=M, L=U` 拨轮上 2 s：串口打印 `滑块托着发射机构，拒绝回零`，滑块保持原位不松劲，`pitch`/`yaw` 照常回零；
   - 回零途中切 `R=D`（无力），立即安全停机。

---

## 4. 风险点与需人工确认处
1. **双目录分叉（已锁定方案）**：本次在 `dart-fw/Modules/local/DartLauncher` 增量实施，不跑 `xrobot setup --update`，待 4k 验证后再统一回并 `xrobot-local/DartLauncher`。
2. **首次并行运动线束观察**：实车首次执行“yaw 扫动 + pitch 回零 + 滑块回顶”并行动作时，操作手需目视确认线束与结构无干涉；如存在干涉，后续再改回串行。
3. **托持语义确认**：滑块处于 `SliderMustHold` 时，本方案采取“拒绝滑块并保持、瞄准组照常回零”。
4. **需人工确认**：确认定稿后即派 `worker` 实施并经 `reviewer` 审查。
