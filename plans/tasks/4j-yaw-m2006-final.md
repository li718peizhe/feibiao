# 4j yaw M2006 接入合并定稿

日期：2026-11-24；归属：主对话 item 5；Full 轨；交付方式：local_only。
本文写定后不得直接改正文。用户确认记入 state.json 与事项评论；需修改方案先报用户批准。
状态：待用户决策，未授权 worker 开始实现。

## 目标

接通已安装的 yaw M2006，复用现有轴控制，实现手动微调、松手位置保持、切自动后仍能判定瞄准到位，补足回零搜索距离限制和调试读数。代码由 worker 实现、reviewer 独立审查。烧录与现场操作由用户执行。

## 基线与接线

- 源模块：`C:/Users/Lenovo/Desktop/xrobot/xrobot-local/DartLauncher`，基线 `e17b490e8ffbefedc08c96d8d74396ba257487de`。
- 固件：`C:/Users/Lenovo/Desktop/xrobot/dart-fw`，基线 `fa0838b6b405db715d3be8eb552784da49f3cc35`。
- yaw 接 C 板 CAN2 的 H/L 到电调 H/L，C610 编号 2，反馈 `0x202`；板上 CAN2 接口已有收发器。PB5/PB6 是处理器收发引脚，不是电调应直接接的引脚。
- YAML 现有 `yaw_motor` 与 `yaw_can` 绑定正确，不改 CAN1/CAN2、编号或 reverse。
- 当前假设仍为 36:1 M2006、4 mm 导程、310 mm 滑台、旧几何半径 0.6045 m、单边 0~12°；旧几何软件上限约 125.7 mm，非全滑台行程。须由用户确认新装机构仍符合单边模型。
- 保留 dart-fw 已有未提交的 USART1=9600、YAML 的 tension_uart，以及未跟踪 build-4g-final.log；保留 Referee 的三个本地补丁。dirty 项目使用隔离工作区，不能直接在原目录覆盖或清理。

## A/B 逐项取舍

| 项目 | A | B | 定稿 |
|---|---|---|---|
| CAN 接口 | CAN2、编号2 | 同 A，并核对分组机制 | 同意；不再给 CAN1 备选实现 |
| 现有驱动 | 复用 RMMotor/MotorPort | 同 A | 不新写 HAL 初始化/中断/发送线程 |
| 松手保持 | 开启 hold_on_release | 同 A | 采用已有 MotorAxis 实现 |
| 切模式 | 原停机行为不改 | 找到 Stop() 清 active 导致 Settled 失败 | 采纳 B 的 HoldPosition 模式记忆；无力或被禁止输出时仍发零，不额外给 yaw 持续通电 |
| 回零搜索距离 | 310+10 mm | 同 A | 新增 kYawHomeSeekLimit=0.320 m；只是空转距离上限，不证明机械可安全走满320 mm |
| 观测输出 | 按需增加 | 证明负位移角度被夹为0 | 采纳 B 的未夹紧滑台位移、cmd/act/rpm 行 |
| 调参 | 沿用初值、现场标定 | 同 A | 不改变 PID、速度、电流或机械角范围 |
| 同步模块 | 先核实未提交源码如何集成 | 建议本地提交后remove/add模块 | 仅用户明确授权本地提交后同步；不直接remove模块。正常update失败则停下报告 |
| 首次上车 | 先在线/方向、小范围 | 后续建议跑满范围 | 先小范围；现场几何确认前不要求跑满12°或滑台全行程 |

回查依据：`src/hw/launcher_hw.cpp:42-45,714-752,859-863,911-924`；`src/motor_axis.cpp:40-87,191-194,325-406`；B 第2.2节给出了 CAN 分组与反馈缩放引用。A 草稿命令块遗漏了编译命令，以下命令以本定稿为准，不能直接运行旧测试二进制当作新验证。

## 授权改动（用户确认闸口通过后生效）

源模块文件：
1. `src/dart_params.hpp`：`kYawInstalled=true`；新增 `kYawHomeSeekLimit=kYawTravel+0.010`。仅修正同处 pitch 安装注释的实际 CAN1；其余参数保留。
2. `src/hw/launcher_hw.cpp`：YawConfig 改用 config 局部值，设置 `hold_on_release=true` 和 `home_seek_limit`；StopMotors 中 yaw 仿照 pitch，HoldPosition 失败才 Stop。保留既有使能与电流输出门控，保持滑块托持规则。
3. `src/hw/launcher_hw.hpp` 及 `.cpp`：在监控快照增加 yaw 指令、反馈转矩/转速；从控制线程按既有锁/快照方式复制。监控线程按既有节奏增加 `Dart yaw sl=...mm cmd=... act=... rpm=...`。sl 用轴 Position，不经 Angle 夹紧；不得说未回零的 sl 是绝对机械位置。
   - 当前驱动将 raw_current 乘 `0.005*10/16384` 存为 torque，反算显示原反馈数值须用逆运算 `torque/0.005/10*16384`。这是适配当前驱动的逆变换，不据此宣称硬件电流已校准；不改 RMMotor 库。
   - 保留 cmd 原始命令量程10000；不新增温度显示。监控行沿用安全格式和间隔，不以推算的字节数/耗时替代构建和现场测量。
4. `test/host_test.cpp`：更新安装断言；新增有意义的 yaw 回归：回零方向/距离上限、手动松手取当前位置保持、切模式保持目标后到位判定、未回零停止、超程故障。已有 pitch/滑块托持和离线回归应通过；不要复制一套实际未被适配层使用的配置却声称覆盖了适配层。
5. `README.md`：接线、安装状态、手势、诊断行与未标定几何、pitch/yaw 同时回零说明。

