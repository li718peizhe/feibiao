# 任务 4g 实现计划：RS485 双路拉力变送器读数接入

- **基线**：`xrobot-local/DartLauncher` @ `5f758fe`（分支 `codex/apply-load-position-calibration`），`dart-fw` @ `c154c46`（分支 `dart-launcher`）
- **边界**：不触碰任何发射/控制/安全状态机逻辑；不改 `DevC.ioc` 与变送器波特率；不重定 USART1 视觉端口冲突；不新建分支、不 push、不烧录。
- **既有代码现状核实**：
  - `xrobot-local/DartLauncher/src/tension_modbus.hpp` 已存在但未跟踪，已声明站号、寄存器、帧长、状态枚举与三个函数原型。
  - `xrobot-local/DartLauncher/CMakeLists.txt` 已用 `file(GLOB ... src/*.cpp src/hw/*.cpp)` 自动收集源码，新增 `.cpp` 无需修改 `CMakeLists.txt`。
  - `dart-fw/User/app_main.cpp`（L51–52、L115、L212）已实例化并 `XR_REGISTER(usart1, LibXR::UART)`（128 B 收发缓冲），无需改 `app_main.cpp` 或 `libxr_config.yaml`。
  - `dart-fw/User/RobotConfig/artisans_dart.yaml`（L67–88）尚未把 `usart1` 传给 `dart_launcher`，且 `DartLauncher.hpp` 与 `launcher_hw.hpp/.cpp` 尚未接收第二路 UART 引用。
  - `plans/tasks/4g-rs485-tension-readout.md` 当前内容为无关的适配器启动日志（588 B），需在收口阶段覆写。**【2026-11-24 状态注：此句是规划时的历史快照，已失效。本文件现已恢复为正式计划全文，D 阶段不要覆写本文件，只需更新 `handoff.md` 与验收记录。】**

---

## 阶段 A：纯逻辑层（Modbus RTU 编解码与主机测试）

### 步骤 A1：补全纯逻辑头文件
- **修改文件**：`xrobot-local/DartLauncher/src/tension_modbus.hpp`
- **改什么**：
  - 保留既有常量：`kTensionRightAddr = 1`、`kTensionLeftAddr = 2`、`kTensionRegStart = 0x0002`、`kTensionRegCount = 2`、`kTensionRequestSize = 8`、`kTensionResponseSize = 9`、`kTensionExceptionSize = 5`。
  - 新增协议常量（`constexpr`）：`kTensionFuncRead = 0x03`、`kTensionFuncException = 0x83`、`kTensionResponseDataBytes = 4`。
  - 在 `enum class ModbusParseStatus : std::uint8_t` 中补充 `Invalid`（即 `{Incomplete, Ok, Exception, Invalid}`）。
  - 保留既有三个函数声明：`ModbusCrc16`、`BuildTensionReadRequest`、`ParseTensionResponse`；并可补充可选输出或独立重载（如不改签名，保持四参数 `ParseTensionResponse(data, length, expected_addr, value)` 即可）。
- **为什么**：当前三态枚举无法区分「字节尚未收齐（`Incomplete`）」、「变送器返回合法 Modbus 异常帧（`Exception`）」与「CRC 错误 / 站号不匹配 / 功能码或字节数非法 / 符号寄存器非 0 或 1（`Invalid`）」。加入 `Invalid` 后，串口接收层可在收到错误帧头或坏帧时立刻终止等待并准确区分异常计数与校验错误计数。

