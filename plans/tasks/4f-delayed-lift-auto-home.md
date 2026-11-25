# 任务 4f：发射后抬起预取镖，全自动包含归零

日期：2026-11-16（2026-11-22 软件收口完成；2026-11-24 4i 固件下实车一次自动回零顺序现场验证通过）。状态：已完成（第一阶段：自动回零顺序与退让验证通过）。

## 目标与成功标准

用户反馈当前已能完成两发流程，但发射瞬间镖架震动，会把已经抬起、仅由磁铁吸持的下一发震掉。本任务把下一发留在镖座支撑上：磁铁下探吸稳后保持低位，上一发发射并完成出膛等待后，才慢抬并换弹。

“全自动”按一次启动完成归零、上膛、两发发射及中间换弹执行。沿用右拨杆上、左拨杆中→下的启动手势；不再要求操作手先分别回 pitch 和滑块，也不再等待第二次发射手势。开机、仅进入自动档、拨杆保持在下、流程结束均不自行重新启动。

本轮只写计划和交接，不修改控制代码、不编译、不烧录。执行者不读 `plans/dart-fw-libxr.md`，不运行 graphify，不使用子代理，不安装或升级工具，不推送。

## 已核对的基线

| 对象 | 2026-11-16 读取结果 |
| --- | --- |
| 源码仓库 | `xrobot-local/DartLauncher`，分支 `codex/apply-load-position-calibration`，HEAD `00e2e1cc9f5deea0c2912d909ee831b11a209725`，工作区干净 |
| 固件仓库 | `dart-fw`，分支 `dart-launcher`，HEAD `421745995139fd7acbe605726b80be600e924018`，工作区干净 |
| 同步模块 | `dart-fw/Modules/local/DartLauncher`，HEAD 与源码相同，工作区干净 |
| Referee | 保留 `Referee.hpp`、`RefereeTypes.hpp`、`tests/types_test.cpp` 三处既有修改 |
| 当前库存初始化 | `LauncherHardware` 使用 `ResetSeatedLeft()`：发射座和左镖座各一发，磁铁和右镖座为空 |
| 发射前预取 | `ExpandOne()` → `ExpandPrefetch()` → `FetchSteps()`：吸稳后立即抬起并摆到待命位 |
| 发射前收杆 | `FireSteps()` → `ExpandStandby()` 会收杆；`Safety::FireBlocker()` 另有未收杆禁止发射检查 |
| 第二发 | `Action::NextOne` → `ExpandOne(true)` 插入 `等待手动发射`，适配层再次收到 `salvo` 才 `ConfirmFire()` |
| 归零入口 | `ModeLogic` 的启动事件是单拍；当前只有 `MatchFlow` 在比赛接管时生成自动归零请求，普通自动启动会被 `LaunchSequence::Tick()` 的 `NotHomed` 检查拒绝 |
| 出膛判定 | 没有出膛传感器；现有 `飞行` 步骤按 `kFlightWait=0.5 s` 计时，之后才扣发射座库存并增加 `Shots()` |

用户本轮确认两发流程已经可执行，优先处理震落问题；板上镜像版本没有新增核验记录，沿用实际源码基线规划，不把旧交接的“4d 待现场检查”作为本轮失败结论。

## 计划编制记录

- [x] 阅读 handoff、安全要求和 4c/4d/4e 任务记录，核对三个 DartLauncher 工作区及 Referee 既有修改。
- [x] 查清预取、发射检查、第二发等待、自动归零和实际硬件请求执行顺序。
- [x] 写出改动范围、状态与时序、回归标准、执行命令和现场验收，并回读核对。

## 实施步骤

- [x] 1. 运行既有主机基线，添加震落时序、全自动归零和连续两发测试，记录旧实现的预期失败。
- [x] 2. 拆开预取的下探与抬起，调整发射准备及对应姿态检查，保留完整取镖和装填入口。
- [x] 3. 接入普通自动启动前的归零协调，去掉连发中的逐发手动等待，验证取消、故障和比赛接管。
- [x] 4. 跑主机回归、同步模块回归和完整固件构建，核对产物并分别提交，更新本任务、README 和 handoff。

### 2026-11-22 软件收口与交付记录