集成文件仅在获准本地提交后：`dart-fw/Modules/modules.yaml`（仅工具要求更新精确模块引用时）、`dart-fw/xrobot.lock`（仅工具维护）。使用工具创建的隔离工作区，分支名 `auto/4j-yaw-m2006`；源模块和固件两个仓库分别记录完整SHA。

交付记录由主对话维护：本文、state.json、worker/reviewer 报告，及 handoff 当前状态/下一步。没有完成现场验收就不能写已验收。

未经另行批准不得改：Safety/LaunchSequence/MatchFlow/ModeLogic、MotorAxis 通用算法、YawGeometry、CAN BSP、Referee、拉力驱动/发射条件、摆臂及推杆的 false 开关、当前 YAML 总线与方向配置。

## worker 执行步骤

1. 读本文、state.json、用户闸口答案及目标仓 AGENTS。先核对 status/remote/HEAD，检查是否已有本任务工作区，避免重复启动。只做本地交付，不能push/创建PR。
2. 由 Orca 管理的隔离工作区实现，保留当前4g配置与Referee补丁作为构建输入，逐一记录其来源和哈希；不能通过reset/clean或重克隆丢弃它们。若工作区无法安全带入输入，停下报告。
3. 按授权清单实现，跑主机回归与依赖扫描。
4. 获准本地提交时先检查 status、diff、git log --oneline -10，仅提交本任务文件；两个仓库分开操作。同步前备份当前4i ELF到不存在的新路径，核验源哈希 `C2A54393EEF4D721E73F1400F02749BF3977B649D5DECF71CC04B9696FD1B41E`，若不同先记录调查，不覆盖备份。
5. 在隔离固件工作区使用 `xrobot setup --update local/DartLauncher`，核对副本的SHA与源模块一致。工具拒绝更新、路径仍指原分支旧提交或要求删除模块时停下报告；不手改lock、不静默换用旧源码。未获准提交时只交付diff与主机结果，不冒充完成固件同步。
6. 编译真实包含新代码的完整固件，记录ELF绝对路径与SHA-256、构建输出、新/旧警告及模块来源。不能覆盖用户正在用的原工作区ELF，不烧录。
7. 报告 `plans/tasks/4j-yaw-m2006-worker-r1.md`，说明隔离工作区、基线/新SHA、改动文件、命令/退出码/结果、现有改动保留证据与现场待测事项。卡住就停下汇报，不要自己乱改。

## 可执行门禁命令

以下 working directory 指 worker 的源模块隔离根目录，执行前确认 build-host 父目录；不存在时仅创建本地构建输出目录。

```powershell
$env:PATH = 'C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;' + $env:PATH
$src = (Get-ChildItem -LiteralPath src -Filter '*.cpp').FullName
g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test/host_test.cpp $src -o build-host/host_test_4j.exe
.\build-host\host_test_4j.exe
g++ -std=c++17 -Wall -Wextra -Werror -O1 -Itest/hx711_stubs -Isrc/hw test/hx711_port_test.cpp src/hw/hx711_port.cpp -o build-host/hx711_port_test_4j.exe
.\build-host\hx711_port_test_4j.exe
rg -n '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define' src -g '*.hpp' -g '*.cpp' -g '!hw/**'
```

每条编译成功才运行对应新二进制；两组测试都应PASS。纯逻辑扫描不包含hw，正常无匹配（rg退出码1）；发现已有命中须区分新增违规。不要把无匹配误认为测试失败。

以下 working directory 指 worker 的固件隔离根目录，同步新模块成功后执行：

```powershell
xrobot format --check
cmake --build --preset debug
Get-FileHash -LiteralPath build/debug/DevC.elf -Algorithm SHA256
```

若隔离目录未配置build preset，按同工程现有工具链先 `cmake --preset debug`，不得安装新工具链或改CMake选型来凑通过。ARM适配层必须由真实完整构建验证。验证新模块源码与被构建副本的文件哈希一致。已知旧警告按 handoff 记录；本次模块不得新增警告。

## reviewer 与交付

主对话提供本文、用户确认记录、分支/两仓SHA、worker报告、实际diff及输出，委派 reviewer 第1轮，报告 `plans/tasks/4j-yaw-m2006-review-r1.md`。重点检查切模式/无力门控没有变成意外带电、到位判定保持正确、CAN分组、回零上限、诊断逆换算及构建来源真实性。
APPROVED 才完成本地交付；P0停止；仅P1/P2首次可返修一次，复审第2轮仍CHANGES_NEEDED则停止请用户决策。不得替reviewer分级，不发布PR。

## 现场验收

先给用户一阶段步骤，依次是无力查看在线与读数、短促点动确认回零方向、确认挡块并将pitch置于上端附近后两轴回零、小范围左右微调与松手保持，再验证切自动到位和4f回零相容性。无力时yaw按原门控清电流；不能把位置目标记忆说成无力时持续通电托住。
现场几何未核准不跑满软件角范围。首次不带镖实打。烧录会复位松开电机，按工程既有条件先退膛、避开滑块托持态；记录实测和现场日志后再决定是否调整方向、几何、速度或电流。

## 需人工确认处（派 worker 前）

1. 新装机构是否仍是旧记录的“一端为0°、从这一端往另一边转到12°”，并仍用4 mm导程丝杠？若实际为正中0°、左右各转，则本方案机械模型不适用，须获准修订后才能实现。
2. 是否同意本轮一并实现松手停在当前位置、切自动仍记住当前位置、回零搜索距离限制和新增读数，并接受现有pitch/yaw同时回零手势？建议同意。
3. 是否授权worker为本任务在隔离分支做本地git提交，以便同步模块并交付可追踪的新固件？仅本地，不push。未授权不提交。
