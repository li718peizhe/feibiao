# 4j 双端寻端、回行程中点、自动回零集成——定稿 v2

2026-11-24；item 5；用户已明确要求自动测两端并回中、合入自动回零。替代旧4j单端方案与手动调试版提案的执行地位；旧正文保持冻结。本文写定后不改，改方案须先报用户。worker轮次=首次实现；local_only；不提交、不push、不烧录。

## 目标与取舍

源项目 `C:/Users/Lenovo/Desktop/xrobot/xrobot-local/DartLauncher` 基线 `e17b490e8ffbefedc08c96d8d74396ba257487de`，固件 `C:/Users/Lenovo/Desktop/xrobot/dart-fw` 基线 `fa0838b6b405db715d3be8eb552784da49f3cc35`。yaw沿用CAN2/C610编号2/0x202。用户要求的中点为实测两接触位置的算术均值，不等于已标定发射方向正前方。

A/B独立方案为 `4j-yaw-dual-end-A.md`、`4j-yaw-dual-end-B.md`。逐项合并：

| 方面 | A | B | 定稿 |
|---|---|---|---|
| 架构 | 专用YawAxis | MotorAxis双端模式 | 采用B，避免拆MotorPort与多处统一循环；仅双端模式有新行为，其他轴回归不变 |
| 坐标 | 转子累计计数 | 输出轴rad | 采用B，counts_per_unit=36*8192/(2*pi)，日志rev；不冒充发射架角度 |
| 寻端判据 | 电流+转速+位移窗口 | 电流+转速持续 | 采用A三项并用，新鲜反馈是必需输入；不把单帧零速当端点 |
| 阶段 | 两端退离再回中 | 同A，外部Seeking/Retreating契约不变 | 采用B契约；回中稳定才Homed |
| 速度 | 转子600rpm | 输出轴2/3圈每秒（4320/6480转子rpm） | 采用A较慢速度：输出轴(600/60/36)圈每秒，寻端/回中相同；放宽yaw独立时限，不改其他轴 |
| 行程窗 | 基于旧310mm留余量 | 输出轴4~90圈 | 采用B 4~90输出轴圈，仅作异常检验不是机械已标定范围 |
| 失能/离线 | 丢弃未完成寻端，禁止自恢复 | 同A但建议一般化修改 | 限定双端模式行为，不能顺手改其他轴失能规则 |
| 集成构建 | 可追溯真实源码 | 提交后同步 | 无git提交授权：先隔离实现+主机回归+真实ARM预检；不得因此闲置任务或用旧ELF冒充新构建 |

## 精确授权文件（源模块隔离工作区）

- `src/dart_params.hpp`
- `src/motor_axis.hpp`、`src/motor_axis.cpp`
- `src/hw/launcher_hw.hpp`、`src/hw/launcher_hw.cpp`
- `src/launcher_io.hpp`（仅澄清SetYaw坐标单位注释）
- `test/host_test.cpp`
- `README.md`

不得修改 MotorPort/RMMotor、Safety/LaunchSequence/MatchFlow/ModeLogic、YawGeometry源码、CAN BSP、4g拉力、Referee。YAML总线与reverse保持。4i摆臂/推杆false和所有pitch/滑块参数保持。旧YawGeometry可保留作历史未接入工具，不再进入实际yaw链路。新增参数/配置须遵从命名风格，默认双端=false时保持原行为。

分支 `auto/4j-yaw-m2006`；use_worktree=true，由Orca创建源模块隔离工作区，不在原源码工作目录写实现。原固件有USART1=9600与tension_uart未提交变更、未跟踪build-4g-final.log；Referee有三处补丁，均保留。可只读原构建配置及compile_commands，生成本任务独立输出，不覆盖原ELF。

## 轴级设计

AxisConfig末尾新增双端专用命名配置及默认值：dual_end_home=false、轴级home_timeout默认kHomeTimeout、center_speed、end_margin、min/max_span、backoff_timeout、center_settle_time、接触位置窗口/速度阈值/观察时间。配置来自constexpr，不增加多层重复参数校验。持有只读双端阶段与故障原因、实测span、有效软边界。

实现可用独立私有StepDualHome辅助函数避免庞大Update；不得新建另一套反馈解码或并行控制器。外部HomeState沿用：两次寻端及两次退离为Seeking，回中为Retreating，成功后Homed，错误Failed。未完成时Homed=false。