- **阻塞解除**：用户确认 pitch 回零沿用堵转判据，4f 自动归零协调阻塞已解除。
- **实际修改文件**：
  - `xrobot-local/DartLauncher`（提交 `5f758fe2ddc8337ffa34c2d15ccdc07149e59b0e`，主题 `Delay prefetched dart lift and automate homing`，分支 `codex/apply-load-position-calibration`）：`src/mode_logic.hpp`、`src/match_flow.hpp`、`src/match_flow.cpp`、`src/hw/launcher_hw.hpp`、`src/hw/launcher_hw.cpp`、`src/launch_sequence.hpp`、`src/launch_sequence.cpp`、`src/safety.hpp`、`src/safety.cpp`、`test/host_test.cpp`、`README.md`（共 11 个文件；未做 CRLF→LF 行尾转换）。
  - `dart-fw`（提交 `c154c462d6d591ac570dea47941c9558de344779`，主题 `Lock automatic homing and delayed lift module`，分支 `dart-launcher`）：`Modules/modules.yaml`、`xrobot.lock`；`Modules/QDU-Robomaster/Referee` 3 处既有修改原封不动。
  - 根目录文档：`plans/tasks/4f-delayed-lift-auto-home.md`、`handoff.md`。
- **旧实现失败记录**：原始主机基线通过 30834 项、HX711 通过 2193 项；新增测试在旧实现分别报 `prefetched dart stays supported until the first flight completes` 和 `one automatic start homes the aim before requesting the salvo`。
- **修改后验证结果**：
  - 源码仓库主机回归：`host_test_4f.exe` `PASS: 31539 checks`（含完整 `TestAutomaticHomeStartRegression`）、`hx711_port_test_4f.exe` `PASS: 2193 HX711 checks`；纯逻辑依赖 `rg` 检查退出码 1（0 命中）。
  - `launcher_hw.cpp` 使用固件 `compile_commands.json` 实际 ARM 参数做 `-fsyntax-only` 检查退出码 0。
  - 同步模块回归（`dart-fw/Modules/local/DartLauncher` @ `5f758fe2ddc8337ffa34c2d15ccdc07149e59b0e`）：`host_test_4f_sync.exe` `PASS: 31539 checks`、`hx711_port_test_4f_sync.exe` `PASS: 2193 HX711 checks`；`xrobot format --check` 退出码 0。
  - 固件构建：已备份原 4e 镜像为 `dart-fw/build/DevC-00e2e1c-before-4f.elf`（SHA-256 `65E6B3C8B3540E064CFCF8D6DDB0983144FDEF290F5512962A2A4D5C737E8CF4`）；执行 `cmake --build --preset debug` 成功，产出 `dart-fw/build/debug/DevC.elf`，SHA-256 `7A2D2584B3BF4B708622FE9747F487F18A954A0A5257D865C79D0FC8B367A44D`（RAM 32624 B / 24.89%、CCMRAM 64 KB / 100.00%、FLASH 222736 B / 21.24%）。
  - 代码审查：第 1 轮发现 `AutoHomeStart()` 未消费 `c.fire` 及外部回零取消测试未触发 `salvo` 边沿，返工修复并补齐 `PitchConfig` 退让停机断言后，第 2 轮复审结论 `VERDICT: APPROVED`。
- **现场验收项状态**：下方现场验收第 1～5 项均**未执行**；烧录与实车测试待用户按阶段执行。

现场验收单独记录在本文件；本任务的软件执行不包含烧录或实车发射。

## 具体改动

### 1. 预取分两段

复用现有步骤队列、左右取镖目标、吸稳等待和慢抬速度，不改机械标定参数。

第一段在上一发发射前运行：磁铁断电吸持 → 收杆 → 摆到有库存的一侧 → 下探 → 等待 `kFetchGripTime=0.5 s` → 保持。保持时推杆目标仍为该侧下探位，摆臂仍在该侧镖座位置，不发收杆或摆到待命位命令；镖仍受镖座支撑。

第二段在上一发的 `飞行` 步骤完成后运行：按 `kCrankLiftSpeed` 慢抬至收起 → 摆到该侧待命位 → 继续原装填流程。用现有 `kFlightWait` 作为发射后等待，不另加一个震动延时；保持从解锁、锁舵机等待到出膛等待结束的全过程。慢抬超时仍按实际左右行程除以慢抬速度加舵机余量计算。

