# 4g 任务简报（planner 输入）

日期：2026-11-23　状态：待规划
基线：DartLauncher `5f758fe`（分支 codex/apply-load-position-calibration）/
dart-fw `c154c46`（分支 dart-launcher），两仓工作区干净。

## 本轮边界

- **先不发射**；结构需机械优化，软件不碰发射相关逻辑。
- 继续做拉力传感器；本阶段只读规划，不改代码、不烧录。
- 按 planner → worker → reviewer 三角色执行。

## 硬件（已锁定，不要重新设计）

- 两台 24V 485 绿板称重变送器并联 → 1 块 EKT010（自动收发，3.3–5V）
- → C 板丝印 UART2 = STM32 USART1，PA9=TX / PB7=RX（ioc 已锁）
- 站号：左 = 2，右 = 1；实测 9600-8-N-1
- 寄存器：01=1 小数点、06=2 滤波、07=0(10Hz)、08=0 零点跟踪关、11=0
- 读数：一次读 0x0002 起 2 个寄存器；11=0 时 02=符号、03=无符号显示值
- 帧长：请求 8 字节，正常应答 9 字节，异常应答 5 字节（功能码 0x03）
- 已核 CRC 的帧：右 `01 03 00 02 00 02 65 CB`；左 `02 03 00 02 00 02 65 F8`
- 波特率决策：不改设备，把 `Core/Src/main.c` 的 `MX_USART1_UART_Init` 115200 改 9600（一行）
- 端口冲突：USART1 原计划留给视觉，视觉上车前必须重定，**本轮不改**

## 磁盘现状（唯一进度）

`src\tension_modbus.hpp` 未跟踪，仅声明：`kTensionRightAddr=1`、`kTensionLeftAddr=2`、
`kTensionRegStart=0x0002`、`kTensionRegCount=2`、`Request=8`、`Response=9`、`Exception=5`、
`enum class ModbusParseStatus{Incomplete,Ok,Exception}`、`ModbusCrc16`、
`BuildTensionReadRequest`、`ParseTensionResponse`

缺：`tension_modbus.cpp`、主机测试、hw 适配层、launcher_hw 接入、main.c 波特率、`plans\tasks\4g-*.md`

## 可照抄的既有范式

- `src\hw\hx711_port.hpp/.cpp`：4e 右侧 HX711，监控线程 + 约每秒打印原始值（里程/年龄/计数）
- `src\hw\remote_port.hpp/.cpp`：`LibXR::UART` + `ReadOperation` + 线程/互斥锁的串口封装
- `test\host_test.cpp`（当前 31539 checks）+ `test\hx711_port_test.cpp`（2193 checks）

## 代码规范（违反会被 reviewer 打回）

- `src` 内禁止 LibXR/HAL/FreeRTOS 头文件；一个功能一对 `.hpp/.cpp`；`constexpr` 常量、`enum class`、不加宏
- 控制循环不 new/malloc；`src` 不用 vector/function/异常/RTTI（测试可用）
- 不写防御性代码：只在串口这类外部入口做校验，不为不可能的分支加回退/夹紧
- `XR_LOG_INFO` 一条 ≤64 字节（汉字按 3 字节）；`Printf` 的 `%s` 不能是空串
- 单位 m/rad/s/N/A；方向问题改 RMMotor 的 reverse，不改正负号

## 验证与提交流程

1. 主机测（改了 src 就跑）：
   ```powershell
   $env:PATH = "C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;" + $env:PATH
   cd C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher
   $src = (Get-ChildItem src\*.cpp).FullName
   g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test\host_test.cpp $src -o build-host\host_test.exe
   .\build-host\host_test.exe
   ```
   再跑纯逻辑依赖检查（应无输出）：
   `Select-String -Path src\*.hpp,src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'`
2. 提交（当前分支、行尾 LF）：`git add` 与 `git commit` 分开跑，之后用 `rev-parse` / `branch --show-current` / `status --porcelain` 核对
3. 同步编译：dart-fw 下先备份 ELF → `xrobot setup --update local/DartLauncher` → 核对
   `Modules\local\DartLauncher` 提交等于刚才那个 → `cmake --build --preset debug` →
   `Get-FileHash build\debug\DevC.elf -Algorithm SHA256` → dart-fw 只提交 `xrobot.lock`
- 不新建分支、不 push、不 reset/restore/clean/amend；烧录由用户自己跑
- 资源占用基线：RAM 24.89%、CCRAM 100.00%、FLASH 21.24%

## 期望的分阶段

- **A 纯逻辑**：`tension_modbus.cpp`（CRC16、组请求帧、解析应答/异常/不完整）+ 主机测试用例
  （含左/右两帧、异常帧、截断帧）
- **B 适配层**：`tension_port.hpp/.cpp` 包 `LibXR::UART`，交替轮询站号 1 与 2，超时与异常计数
- **C 接入**：`launcher_hw` 增加左右拉力读数 + 周期打印（照 hx711 格式），`main.c` USART1 改 9600
- **D 收口**：补写 `plans\tasks\4g-rs485-tension-readout.md`、更新 `handoff.md`、reviewer 审一轮

## 现场待验证

空载零点；手拉拉皮筋方向与数值（左 2 号、右 1 号谁对应哪侧要实测）；A/B 若全无应答先对调。

## 输出要求

给**文件级**计划：每个阶段列到"改哪个文件、加哪些函数/常量、测试加哪些用例"，
并标注风险点与需要人工确认的地方。不要改任何文件。
