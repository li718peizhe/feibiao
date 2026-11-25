# 审查报告 · 4j-yaw-m2006（第 2 轮复审）

- VERDICT: APPROVED (PASS)
- MAX_SEVERITY: None
- REVIEW_ROUND: 2
- BRANCH: auto/4j-yaw-m2006
- COMMIT_SHA: 未提交工作区改动（基线 HEAD `e17b490e8ffbefedc08c96d8d74396ba257487de`，未提交 diff SHA-256 `4ca0e3a8cdd7269b1dec9944f592a643df2d0cca46d889f4e543d8079ca6a884`）

## 一、范围与冻结证据核验

- **授权文件一致性**：通过。`git status -sb` 与 `git diff --stat` 仅包含定稿 v2 授权的 8 个文件（`README.md`、`src/dart_params.hpp`、`src/motor_axis.hpp`、`src/motor_axis.cpp`、`src/hw/launcher_hw.hpp`、`src/hw/launcher_hw.cpp`、`src/launcher_io.hpp`、`test/host_test.cpp`），无超范围改动，无禁改模块修改；`git diff --check` 无异常；基线 HEAD 保持 `e17b490e8ffbefedc08c96d8d74396ba257487de` 未提交。
- **冻结哈希一致性**：通过。
  - 工作区实时 `git diff --binary --no-ext-diff` SHA-256 = `4ca0e3a8cdd7269b1dec9944f592a643df2d0cca46d889f4e543d8079ca6a884`（46,357 B），与 `build-host/primary-final.diff` 及 `build-host/primary-freeze.json`（SHA-256 `0cf844377e4003709fc43bf71c59205200318e06ad9f89759e28ac989948f2c7`）完全一致。
  - `build-host/primary-freeze.json` 中 8 个授权文件 SHA-256 与工作区文件逐一匹配（`file sha mismatches: []`）。
- **测试与 ARM 预检证据**：通过。
  - 只读复跑既有验证二进制：`.\build-host\host_repair_r1_verify.exe` 输出 `PASS: 2125939 checks`（退出 0）；`.\build-host\hx711_repair_r1_verify.exe` 输出 `PASS: 2193 HX711 checks`（退出 0）。
  - 独立核验 `build-host/arm_primary_evidence.json`（SHA-256 `9563ff7042c27db279caa321407a6d686692d07d469ade8971f4e2c65aa0a000`）：25 个 DartLauncher ARM 源码单元全部退出 0、stderr 为空；全部输入与 `-I` 路径均指向隔离工作区 `auto-4j-yaw-m2006`，无 `Modules/local/DartLauncher` 残留引用；25 份 `input_sha256` 与当前源码一致，25 份 `object_sha256` 与 `build-host/arm_primary_*.o` 一致。
  - 顶层 `src/*.{hpp,cpp}` 依赖扫描为空，无新增 HAL/LibXR/FreeRTOS 依赖。

## 二、第 1 轮问题复核结果

1. **[P1] `src/motor_axis.cpp:339-344` 已回零双端轴断力复使能** — **已修复，未引入新问题**
   - `!enabled` 分支改为 `if (!Homed()) { Stop(); } else if (jogging_) { HoldPosition(); }`。
   - 未完成回零与未回零点动遇 `enabled=false` 仍立即 `Stop()` 清测量并禁止自启；已回零状态断力返回 0 且保留 `active_`、目标与动态边界（点动中断力切入当前位置 `HoldPosition()`），复使能后立即可保持且 `Settled()`/`Aimed()` 为真；非双端轴路径未受影响。
2. **[P1] `test/host_test.cpp:5429-5506` 重复块清理与动态覆盖补齐** — **已修复，未引入新问题**
   - 两处重复代码块已删除；`SpanLong` 用 `high = 90.3 * 2 * kPi` 精确断言 `DualHomeFault::SpanLong`；新增 `Homed()` 下 `jogging=false/true` 断力复使能保持与 `Settled()` 测试、未回零双向手动 `Jog` 限流与归零测试、`BackoffFirst`/`BackoffSecond` 30 s 超时测试、退离弹性回弹无反向电流测试；4f 联动测试改为由 `cmd.salvo` 驱动 `MatchFlow` 发出 `cmd.auto_home_aim` 后再启动 `p.Start()`，完整覆盖 `WaitAim -> WaitSlider`。
3. **[P2] `src/motor_axis.cpp:561-565` 退离/回中误差进入容差后重置接触窗口** — **已修复**
   - `std::fabs(error) < config_.home_tolerance` 分支已补 `else { ResetContact(); }`。
4. **[P2] `src/motor_axis.cpp:528-531` 退离阶段弹性回弹单调推进 `target_`** — **已修复**
   - `!centering` 退离阶段当 `Position()` 位于 `target_` 与 `end_` 之间时单调更新 `target_ = Position()`，避免向硬限位拉回，且不越过 `end_`。
5. **[P2] `src/hw/launcher_hw.cpp:435-440` `monitor_feedback_.yaw_*` 安装开关保护** — **已修复**
   - 已统一加上 `kYawInstalled ? ... : ...` 条件保护，与 `pitch_`/`arm_` 风格一致。

## 三、未确认项与后续集成/现场门禁（不构成本轮代码失败）

- **完整固件 ELF 未构建**：当前为模块隔离工作区未提交状态，仅完成 25 个 DartLauncher 源码单元的 ARM 交叉编译预检（`-c`）；待用户授权本地提交并同步至 `dart-fw/Modules/local/DartLauncher` 后执行完整链接门禁（核对无 ODR/符号冲突及 `OnMonitor()` 新增 `Printf` 模板后的 `.text`/栈水位）。
- **未烧录与现场待确认项**：
  1. CAN2 / C610 ID2（`0x202`）实车在线与方向极性（负向寻端对应现场左/右）；
  2. 首轮 `kYawHomeCurrent = 848`（0.848 A）在实车静摩擦与负载下能否稳定带动且不中途误判接触；
  3. 实测两端跨度是否落于 `4~90` 输出轴圈内，以及算术中点相对实际发射方向正前方的偏置；
  4. USB CDC 128 B 缓冲下 `OnMonitor()` 第 5 行及新增两行 yaw 日志是否完整不截断；
  5. 4f 自动回零实车时序（pitch 先完成后等待 yaw 回中再启动滑块，且 4i 换弹关闭时自动停住不发射）。

VERDICT: APPROVED