`FetchSteps()` 的直接装填路径仍一次完成下探、吸稳、慢抬和回中；`CommandFetchTest()` 的手动取镖仍按原逻辑完成慢抬。只有 `ExpandPrefetch()` 使用两段时序。单发自动入口如果预取了下一发，也须在本发结束后完成慢抬，不能结束在未记录的半段流程。

### 2. 发射准备和姿态检查同步修改

现有 `ExpandStandby()` 和 `FireBlocker()` 都要求推杆收起，只移动抬起步骤会在发射前再次收杆，或报 `CrankNotRetracted`。实现必须同时覆盖这两个位置。

以明确的流程状态记录“本次预取已吸稳、仍在镖座支撑上”和来源侧；该状态在下探与吸稳完成后建立。发射准备在这个阶段保持原姿态；普通持镖状态仍按原流程收杆、避让。

发射检查接受两种明确姿态：现有收杆避让姿态，以及本任务要求的低位预取支撑姿态。后者须关联本次预取状态和持镖来源，并核对对应侧的摆臂、推杆到位条件；只靠 `Held()` 或某个角度相近不能获得放行。复用现有到位公差，检查集中在现有发射检查路径；不伪造反馈，不把 `Snapshot().crank` 写成零，不全局删除推杆检查，不增加配置开关保留旧时序。

低位预取姿态的几何避让是本次行为变更的一部分，须先做现场空载与无皮筋检查。发射座库存、锁止确认、滑块不低于 361 mm、瞄准、在线状态、比赛阶段和闸门检查继续生效；夹爪仍只在正式发射解锁步骤放开。

### 3. 库存、来源与中断

沿用 `Grip(side)` 将该侧库存转为磁铁持镖，不新增库存类别；低位支撑阶段只描述持镖姿态，抬起时不再次扣镖。出膛等待完成仍只调用一次 `Fire()`；放镖仍只调用一次 `Release()`，保留左右来源决定 +2.0° / +1.5° 放镖角度的规则。

取消或故障保留已吸住镖的库存与来源，清除待执行动作及本次低位发射资格；无力恢复不得直接抬镖或自动发射。重新启动时必须经过受控姿态准备，不能只凭仍为 `Held()` 就跳过准备。覆盖低位等待、解锁后出膛等待、慢抬中三个中断点；保留现有 `SliderMustHold`、磁铁无力断电吸持及 `ReleaseInterrupted` 规则，不扩展比赛自动故障恢复白名单。

### 4. 普通自动启动先归零

在现有 `MatchFlow` 中增加普通自动启动协调，复用它已经接收的 `MatchMechanism` 和输出的 `auto_home_aim` / `auto_home_slider`，不新增一套电机归零算法或硬件接口。允许协调的场景为没有接过裁判系统，或裁判阶段为“未开始”；比赛中继续由原比赛流程接管。

收到普通自动模式的 `salvo` 启动边沿时，若流程空闲且当前无故障，则保存这一次启动意图；归零完成以前消费原事件，避免提前调用 `CommandSalvo()` 触发 `NotHomed`。已经全部回零时直接继续启动。

顺序为：需要时回瞄准组 → 等到寻零和退让都完成 → 需要时回滑块组 → 等两侧回零且 `all_homed` 成立 → 发出一次原连发请求。回零进行中不重复 `StartHome()`；全部回零但本拍仍在退让时也不能开始发射队列。摆臂继续用现有绝对编码器建零，未安装的 yaw 不生成寻零动作。

适配层沿用 `StartHome()` 和 `MotorAxis` 的实际反馈状态，先执行离开手动的停机交接，再发归零请求，后续周期不再用交接停机打断归零；已完成的轴不重回零。pitch 沿用 5 mm/s、869 限流、堵转 1 s 和 190 mm 行程上限，不改 PID 或低位回零判据。

收到重复启动时不覆盖正在执行的启动意图；流程忙时不排下一轮任务。切无力、切回手动、遥控器离线、机构故障、故障复位或比赛阶段接管时清掉普通自动启动意图，并停止该准备流程启动中的归零；恢复后等待新的启动边沿。`StartHome()` 因滑块托持拒绝时终止这一次准备并保留现有提示，不循环重试、不偷偷进入发射流程。

### 5. 两发连续执行