### 步骤 A2：实现 Modbus RTU 纯逻辑
- **新增文件**：`xrobot-local/DartLauncher/src/tension_modbus.cpp`
- **改什么**：
  - 仅包含 `"tension_modbus.hpp"` 及标准库定长头，严禁引入 LibXR/HAL/FreeRTOS 头文件与 `#define`。
  - 实现 `std::uint16_t ModbusCrc16(const std::uint8_t *data, std::size_t length)`：初值 `0xFFFF`、多项式 `0xA001` 逐位右移计算；`data == nullptr || length == 0` 返回 `0xFFFF`。
  - 实现 `std::array<std::uint8_t, kTensionRequestSize> BuildTensionReadRequest(std::uint8_t addr)`：按 `[addr, 0x03, hi(0x0002), lo(0x0002), hi(2), lo(2), crc_lo, crc_hi]` 组装 8 字节请求帧（CRC 低字节在前）。
  - 实现 `ModbusParseStatus ParseTensionResponse(const std::uint8_t *data, std::size_t length, std::uint8_t expected_addr, std::int32_t &value)`：
    1. `data == nullptr || length < 2` 返回 `Incomplete`。
    2. 校验 `data[0] == expected_addr`，不符返回 `Invalid`。
    3. 若 `data[1] == kTensionFuncException`（`0x83`）：`length < 5` 返回 `Incomplete`；校验前 3 字节 CRC 与 `data[3..4]`（低字节在前），不符返回 `Invalid`，相符返回 `Exception`（不改 `value`）。
    4. 若 `data[1] == kTensionFuncRead`（`0x03`）：`length < 9` 返回 `Incomplete`；校验 `data[2] == 4` 及前 7 字节 CRC 与 `data[7..8]`，不符返回 `Invalid`；解析寄存器 `02`（`sign = (data[3] << 8) | data[4]`）与寄存器 `03`（`mag = (data[5] << 8) | data[6]`）；仅当 `sign == 0 || sign == 1` 时计算 `value = (sign == 1) ? -static_cast<std::int32_t>(mag) : static_cast<std::int32_t>(mag)` 并返回 `Ok`，否则返回 `Invalid`。
    5. 其余功能码直接返回 `Invalid`；所有非 `Ok` 分支均不修改 `value`。
- **为什么**：在外部串口入口集中完成帧边界、站号、CRC16 与 `11=0` 符号/无符号显示值解码，保持纯逻辑可主机全覆盖测试。

### 步骤 A3：补充主机单元测试
- **修改文件**：`xrobot-local/DartLauncher/test/host_test.cpp`
- **改什么**：
  - 头部（L2–L18 附近）新增 `#include "tension_modbus.hpp"`。
  - 新增 `void TestTensionModbus()`，并在 `main()`（L5276 `TestAutomaticHomeStartRegression();` 后）调用。
  - 新增测试用例：
    1. **CRC16 校验**：标准向量 `"123456789"` → `0x4B37`；空缓冲 → `0xFFFF`。
    2. **左右两路请求帧精确字节比对**：
       - `BuildTensionReadRequest(kTensionRightAddr)` == `{0x01, 0x03, 0x00, 0x02, 0x00, 0x02, 0x65, 0xCB}`
       - `BuildTensionReadRequest(kTensionLeftAddr)` == `{0x02, 0x03, 0x00, 0x02, 0x00, 0x02, 0x65, 0xF8}`
    3. **正常应答解析**：
       - 右路站号 1 正值（如 `02=0, 03=1000` → `+1000`）
       - 左路站号 2 负值（如 `02=1, 03=500` → `-500`）
       - 零值（`02=0, 03=0` → `0`；`02=1, 03=0` → `0`）
       - 满量程边界（`02=0, 03=65000` → `+65000`；`02=1, 03=65535` → `-65535`，验证无符号 16 位转有符号 32 位不溢出）
    4. **异常应答帧（5 字节）**：站号 1/2 功能码 `0x83` + 合法 CRC 返回 `Exception`，且不覆写 `value`。
    5. **截断 / 不完整帧**：`nullptr`、长度 `0..1`、正常应答前缀长度 `2..8`、异常应答前缀长度 `2..4` 均返回 `Incomplete`，且不覆写 `value`。
    6. **损坏 / 非法帧**：正常帧 CRC 翻转 1 bit、异常帧 CRC 翻转 1 bit、站号错配（期望 1 收到 2）、功能码非法（如 `0x06`）、字节数非 4（如 `data[2]=2`）、符号寄存器非 0/1（如 `0x0002`）均返回 `Invalid`，且不覆写 `value`。
- **自测**：编译运行 `host_test.exe`（检查数应大于基线 31539 且全部通过），并运行 `src\*.hpp,src\*.cpp` 纯逻辑依赖正则检查确认 0 命中。

---

## 阶段 B：LibXR 串口适配层（RS485 轮询驱动）

