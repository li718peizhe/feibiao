# 任务 4e：右侧 HX711 原始读数

## 目标与范围

用户已接右侧传感器及 HX711，要求编写代码查看数据。右侧模块 VCC 接 8 针口第 4 针 3.3 V，GND 接第 2 针，SCK 接第 1 针 PB12，DT 接第 3 针 PB13。按 A 通道、128 倍增益读取有符号 24 位原始值，每轮 USB 监控输出本次是否更新、读取次数及距上次读数的时间。

不读 `plans/dart-fw-libxr.md`，不运行 graphify。仅增加读数，不修改轴控制、发射、装填、标定参数，不烧录、不推送。

## 计划

- [x] 检查两个仓库及模块副本状态、接线与引脚占用，运行现有回归基线。
- [x] 增加右侧 HX711 驱动、USB 状态行和实际驱动的主机协议测试。
- [x] 验证驱动与现有回归，完成 ARM 编译、源码提交和模块同步。
- [x] 完整构建固件，核对 ELF、提交固件版本引用，更新交接与现场查看步骤。

## 文件

- `xrobot-local/DartLauncher/src/hw/hx711_port.hpp/.cpp`：PB12/PB13 GPIO、非阻塞就绪检查、24 位补码、25 个读时钟。
- `xrobot-local/DartLauncher/src/hw/launcher_hw.hpp/.cpp`：监控线程持有驱动并读取、输出，控制线程不调用。
- `xrobot-local/DartLauncher/test/hx711_port_test.cpp`、`test/hx711_stubs/`：以 GPIO/时间替身运行生产驱动，检查时序、符号和未就绪行为。
- `xrobot-local/DartLauncher/README.md`：右侧接线、数据含义、测试命令。
- `dart-fw/Modules/modules.yaml`、`dart-fw/xrobot.lock`：通过 XRobot 同步固定新模块版本。
- 本任务与 `handoff.md`：记录验证结果、构建产物与待现场检查。

## 验收与实现约束

- PB12 为推挽时钟输出，PB13 为上拉输入；不连接模块时 DT 保持未就绪。
- 上电通过 SCK 高电平至少 60 µs 后拉低复位，选择默认 A 通道、128 倍增益。
- DT 高时立即返回，不等待转换；DT 低时按 MSB 优先读取 24 位，第 25 个时钟保持下一次 A/128。
- 时钟高低阶段各至少 1 µs，读取期间保存并恢复中断屏蔽状态，避免任务或中断延长高电平导致休眠。
- `raw` 保留有符号 ADC 原始值；`new=1` 表示这轮读到新值，`n` 是成功读取次数，`age` 是距上次成功读取的毫秒数，`n=0` 表示尚无样本。
- 每次监控最多读取一个样本，约每秒一条；不输出未标定的 N/kg，不自动去皮或滤波。

## 验证记录

- 基线：两个主仓库干净；源码 `322e39b`，固件 `2c76594`，模块副本 `322e39b` 且干净。
- 原固件 ELF SHA-256：`E98DE8C4516CF3836F66AF591E8CC0F530169B259FA6CB99832EA9236F273052`。
- 现有源码回归基线 `PASS: 30834 checks`。
- 完整 build-cmake 脚本列出 debug/relWithDebInfo/release/minSizeRel 预设成功，使用 debug；`--no-save-profile` 避免新增缓存。
- 驱动协议测试 `PASS: 2193 HX711 checks`；新增后现有源码回归 `PASS: 30834 checks`。
- 两个改动的硬件源文件沿用固件编译数据库的 ARM 参数检查通过，开启 `-Werror`。
- 初次 ARM 检查命令 `python -c '... shlex.split(entry["command"]) ...'` 失败：POSIX 分词移除了 Windows 路径反斜杠，`CreateProcess` 报 `FileNotFoundError: [WinError 2]`；改用 `shlex.split(..., posix=False)` 后两个文件返回码均为 0。
- 源码提交 `00e2e1cc9f5deea0c2912d909ee831b11a209725`（`Add right HX711 raw readout`），固件提交 `421745995139fd7acbe605726b80be600e924018`（`Lock right HX711 readout module`）；均在原分支，两个主仓库与同步模块干净，Referee 的三处既有修改保留。
- `xrobot setup --update local/DartLauncher` 成功，副本 HEAD 与源码提交一致；8 个改动文件按 Git 换行转换后内容一致。同步模块协议测试 `PASS: 2193 HX711 checks`，现有回归 `PASS: 30834 checks`，`xrobot format --check` 通过。
- build-cmake 完整配置与构建成功；初次日志转发包装命令 `python -c '... print(result.stdout[-16000:]) ...'` 在显示构建成功输出时因 GBK 不支持 emoji 报 `UnicodeEncodeError`，完整日志已先保存到 `dart-fw/build/4e-right-hx711-build.log`；随后直接用 `python -X utf8` 运行构建脚本返回 0。
- 初次逐字节比较源码与同步副本报 `AssertionError`，原因是 Git 将新增文件 LF 转换为 CRLF；统一换行后 8 个文件全部相同。
- ELF 的编译数据库包含生产驱动，符号表含 `dart::Hx711Port::Read()`（`0x0802c7e4`）及构造函数（`0x0802c738`），映像包含新增日志前缀。
- 新 ELF `dart-fw/build/debug/DevC.elf` SHA-256：`65E6B3C8B3540E064CFCF8D6DDB0983144FDEF290F5512962A2A4D5C737E8CF4`；4d 备份 `dart-fw/build/DevC-322e39b-before-4e.elf` 与原哈希一致。
- 本次未烧录，未采集实机拉力日志，未执行 N/kg 标定。

## 复核命令（PowerShell）

源码目录：

```powershell
Set-Location 'C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher'
$env:PATH = 'C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;' + $env:PATH
g++ -std=c++17 -Wall -Wextra -Werror -O1 -Itest/hx711_stubs -Isrc/hw test/hx711_port_test.cpp src/hw/hx711_port.cpp -o build-host/hx711_port_test.exe
.\build-host\hx711_port_test.exe
$taskSources = (Get-ChildItem -LiteralPath src -Filter '*.cpp').FullName
g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test/host_test.cpp $taskSources -o build-host/host_test_4e.exe
.\build-host\host_test_4e.exe
```

固件目录：

```powershell
Set-Location 'C:\Users\Lenovo\Desktop\xrobot\dart-fw'
xrobot format --check
python -X utf8 C:\Users\Lenovo\.codex\skills\embed-ai-tool-master\skills\build-cmake\scripts\cmake_builder.py --source C:\Users\Lenovo\Desktop\xrobot\dart-fw --preset debug --no-save-profile
Get-FileHash -LiteralPath build/debug/DevC.elf -Algorithm SHA256
```

## 现场检查

烧录由用户在自己的终端进行。烧录后 USB 日志应出现 `Dart tension R raw=... new=... n=... age=...ms`，空载记录连续数据，再轻拉右侧传感器观察 `raw` 变化，供后续标定使用。