去掉 `CommandSalvo(count)` 第二发及后续各发的手动等待，删除对应 `WaitFire` / `ManualFire`、`WaitingForFire()` / `ConfirmFire()` 及适配层确认分支；删除或改写因此失效的测试辅助函数和说明。每发仍执行装填、预取、姿态准备、瞄准、解锁和出膛等待；库存耗尽后结束。

当前两发测试起始库存不变：发射座和左镖座各一发。自动单发入口、独立装填和手动取镖保留；比赛仍依据阶段和闸门控制发射权限，不把这次普通自动启动意图带入比赛接管。

## 允许修改的文件

所有路径均相对于 `C:\Users\Lenovo\Desktop\XROBOT`。

| 文件 | 用途 |
| --- | --- |
| `xrobot-local/DartLauncher/src/launch_sequence.hpp`、`src/launch_sequence.cpp` | 两段预取、低位状态、发射准备、中断和连发步骤 |
| `xrobot-local/DartLauncher/src/safety.hpp`、`src/safety.cpp` | 低位预取支撑姿态的发射检查，其他条件保留 |
| `xrobot-local/DartLauncher/src/match_flow.hpp`、`src/match_flow.cpp` | 普通自动启动意图、归零顺序、完成后单次启动和取消 |
| `xrobot-local/DartLauncher/src/hw/launcher_hw.hpp`、`src/hw/launcher_hw.cpp` | 请求执行结果、归零中断交接及删除第二次触发确认分支；日志仍遵守每条 64 字节限制 |
| `xrobot-local/DartLauncher/test/host_test.cpp` | 复用 FakeIo、ModeLogic、MatchFlow、MotorAxis 测试，覆盖需求行为 |
| `xrobot-local/DartLauncher/README.md` | 全自动手势、归零顺序、低位待发时序和停止方式 |
| `dart-fw/Modules/modules.yaml`、`dart-fw/xrobot.lock` | 用 XRobot 固定新模块提交；不手改 lock，不直接编辑同步模块 |
| `plans/tasks/4f-delayed-lift-auto-home.md`、`handoff.md` | 记录结果、产物版本及现场验收 |

保留 HX711、所有标定值和其他仓库改动；本任务不改 `motor_axis.*`、DBUS/裁判协议、硬件接线、生成代码和依赖。

## 主机验证标准

先在旧代码补测试并确认对应行为失败，再完成实现；旧手动逐发等待测试须按新需求改写，不保留旧行为开关。

1. 两发集成：模拟未回零起点，一次真实拨杆边沿，自动归零后完成两次解锁、两次库存消耗；全程不手动写 `all_homed=true` 冒充归零，也不调用 `ConfirmFire()`。
2. 预取顺序：分别覆盖左右。下探吸稳到第一发整个发射窗口结束前，没有收杆或向中间摆臂命令；出膛等待完成后首次抬起使用慢抬速度，第二发正常装填并发射。
3. 原入口：独立装填、手动取镖仍完整抬起；单发预取后正常收尾；无待取镖时不插入虚假的等待或额外取镖动作；原四发有效库存流程也可连续完成。
4. 发射检查：低位合法来源及对应姿态通过；来源不符、未完成吸稳、摆臂不在该侧、推杆不在该侧下探位均不能使用低位放行。普通推杆未收起、持镖挡路及既有发射条件仍有有效回归。
5. 归零协调：全部未回零、仅瞄准未回零、仅滑块未回零、全部已回零、回零退让中分别覆盖；每组只启动一次，完成前无取镖、上膛或解锁请求。用生产 `MotorAxis` 的反馈驱动寻零/退让，覆盖离开手动时停机与首次 `StartHome()` 的先后关系。
6. 启动边沿：保持拨杆不重启，忙时的边沿不排队；无力、手动、遥控离线、故障复位后的恢复不重放启动；外部无效遥控帧维持现有有效/无效输入回归。
7. 中断：低位等待、飞行等待、慢抬中取消/故障/无力后的库存来源正确；不误抬、不重复扣镖；需要托持时继续托持；释放窗口中断仍报 `ReleaseInterrupted`。
8. 比赛：原阶段禁发、闸门闭合、急停锁存、结算退膛、遥控可选和故障重试回归通过；阶段切换清除普通自动准备意图。保留右侧 HX711 协议测试。