### 步骤 B1：定义 `TensionPort` 接口与快照结构
- **新增文件**：`xrobot-local/DartLauncher/src/hw/tension_port.hpp`
- **改什么**：
  - 参照 `src/hw/remote_port.hpp` 与 `src/hw/hx711_port.hpp` 的命名与并发封装模式，在 `namespace dart` 中定义：
    - `struct TensionChannel`：字段 `std::int32_t raw = 0`、`std::uint32_t samples = 0`、`std::uint32_t timeouts = 0`、`std::uint32_t exceptions = 0`、`std::uint32_t errors = 0`、`std::uint32_t age_ms = 0`、`bool valid = false`。
    - `struct TensionSnapshot`：字段 `TensionChannel left{}`（站号 2）、`TensionChannel right{}`（站号 1）。
    - `class TensionPort`：
      - `explicit TensionPort(LibXR::UART &uart);`
      - `TensionSnapshot Read();`（线程安全，供 `OnMonitor()` 读取快照并计算实时 `age_ms`）。
      - 私有成员：`LibXR::UART &uart_`、`LibXR::Thread thread_`、`LibXR::Semaphore tx_sem_{0}`、`LibXR::Semaphore rx_sem_{0}`、`LibXR::WriteOperation tx_op_`、`LibXR::ReadOperation rx_op_`、`LibXR::Mutex mutex_`、左右两路内部状态及 `received_ms_`。
      - 私有函数：`static void ThreadMain(TensionPort *self)` 与单站轮询辅助 `void PollStation(std::uint8_t addr, ChannelState &channel)`。
- **为什么**：将 RS485 半双工阻塞收发隔离在独立低优先级线程中，避免阻塞 1 kHz 控制线程或每秒监控线程，并对外提供与 `Hx711Port` 一致的快照读取语义。

### 步骤 B2：实现 `TensionPort` 交替轮询与超时/异常统计
- **新增文件**：`xrobot-local/DartLauncher/src/hw/tension_port.cpp`
- **改什么**：
  - 模块内 `constexpr` 时序参数：
    - `kTensionBaudRate = 9600`
    - `kTxTimeoutMs = 30`（8 字节 @ 9600 bps 传输约 8.3 ms）
    - `kRxChunkTimeoutMs = 10`（单次读已就绪字节的阻塞上限）
    - `kResponseTimeoutMs = 80`（单站应答总超时，覆盖 10 Hz AD 响应与 9 字节接收时间）
    - `kTurnaroundMs = 15`（两站之间静默间隔，大于 9600 bps 下 Modbus 3.5 字符静默 3.65 ms 与 EKT010 自动收发切换余量）
    - `kRxPollStepMs = 3`（等待应答时轮询 `read_port_->Size()` 的休眠步长）
    - `kThreadStackBytes = 1024`（线程名 `"dart_tension"`，优先级 `LibXR::Thread::Priority::LOW`）
  - 构造函数：
    - 初始化 `tx_op_(tx_sem_, kTxTimeoutMs)` 与 `rx_op_(rx_sem_, kRxChunkTimeoutMs)`。
    - 调用 `uart_.SetConfig({kTensionBaudRate, LibXR::UART::Parity::NO_PARITY, 8, 1});`（确保串口配置为 9600-8-N-1）。
    - 启动线程 `thread_.Create(this, ThreadMain, "dart_tension", kThreadStackBytes, LibXR::Thread::Priority::LOW);`。
  - `ThreadMain` 循环：
    - 交替调用 `PollStation(kTensionRightAddr, right_)` → `LibXR::Thread::Sleep(kTurnaroundMs)` → `PollStation(kTensionLeftAddr, left_)` → `LibXR::Thread::Sleep(kTurnaroundMs)`。
  - `PollStation(addr, channel)` 流程：
    1. 发送前调用 `uart_.read_port_->ClearQueuedData()` 清空残留噪声字节。
    2. 调用 `BuildTensionReadRequest(addr)` 组 8 字节请求帧，`uart_.Write({req.data(), req.size()}, tx_op_)`；若写失败，记 `++timeouts`（或 `++errors`）后返回。
    3. 在 `kResponseTimeoutMs` 窗口内循环检查 `avail = uart_.read_port_->Size()`：
       - 若 `avail > 0`，读入 `std::min(avail, buf.size() - received)` 字节到定长 `std::array<std::uint8_t, kTensionResponseSize> buf{}`；
       - 每次增量收数后调用 `ParseTensionResponse(buf.data(), received, addr, parsed)`：
         - `Ok`：加锁更新 `channel.raw = parsed`、`++channel.samples`、`channel.received_ms = now`、`channel.valid = true`，结束本次轮询；
         - `Exception`：加锁 `++channel.exceptions`，结束本次轮询；
         - `Invalid`：清接收队列，加锁 `++channel.errors`，结束本次轮询；
         - `Incomplete`：继续等待直到超时。
       - 无新字节时 `LibXR::Thread::Sleep(kRxPollStepMs)`。
    4. 超时未完成时清接收队列，加锁 `++channel.timeouts`。
  - `Read()`：加锁复制左右两路状态，按 `now_ms - received_ms`（无符号差值防 32 位回绕；`samples == 0` 时 `age_ms = 0`）计算两路 `age_ms` 后返回 `TensionSnapshot`。