端点优先使用累计原始counts记录，避免offset/重复回零污染：Ccounts=(Acounts+Bcounts)/2；新的 offset=-Ccounts/counts_per_unit。span=abs(Bcounts-Acounts)/abs(counts_per_unit)，运行软边界[-span/2+margin,+span/2-margin]。本次开始即使旧边界有效也不能用旧软边界夹死寻端。所有SetTarget/RampTo/Jog/HoldPosition/逼近限速使用当前有效边界；非双端轴取原边界。

阶段：SeekFirst(负向) -> BackoffFirst -> SeekSecond(正向) -> BackoffSecond -> Centering -> Done。负/正不保证对应现场左/右，按日志核对；测均值不依赖其贴标签。

### 寻端、退离、回中

1. 启动需有效且在线反馈；不允许用未收到反馈的默认0当端点。启动清旧span/阶段/计时/窗口/积分，本次未完成不保留Homed。
2. 寻端速度环且限流848。命令绝对值达到限流95%以上、反馈转子速度绝对值<=5rpm、最近0.5s累计位置跨度<=转子0.05圈，连续满足窗口才记录接触位置。不能只每tick复用已过期的反馈；已有MotorPort在线门控继续生效。
3. 允许初始就在第一端，不设置“第一段必须先走X”的要求；但记录后必须退离成功及测得有效两端跨度才认可。
4. 接触后立即消除向端点电流和积分，下一阶段仅施加退离方向控制。退离0.5输出轴圈，目标限速不超过寻端速度；实际达到退离目标容差且速度降下才切阶段。退离方向持续受阻或超时30s即失败。
5. 第二端记录须方向相对第一端正确且span处于[4,90]输出轴圈；退离后才能回中。不足/超出报错，不能自动提高电流或把第一处任意卡点当已证实真实端点。
6. 回中以当前反馈位置初始化斜坡，向0目标限速移动，输出848上限；target到0、位置误差<0.05输出轴圈、转子速度<=5rpm连续0.3s才置Homed；保持active、目标0、不在成功瞬间清成未使能。回中偏差仍大且满足接触判据报CenterBlocked，不到位不能标完成。
7. 位置/速度窗口判据可能把障碍当端点，跨度窗口不能发现所有“跨度合法的中途障碍”；文档及测试如实反映，不能声称能够识别所有机械卡阻。现场核对日志跨度和行程中点。

### 首轮常量（集中命名，不能擅自改数值凑测试）

- kYawInstalled=true；输出轴单位rad；原kYawPos/kYawSpeed与正常current_cap=2829保留。
- 寻端/退离/回中/手动满杆速度均 `2*pi*(600.0/60.0/36.0) rad/s`（转子600rpm，输出轴约0.278圈/s）。旧导程为4mm时约1.11mm/s，仅解释速度不作为几何标定。
- 回零全过程限流848，正常已回零手动限流2829；不自动加流。
- 退离0.5输出轴圈、边距1输出轴圈、合法span4~90输出轴圈、每个寻端从本段起点最大90输出轴圈，超过锁故障。
- 接触窗口0.5s、转子5rpm、位移窗口转子0.05圈，95%限流阈值。
- 回中位置容差0.05输出轴圈、低速阈值转子5rpm、稳定0.3s；Aimed位置容差同前，速度容差输出轴0.1圈/s。不要将位置容差与转子0.05圈窗口混用。
- yaw回零总时限1000s（最坏约2.5*90/0.278=810s，再留退离与稳定余量）；退离每段30s。其他轴仍90s。主机测试使用模拟时间，无须真实等待1000s。

## 停止/恢复契约

双端轴每个寻端/退离/回中阶段遇Stop、enabled=false、online=false立即输出0并清未完成测量/回到Idle（已Failed保持Failed），重新使能不自启动。不要让旧回零阶段半截恢复。离线后已完成的边界和Homed也失效，重连后完整重测；复位清专用故障但不能直接标Homed。

回零完成后松手HoldPosition，切自动StopMotors对yaw仿照pitch保留当前位置目标；无力/故障下Gate照常零电流，不靠无力保持电流改变语义。重复StartHome从头重测两端。双端未回零手动可双向，速度/限流同寻端；不把未回零手动点动视作标定完成。回零忙Manual不得覆盖。

故障至少区分：超程、总超时、跨度过短、退离受阻、回中受阻。安全层复用现有码映射，按现有RefreshState把轴Failed阻断自动入口，诊断记录精确原因。ClearFault清测量/窗口/阶段，下一次回零从头测。

## 集成落点与遗漏检查