适配层不参加普通主机编译；利用纯逻辑集成测试检查事件顺序，用实际 ARM 参数及完整固件构建检查生产适配层，现场再检查轴真实运动。

## 执行命令与交付

在 PowerShell 7 执行，每条命令完成后检查退出码；主机编译和运行各限 120 s，模块同步限 120 s，固件构建限 300 s，超过限制停止并记录。只使用现有工具和输出目录，不创建工具缓存。

预检：

```powershell
Set-Location 'C:\Users\Lenovo\Desktop\XROBOT'
git -C xrobot-local/DartLauncher branch --show-current
git -C xrobot-local/DartLauncher status --short
git -C xrobot-local/DartLauncher rev-parse HEAD
git -C dart-fw branch --show-current
git -C dart-fw status --short
git -C dart-fw/Modules/local/DartLauncher rev-parse HEAD
git -C dart-fw/Modules/local/DartLauncher status --short
git -C dart-fw/Modules/QDU-Robomaster/Referee status --short
```

主机基线及修改后使用同一命令：

```powershell
Set-Location 'C:\Users\Lenovo\Desktop\XROBOT\xrobot-local\DartLauncher'
$env:PATH = 'C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;' + $env:PATH
$taskSources = (Get-ChildItem -LiteralPath src -Filter '*.cpp').FullName
g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test/host_test.cpp $taskSources -o build-host/host_test_4f.exe
.\build-host\host_test_4f.exe
g++ -std=c++17 -Wall -Wextra -Werror -O1 -Itest/hx711_stubs -Isrc/hw test/hx711_port_test.cpp src/hw/hx711_port.cpp -o build-host/hx711_port_test_4f.exe
.\build-host\hx711_port_test_4f.exe
rg -n '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define' src -g '*.hpp' -g '*.cpp' -g '!**/hw/**'
```

纯逻辑依赖搜索应无匹配；执行时核实排除硬件目录，`rg` 无匹配的退出码 1 按预期记录。

源码验证通过后审阅 diff，逐个暂存本任务实际修改的文件，单独执行 `git diff --cached --check`、`git diff --cached`，再在当前分支做一个本地提交，建议主题 `Delay prefetched dart lift and automate homing`。不暂存整个目录。核对提交哈希、分支和工作区。

固件先保存现有 ELF，确认同名备份不存在后执行：

```powershell
Set-Location 'C:\Users\Lenovo\Desktop\XROBOT\dart-fw'
Get-FileHash -LiteralPath build/debug/DevC.elf -Algorithm SHA256
Copy-Item -LiteralPath build/debug/DevC.elf -Destination build/DevC-00e2e1c-before-4f.elf
```

读取 `Modules/modules.yaml` 确认当前请求，使用上一步真实提交替换下面的 `源码完整提交哈希`；不要照抄占位值：

```powershell
xrobot module remove local/DartLauncher
xrobot module add 'local/DartLauncher@源码完整提交哈希'
xrobot setup --update local/DartLauncher
git -C Modules/local/DartLauncher rev-parse HEAD
git -C Modules/local/DartLauncher status --short
xrobot format --check
cmake --build --preset debug
Get-FileHash -LiteralPath build/debug/DevC.elf -Algorithm SHA256
```

同步副本必须与源码提交一致且干净；在副本目录用同一批源文件和测试执行两套主机回归，输出到源码仓库现有 `build-host`，不得输出到不存在的副本目录。用固件实际 `compile_commands.json` 检查改动的硬件 `.cpp` 编译结果；不需要重新生成产品配置。

完整构建及副本验证通过后，固件仓库只暂存实际变化的 `Modules/modules.yaml` 和 `xrobot.lock`，审阅暂存 diff，当前分支做一个本地提交，建议主题 `Lock automatic homing and delayed lift module`；不提交构建产物。再次核对源码、固件和模块 HEAD、分支及状态。

交付记录写清实际修改文件、旧实现失败的测试、修改后结果、构建命令、ELF SHA-256、两个提交的哈希和主题，以及以下现场检查；本轮未执行的检查逐项写“未执行”。

## 停止条件