- **为什么**：
  - 逐块读入并调用状态机解析，能正确识别 **5 字节异常帧** 与 **9 字节正常帧**，不会因硬等 9 字节而把异常帧误判为超时。
  - 独立低优先级线程确保绝不抢占 `dart_control`（`HIGH`）、`dart_dbus`（`HIGH`）和 `referee`（`MEDIUM`）；单站掉线超时不影响另一站正常轮询。
  - 全程定长栈数组、无动态内存分配、不在轮询线程打日志。

---

## 阶段 C：模块接入与固件配置

### 步骤 C1：在 `LauncherHardware` 中接入 RS485 拉力读数与周期打印
- **修改文件**：
  - `xrobot-local/DartLauncher/src/hw/launcher_hw.hpp`
  - `xrobot-local/DartLauncher/src/hw/launcher_hw.cpp`
- **改什么**：
  - `launcher_hw.hpp`：
    - 新增 `#include "tension_port.hpp"`。
    - `LauncherHardware` 构造函数（L25–L32）末尾新增参数 `LibXR::UART &tension_uart`。
    - 私有成员（L93 `Hx711Port tension_right_;` 附近）新增 `TensionPort tension_rs485_;` 及监控线程专用的上次样本计数 `std::uint32_t printed_tension_left_ = 0, printed_tension_right_ = 0;`（用于输出与 HX711 一致的 `new=%d` 字段）。
  - `launcher_hw.cpp`：
    - 构造函数（L78–L96）接收 `LibXR::UART &tension_uart` 并在初始化列表中初始化 `tension_rs485_(tension_uart)`。
    - `OnMonitor()` 末尾（L636 之后）新增读取与分两行打印（与 USB CDC 128 B 缓冲习惯一致，每行前 `LibXR::Thread::Sleep(20)`）：
      - 调用 `const auto rs485 = tension_rs485_.Read();`
      - 计算 `left_new = (rs485.left.samples != printed_tension_left_)`、`right_new = (rs485.right.samples != printed_tension_right_)` 并更新记录。
      - 输出两行（每行实测约 55–65 字节，远小于 128 B 缓冲，不含空 `%s`，类型严格强转 `int`/`unsigned`）：
        - `Dart rs485 L raw=%d new=%d n=%u to=%u ex=%u err=%u age=%ums\r\n`
        - `Dart rs485 R raw=%d new=%d n=%u to=%u ex=%u err=%u age=%ums\r\n`
    - **不修改 `Tick()` 及任何发射/安全/模式逻辑**；默认保留既有 `Hx711Port tension_right_` 与 `test/hx711_port_test.cpp`（是否从 `OnMonitor()` 移除旧 HX711 打印见下方人工确认项 2）。
- **为什么**：监控线程每秒稳定输出左右两路原始显示值及通信诊断计数器（成功/超时/异常/错帧/数据年龄），便于现场一次区分“无应答（A/B 反接或波特率）”、“异常码”与“干扰错帧”。

### 步骤 C2：扩展 `DartLauncher` 模块构造入口
- **修改文件**：`xrobot-local/DartLauncher/DartLauncher.hpp`
- **改什么**：
  - `DartLauncher` 构造函数（L16–L23）末尾新增参数 `LibXR::UART &tension_uart`，并透传给 `hardware_(..., tension_uart)`。
- **为什么**：XRobot 代码生成器依据 `DartLauncher.hpp` 唯一公有构造函数的参数名与顺序绑定 `artisans_dart.yaml` 中的外设对象。