- YawConfig使用上述模式、counts_per_unit、双向名义窗口（仅占位非学习后边界）、hold_on_release。ModeLogic继续输出现有command.yaw_rate，直接把kManualYawSpeed重定义成输出轴rad/s，Manual直接Jog(command.yaw_rate)，不再经过YawGeometry。
- `SetYaw(double)`当前无生产调用，改为输出轴rad目标并在launcher_io/hw注释与README明确；保留函数签名，不用YawGeometry误转，未来视觉需要另做几何适配。
- `Aimed`用输出轴容差；第5行yaw日志改rev；额外日志记录阶段、span、cmd/act/rpm，必要时分短行并按既有快照锁复制；不能在监控线程读写轴状态。act按当前驱动 `torque/0.005/10*16384` 逆算，只表示驱动原反馈数值。
- 所有StartHome(Aim)入口复用Axis.StartHome；MatchFlow/4f现有Homing检测需能覆盖Seeking与Retreating所有子阶段。必须用真实轴反馈驱动MatchMechanism的联动测试，不是硬写布尔值假通过。pitch完成而yaw尚回中时不能启动滑块。4i仍在自动回零结束后因换弹机构未装停住，不为了测试放开发射/换弹。
- 核查既有普通自动回零/比赛时序是否存在小于本流程时限的外层等待；发现有则停下报告必要文件，不越清单自行改。

## 验证门禁

worker在隔离源模块目录使用原项目已有g++环境，确认build-host输出目录后逐条运行，编译失败不得执行旧二进制：

```powershell
$env:PATH = 'C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;' + $env:PATH
$src = (Get-ChildItem -LiteralPath src -Filter '*.cpp').FullName
g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test/host_test.cpp $src -o build-host/host_test_4j.exe
.\build-host\host_test_4j.exe
g++ -std=c++17 -Wall -Wextra -Werror -O1 -Itest/hx711_stubs -Isrc/hw test/hx711_port_test.cpp src/hw/hx711_port.cpp -o build-host/hx711_port_test_4j.exe
.\build-host\hx711_port_test_4j.exe
```

要求全部PASS，扫描src顶层纯逻辑无新增HAL/LibXR/FreeRTOS依赖。新增动态测试：中间/两端/非对称/负多圈启动、跨编码器圈界、回中未稳不Homed、反馈仍移动不认接触、短跨度/超长/无端/退离阻塞/回中卡阻/超时、每阶段Stop/无力/离线再使能不自启、重复回零不受旧offset和边界污染、ClearFault后重测、动态边界下SetTarget/RampTo/Jog/HoldPosition一致、手动保持和切模式Settled、真实yaw轴+MatchFlow的4f顺序，以及原pitch/滑块/摆臂回归。别给无法物理区分的合法跨度障碍写虚假“必识别”断言。

ARM门禁：只读原固件 `build/debug/compile_commands.json`，将**所有** DartLauncher源码及include前缀改为本隔离目录，删除原编译输出/依赖文件参数，按原编译选项编译本轮受影响适配单元，输出到隔离build-host；逐项记录实际命令、输入路径、退出码、sha。不得只编原Modules副本。不能把ARM单文件预检声称完整ELF已构建。

若无需提交即可利用现有正式CMake支持的模块路径变量在独立构建目录构建，允许只读依赖、保持原生成文件及lock不变的真实完整验证，须证明包含新代码；不得为此改工程配置或安装依赖。若做不到，报告完整构建待模块同步，不私自提交、不remove/add模块、不覆盖已安装固件。

## 交付与审查

worker写 `C:/Users/Lenovo/Desktop/xrobot/plans/tasks/4j-yaw-dual-worker-r1.md`，提供实际隔离工作区/分支、基线HEAD（未提交则明确）、diff文件/哈希、授权文件改动清单、所有命令与结果、动态测试覆盖、ARM证据、完整ELF有无、未现场验证事项。非零编译或失败测试意味着未通过，不能报成功。卡住就停下汇报，不要自己乱改。不派其他agent。

主对话随后派reviewer，只读新定稿+确切HEAD和未提交diff哈希+worker证据，审查r1报告 `plans/tasks/4j-yaw-dual-review-r1.md`；明确APPROVED才本地交付，原风险分级由reviewer独立决定。当前无提交授权，reviewer可先审查冻结未提交diff，不能伪造commit SHA或代为批准。后续用户授权提交/同步时再补集成证据。

## 需人工确认处

无阻塞实现的新增决策。用户已批准自动寻两端、回中及自动回零集成。首轮速度/限流取上文命名默认值，现场核对方向、0.848A是否能动和学习跨度；不在代码阶段操作设备。git提交授权待后续独立处理，不阻塞本地实现/测试；不push、不烧录。