- 现有基线失败：先记录失败测试，不在本任务顺手修其他问题；验证失败不提交。
- 发现需求与文件中的机械位置、库存或安全约束存在未在本计划说明的矛盾：停止对应改动并报告位置与原因。
- 新逻辑需要删除既有锁止、托持、阶段、闸门或磁铁限时要求：停止该方案；低位预取只按本计划指定的姿态检查实现。
- 同步会覆盖 Referee 既有修改、模块版本不符、构建或 hooks 失败、改动无法按任务分离：保留现场，停止提交及后续交付动作，记录失败命令和关键错误。

## 现场验收

由用户按阶段执行，实际烧录时再提供与对应 ELF 匹配的命令。烧录会复位、电机会松开；上着膛先退膛，滑块托着没锁住的发射机构时不能断电或烧录，手离开摆臂和夹爪。发射方向及靶附近不得有人。

1. 无皮筋、空镖座检查：一次启动自动回 pitch 和滑块，记录寻零、退让与首个上膛步骤的先后；分别从已回零和未回零状态检查，不重复回已完成的轴。pitch 从低位自动回零仍须现场检查实际到顶和退让。
2. 无皮筋、下一发放在镖座：观察下探吸稳后推杆不抬、摆臂不回；发射机构运动全程不碰该侧磁铁、推杆或镖；出膛等待后才慢抬。左右两侧分开记录。
3. 当前两发带镖流程：发射座和左镖座各一发，一次启动完成归零与两发；第一发震动时第二发仍受镖座支撑、不掉落，之后慢抬、换弹、第二发自动发射，库存归零后停止。
4. 记录首发解锁、开始慢抬、第二发解锁的时间，以及震落、碰撞、落座、继电器释放和重复成功次数；核实 `kFlightWait=0.5 s` 足以避开发射震动，需要调整时只修改这个命名参数并重新验证。
5. 保留原有独立取镖和装填检查；验证低位等待时取消后的保持与重新启动，释放窗口中断按既有规则处理。

## 本轮计划验证记录

- 已完成：文档与代码只读核对、Git 状态核对、计划文件与 handoff 回读检查。
- 命令复核：首次依赖搜索使用 `-g '!hw/**'` 未排除嵌套的硬件目录，匹配到 `src/hw/hx711_port.cpp` 的 `main.h`；改为 `-g '!**/hw/**'` 后重新检查无匹配，退出码 1。
- 未执行：新增测试、控制代码修改、主机回归、ARM 编译、固件构建、模块同步、烧录、实车验收。
- 根目录没有 Git 仓库；本轮只修改根目录计划与交接，没有本地提交，两个主仓库保持原 HEAD。

---

## 4f 实车现场验收记录（2026-11-24）

- **测试固件**：4i 临时屏蔽换弹机构固件 `dart-fw/build/debug/DevC.elf`（SHA-256：`C2A54393EEF4D721E73F1400F02749BF3977B649D5DECF71CC04B9696FD1B41E`）
- **操作流程**：
  1. 上电（右拨杆下）：`fault=无故障`（消除了 `fault=电机离线`），`online=1/1/1/0/0`，`arm=0.00 raw=0[X]` 与 `crank=0[X]` 正确静默。4g RS485 拉力双路读数正常更新（`raw=-5/-3`）。
  2. 切入自动档（右拨杆置上，左拨杆置中）：静置不自行回零，`step=空闲` 保持。
  3. 触发一次启动（左拨杆从中快速拨到下，保持在下）：
     - 15:23:28 触发：串口打印 `[I] 自动回零：瞄准`，pitch 向上寻零至上端挡块后退让至 `pi=95.19 mm`（目标 95 mm），期间双滑块保持静止（`sl=2.39/2.65 mm`）；
     - 15:23:32 pitch 退让完成后：串口打印 `[I] 自动回零：滑块`，双滑块同步向上寻零至挡块并退让完成，双滑块位置 `sl=367.02/367.00 mm`（高度差 0.02 mm）；
     - 15:23:33 全回零完成：`home=1/1/1/0/0`，全局 `home=1`；连发指令被 `!kReloadInstalled` 拦截，仅打印一次 `[W] 换弹机构未安装`；
     - 流程自然停在 `step=空闲`，未触发任何上膛或发射，左拨杆保持在下未重复回零，无刷屏。
- **验收结论**：4f 一次启动自动回零顺序（瞄准 pitch → 滑块）及退让等待、安全停止与单次触发保护在实车上**全部通过**。后续重装换弹机构后即可直接开展带镖两发连续发射全流程验收。