### 步骤 C3：调整 `dart-fw` USART1 初始化波特率为 9600
- **修改文件**：`dart-fw/Core/Src/main.c`
- **改什么**：
  - `MX_USART1_UART_Init()`（L938）将 `huart1.Init.BaudRate = 115200;` 改为 `huart1.Init.BaudRate = 9600;`（仅改此一行）。
- **为什么**：两台变送器已实测锁定 9600-8-N-1，且 PA9/PB7 引脚与 DMA 已在 `DevC.ioc` / `stm32f4xx_hal_msp.c` 中就绪；改固件一行成本最低，与 `TensionPort` 构造中的 `SetConfig` 保持一致。

### 步骤 C4：在机器人配置中将 `usart1` 绑定给 `dart_launcher`
- **修改文件**：`dart-fw/User/RobotConfig/artisans_dart.yaml`
- **改什么**：
  - 在 `id: dart_launcher` 的 `args:` 列表末尾（L88 `- magnet_pwm: pwm_tim1_ch3` 之后）新增一行：`- tension_uart: usart1`。
- **为什么**：`User/xrobot_main.hpp` 由 `xrobot gen` / `xrobot setup` 自动生成，当前 `XRobotMain` 未引入 `usart1`；必须在配置中声明该参数绑定，生成代码才能把已注册的 `usart1` 传入 `DartLauncher`。

### 步骤 C5：提交源码仓并完成 `dart-fw` 同步与编译验证
- **执行操作**（不新建分支、不 push、不 reset/restore/clean/amend）：
  1. 在 `xrobot-local/DartLauncher` 中确认新增/修改文件行尾均为 LF，运行主机回归（`host_test.exe` + `hx711_port_test.exe`）与纯逻辑依赖检查。
  2. 分开执行 `git add` 与 `git commit` 提交源码仓，记录新 commit SHA，用 `git rev-parse HEAD` / `git branch --show-current` / `git status --porcelain` 核对。
  3. 在 `dart-fw` 中先备份当前 4f 固件 `build/debug/DevC.elf` 为 `build/DevC-c154c46-before-4g.elf`。
  4. 执行 `xrobot setup --update local/DartLauncher`（必要时跟 `xrobot gen -c User/RobotConfig/artisans_dart.yaml`），核对 `Modules/local/DartLauncher` HEAD 等于刚才源码提交、同步副本内主机回归与 `xrobot format --check` 均通过，且 `User/xrobot_main.hpp` 已生成 `usart1` 绑定（`xrobot_main.hpp` 不提交）。
  5. 执行 `cmake --build --preset debug`，核对 RAM / CCRAM / FLASH 占用并计算新 `build/debug/DevC.elf` 的 SHA-256，确认符号表含 `dart::TensionPort` 与 `dart::ParseTensionResponse` 且 `Modules/QDU-Robomaster/Referee` 既有 3 处补丁原封不动。

---

## 阶段 D：文档收口与代码审查

### 步骤 D1：补写 4g 任务文档
- **修改文件**：`plans/tasks/4g-rs485-tension-readout.md`（覆写当前 588 B 无关日志）
- **改什么**：参照 `plans/tasks/4e-right-hx711-readout.md` 与 `4f-delayed-lift-auto-home.md` 格式，写清目标边界、已锁硬件与协议要点、改动文件列表、主机与编译验证记录（含 checks 数量、资源占用、新旧 ELF SHA-256、两仓提交号）、现场待验证清单与停止条件。
- **为什么**：保证任务文档可追溯且替代损坏占位文件。

### 步骤 D2：更新交接文档
- **修改文件**：`handoff.md`
- **改什么**：更新「当前状态」、状态总表（`DartLauncher` / `dart-fw` / `固件 4g` / 拉力传感器条目）、「下一步」及「提交记录」表格，明确记录 4g 镜像未烧录及 USART1 临时占用待视觉上车前重定。
- **为什么**：保持跨会话上下文准确，避免后续任务基于旧 4f 基线误操作。

### 步骤 D3：`dart-fw` 提交与审查
- **执行操作**：
  1. 在 `dart-fw` 中提交本轮必需的追踪改动（详见人工确认项 1：`Core/Src/main.c`、`User/RobotConfig/artisans_dart.yaml`、`xrobot.lock`；不提交生成的 `User/xrobot_main.hpp` 与 `Referee` 既有本地补丁），核对 `git status --porcelain` 与分支名。
  2. 交由 `reviewer` 按代码规范、安全边界、串口协议与并发安全进行审查；若结论为 `CHANGES_NEEDED` 则回交 `worker` 修复后复测复审。

---

## 风险点

1. **FreeRTOS 运行时堆（CCRAM）余量风险**：当前链接基线显示 `CCRAM 100.00%`（FreeRTOS 静态堆占满 64 KB CCMRAM），新增 `dart_tension` 线程（栈 1024 B + TCB）会从 FreeRTOS 运行时堆分配。若堆余量紧张可能导致线程创建失败或静默无法调度，需在编译后检查堆配置及上电后确认 `to`/`n` 计数器确实在递增（证明线程已成功启动）。
2. **`dart-fw` 提交范围与简报口径冲突**：简报既要求改 `Core/Src/main.c`（USART1 改 9600）并把串口接入 `launcher_hw`（必须改 `User/RobotConfig/artisans_dart.yaml` 传入 `usart1`），末尾又写「dart-fw 只提交 `xrobot.lock`」。若漏提 `main.c` 与 `artisans_dart.yaml`，仓库状态会不干净或切分支丢配置。
3. **CubeMX 重新生成覆盖风险**：仅修改 `Core/Src/main.c` L938 而未改 `DevC.ioc`，日后若重新用 CubeMX 生成代码，`MX_USART1_UART_Init` 会回退为 `115200`。通过在 `TensionPort` 构造函数中显式调用 `uart_.SetConfig({9600, NO_PARITY, 8, 1})` 可缓解此风险，但仍需在文档中标注。
4. **STM32 UART DMA + `ClearQueuedData` / `SetConfig` 交互细节**：`STM32UART` 采用 `HAL_UARTEx_ReceiveToIdle_DMA`，9600 bps 下帧尾靠 IDLE 中断推入 `read_port_`。若 `Write` 刚结束变送器应答极快，不能在 `Write` 之后再调用 `ClearQueuedData()`，必须**在 `Write` 之前**清空队列；同时单站应答可能拆成两段进入 `read_port_`，必须按增量拼接并反复调用 `ParseTensionResponse`，切勿单次 `Read(9)` 死等（否则 5 字节异常帧必然超时）。
5. **EKT010 自动收发方向切换与总线上电回声/干扰**：半双工 RS485 在两站切换间隙过短易发生总线碰撞；若 EKT010 或接线存在本地回显，接收缓冲会先看到刚发的 8 字节请求帧导致 `ParseTensionResponse` 误判。需留足 `kTurnaroundMs = 15 ms` 间隔，并在现场首次上电观察 `err` 与 `to` 计数定位。
6. **旧 HX711 驱动在拆线后空读风险**：若物理上已拆除 PB12/PB13 上的 HX711，但 `OnMonitor()` 仍调用 `tension_right_.Read()`，当 PB13 浮空为低时会短暂关中断打 25 个时钟并打印无意义 `raw` 值；反之若直接删除 `hx711_port.*` 会影响 `hx711_port_test.cpp`（2193 checks）回归。

---

## 需要人工确认的地方

1. **`dart-fw` 本轮提交文件清单**（必确认）：
   - 接入 `usart1` 必然产生三处 `dart-fw` 追踪文件变更：`Core/Src/main.c`（波特率改 9600）、`User/RobotConfig/artisans_dart.yaml`（新增 `tension_uart: usart1`）和 `xrobot.lock`（更新 `local/DartLauncher` 提交号）。
   - **建议**：在 `dart-fw` 本次提交中一并提交这 3 个文件（仍不提交生成的 `User/xrobot_main.hpp`，仍保留 `Referee` 既有本地补丁）。请确认是否按此执行，而非仅提交 `xrobot.lock`。
2. **既有右侧 HX711（`tension_right_`）的处理方式**：
   - **选项 A（推荐）**：保留 `src/hw/hx711_port.hpp/.cpp` 与 `test/hx711_port_test.cpp`（确保 2193 checks 完整回归），但在 `LauncherHardware` 中**保留还是停用** `tension_right_.Read()` 打印？推荐本轮暂保留不动、额外新增两行 `Dart rs485 L/R ...`（零侵入）；若 PB12/PB13 硬件已完全拆线且不希望日志多一行，也可改为在 `LauncherHardware` 中移除 `tension_right_` 成员仅保留文件与单测。
3. **`ModbusParseStatus` 枚举是否允许新增 `Invalid` 成员**：
   - 当前未跟踪的 `src/tension_modbus.hpp` 仅定义了 `{Incomplete, Ok, Exception}`。
   - **建议**：扩展为 `{Incomplete, Ok, Exception, Invalid}`，以准确区分 Modbus 协议异常帧与 CRC/站号/格式损坏帧。若希望冻结 `tension_modbus.hpp` 现有声明不增枚举值，请告知（届时损坏帧将只能归并处理）。
4. **监控打印格式确认**：
   - 计划采用分两行输出（每行约 60 字节，避开 USB CDC 128 B 截断）：
     `Dart rs485 L raw=%d new=%d n=%u to=%u ex=%u err=%u age=%ums`
     `Dart rs485 R raw=%d new=%d n=%u to=%u ex=%u err=%u age=%ums`
   - 其中 `raw` 为变送器有符号原始显示整数（小数点 `01=1` 时对应 0.1 单位，与 `hx711` 阶段一致暂不做牛顿换算，待现场标定后再换算为 N）。请确认字段与命名是否符合日志分析习惯。

---

## 验收标准

### 1. 主机测试与规范检查（阶段 A、C 均须通过）
- `xrobot-local/DartLauncher`（源码仓及 `dart-fw/Modules/local/DartLauncher` 同步副本）编译运行 `host_test.exe` 通过（`-std=c++17 -Wall -Wextra -Werror -O1`），检查总数在基线 `31539` 之上新增覆盖全部 Modbus 正常/异常/截断/损坏用例，0 失败。
- `hx711_port_test.exe` 继续通过 `PASS: 2193 HX711 checks`。
- 纯逻辑依赖扫描无任何命中：
  `Select-String -Path src\*.hpp,src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'` 输出为空。
- `xrobot format --check` 退出码为 0；改动文件行尾均为 LF；`src/` 下无动态分配、无 STL 动态容器/异常/RTTI。

### 2. 固件构建与仓库状态验收（阶段 C、D）
- 4f 原镜像已备份至 `dart-fw/build/DevC-c154c46-before-4g.elf` 且 SHA-256 与基线 `7A2D2584...367A44D` 一致。
- `xrobot setup --update local/DartLauncher` 后，`Modules/local/DartLauncher` HEAD 与源码仓新提交完全一致。
- `cmake --build --preset debug` 零警告/零错误完成，产出 `dart-fw/build/debug/DevC.elf` 并记录新 SHA-256 与 RAM/CCRAM/FLASH 占用。
- `DartLauncher` 与 `dart-fw` 均位于原分支（无新建分支、未 push），`Referee` 3 处本地补丁原样保留。
- `plans/tasks/4g-rs485-tension-readout.md` 与 `handoff.md` 已更新且通过 `reviewer` 审查（`APPROVED`）。

### 3. 现场上电验收（用户自行烧录后核对）
- 烧录前确认未上膛、滑块未托持受力机构；上电无力模式下观察 USB 串口日志：
  - 每秒出现 `Dart rs485 L ...` 与 `Dart rs485 R ...` 两行，无截断、无乱码，1 kHz 控制周期 `tick`/`late` 无异常升高。
  - **通信正常判据**：两路 `n` 持续递增、`new=1`、`age` 稳定在几十毫秒内、`to`/`ex`/`err` 保持 0 或偶发极低。
  - **排障判据**：若两路均 `n=0` 且 `to` 持续增长，优先对调 RS485 A/B 线；若仍无应答，核查 EKT010 供电（丝印 `UART2` 5 V 改接 8 针口第 4 脚 3.3 V）及 TX/RX 交叉。
  - **方向与站号实测**：记录两路空载零点；分别轻拉左/右皮筋，确认站号 2 对应左侧、站号 1 对应右侧，并记录受拉时的正负号与变化幅值供后续标定使用。
- **停止条件**：出现控制线程异常延迟（`late` 持续增加）、整板复位/卡死、串口日志乱码卡顿或机构任何非预期动作，立即断电停止并回退至 `build/DevC-c154c46-before-4g.elf`。
