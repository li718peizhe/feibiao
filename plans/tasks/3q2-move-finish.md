# 任务 3q2：补完 3q——摆臂流程移动的参数和“卡在终点附近也算到”，然后提交、编译固件

只读这一个文件就够了。不要读 `plans/dart-fw-libxr.md`、`handoff.md` 和别的任务文件，也不要运行 graphify（包括只读查询）。做完写汇报，然后停下。

## 背景
- 工作区 `C:\Users\Lenovo\Desktop\xrobot`（本身不是 git 仓库）。模块仓库 `xrobot-local\DartLauncher\`（分支 `main`，HEAD `2cc1764`），固件工程 `dart-fw\`（分支 `dart-launcher`，HEAD `be3d3ee`）。两个仓库都在当前分支上直接提交，不新建、不切换分支。不要改 `dart-fw\Modules\local\DartLauncher` 这份副本，不要手改 `xrobot.lock`。
- 上一轮任务 3q（推杆标定 + 自动取镖试运行）已经改了 DartLauncher 的 14 个文件，还没提交，停在主机测试：流程里摆臂移动（`MotorAxis::MoveTo`）的受力模型检查没过。第一轮参数（60°/s、最低 4°/s、到位 0.3°）只有摩擦最小、惯量最大的模型冲过终点 0.72°，超了 0.5° 的门槛；改成 40°/s、最低 2°/s 以后，按实车拟合的模型反而卡在终点前 0.44°，堵转保护把电流限在 1.2 A，再也爬不上去，等到超时。
- 修法已经在隔离副本 `xrobot-local\DartLauncher\build-host\3q-fix-lab\` 里做好并验证过（整套主机测试 `PASS: 22028 checks`）：
  - 参数退回 60°/s、4°/s、0.3°。
  - 新增“离终点不到 1°、不转持续 0.3 s 也算走到”（`kArmMoveNear`、`kArmMoveSettleTime`，`AxisConfig::move_near`、`move_settle_time`），实车上镖座比目标早一点挡住时不会一直顶到超时报故障。
  - “停在终点附近”的检查门槛从 0.5° 放宽到 1°（用户手动取镖时摆臂位置本来就差 ±1°，照样能取到）。
  - 新增一条检查“镖座比终点早 0.45° 挡住时移动照样结束”。
  副本和工作区只差 4 个文件，其余源文件完全一样。这次把这 4 个文件拷回去，再手改适配层两行和 README 一行，然后完成 3q 剩下的提交和固件编译。

## 允许改的文件
- `xrobot-local\DartLauncher\`：`src\dart_params.hpp`、`src\motor_axis.hpp`、`src\motor_axis.cpp`、`test\host_test.cpp`（这四个只从副本拷，不手改），`src\hw\launcher_hw.cpp`（只加两行），`README.md`（只加一行、改一句）。
- `dart-fw\`：`xrobot setup` 改动的 `xrobot.lock` 和它生成的文件，`build\` 下的产物，备份 `build\DevC-2cc1764.elf`。
- 上一轮改过的其他 10 个文件一个字都不动。

## 步骤
1. 核对状态（PowerShell）：
   ```powershell
   Set-Location C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher
   git branch --show-current
   git rev-parse HEAD
   git status --porcelain
   Get-FileHash build-host\3q-fix-lab\src\dart_params.hpp, build-host\3q-fix-lab\src\motor_axis.hpp, build-host\3q-fix-lab\src\motor_axis.cpp, build-host\3q-fix-lab\test\host_test.cpp -Algorithm SHA256
   git -C C:\Users\Lenovo\Desktop\xrobot\dart-fw branch --show-current
   git -C C:\Users\Lenovo\Desktop\xrobot\dart-fw status --porcelain
   ```
   应该是：分支 `main`；HEAD `2cc17640298f611c4c3c5f035b6d518142343199`；`status` 正好是这 14 行修改：`README.md`、`src/dart_params.hpp`、`src/hw/launcher_hw.cpp`、`src/hw/launcher_hw.hpp`、`src/launch_sequence.cpp`、`src/launch_sequence.hpp`、`src/match_flow.cpp`、`src/mode_logic.cpp`、`src/mode_logic.hpp`、`src/motor_axis.cpp`、`src/motor_axis.hpp`、`src/stall_guard.cpp`、`src/stall_guard.hpp`、`test/host_test.cpp`；四个副本文件的 SHA-256 依次是：
   ```
   885CD472D4C77DA5326D2C23C518294F8493C846F74D2A68A0013F281DF15861  dart_params.hpp
   D4A77684083B44E74C01C25B1A225EAEAB2044E1CE771A2AB5D2106A5B4E623C  motor_axis.hpp
   FA0A78032BB57014D47C70808824A8D920D3974B48C9C790352B9C57989A8783  motor_axis.cpp
   DFA53A16E4511BC0F78B9647C4FDED812BB11ED174125FA8B264BF1CC8CED2BC  host_test.cpp
   ```
   dart-fw 分支 `dart-launcher`，`status` 为空。有一项不对就停下写汇报。
2. 拷文件：
   ```powershell
   Copy-Item build-host\3q-fix-lab\src\dart_params.hpp src\dart_params.hpp
   Copy-Item build-host\3q-fix-lab\src\motor_axis.hpp src\motor_axis.hpp
   Copy-Item build-host\3q-fix-lab\src\motor_axis.cpp src\motor_axis.cpp
   Copy-Item build-host\3q-fix-lab\test\host_test.cpp test\host_test.cpp
   ```
3. `src\hw\launcher_hw.cpp` 的 `ArmConfig()` 里，`config.move_tolerance = kArmMoveTolerance;` 那行后面加两行（和主机测试里的 `ArmAxisConfig()` 保持一致）：
   ```cpp
       config.move_near = kArmMoveNear;
       config.move_settle_time = kArmMoveSettleTime;
   ```
4. `README.md`：
   - 参数表里 `kArmAutoSpeed` / `kArmMoveCreep` / `kArmMoveTolerance` 那行后面加下面这一行（原样照抄）：
     ```
     | `kArmMoveNear` / `kArmMoveSettleTime` | 1° / 0.3 s，离终点不到 1° 卡住 0.3 s 也算走到，按当时位置保持，不一直顶到超时 |
     ```
   - “每段流程摆臂移动通过 `MoveTo` 按点动走，接近终点减速、到位按松手保持”这句后面补上“；在终点前 1° 以内卡住不动 0.3 s 也算到位”。
5. 主机测试：
   ```powershell
   $env:PATH = "C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;" + $env:PATH
   $src = (Get-ChildItem src\*.cpp).FullName
   g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test\host_test.cpp $src -o build-host\host_test.exe
   .\build-host\host_test.exe
   ```
   应该打印 `PASS: 22028 checks`。
6. 框架引用检查，下面这条应该没有任何输出：
   ```powershell
   Select-String -Path src\*.hpp, src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'
   ```
7. 在 DartLauncher 的 `main` 上提交：先单独运行 `git add src test README.md`，再单独运行 `git commit -m "推杆按实测脉宽标定；装弹组拨轮往上 2 s 自动取镖试运行（先左后右，取起再放回）；流程摆臂移动按点动走，卡在终点附近也算到"`。提交后用 `git rev-parse HEAD`、`git branch --show-current`、`git status --porcelain` 核对（沙箱有时会吞掉 git 的写入）；`status` 应该为空。
8. 备份板上那版固件，更新模块并编译：
   ```powershell
   Set-Location C:\Users\Lenovo\Desktop\xrobot\dart-fw
   Copy-Item build\debug\DevC.elf build\DevC-2cc1764.elf
   xrobot setup --update local/DartLauncher
   git -C Modules\local\DartLauncher log --oneline -1
   cmake --build --preset debug
   Get-FileHash build\debug\DevC.elf -Algorithm SHA256
   ```
   - 拷之前的 `build\debug\DevC.elf` 应该是 2026-11-12 23:23:44 那版（SHA-256 `B1A28470E5801B27BF79328EA650B42041BCD4C85AEDCA43926619A6B5981D16`），就是板上现在跑的。
   - xrobot 提示安装版本 1.0.0 和固定版本 1.0.1 不同，这是已知的，退出码是 0 就继续，不要升级 xrobot。不用跑 `xrobot gen`（模块构造参数没变）。
   - `xrobot setup` 偶尔报 502 之类的临时网络错误，原样重跑一次；还不行就停下。不要手改 `xrobot.lock`。
   - 副本的 `log --oneline -1` 必须是第 7 步的提交；不是就停下，不要进副本 fetch 或 checkout。
   - 编译输出里模块文件（`launcher_hw.cpp` 等）不能有新警告；referee.h 的 packed 警告是老的。适配层编译错误可以修（只能改 `src\hw\` 里上一轮改过的那两个文件），修了要重新跑第 5~7 步（新提交）再更新副本；修两轮还不行就停下。
9. 在 dart-fw 的 `dart-launcher` 上提交：先单独运行 `git add xrobot.lock`，再单独运行 `git commit -m "更新 DartLauncher：推杆标定，自动取镖试运行"`，同样核对。`git status --porcelain` 里还有别的文件，不要提交，写进汇报。不要烧录（沙箱里连不上调试器，烧录由用户做）。

## 卡住就停
遇到下面任何一种情况，就停下写汇报，不要自己想办法绕过去：
- 第 1 步有一项对不上
- 主机测试不是 `PASS: 22028 checks`（失败，或者检查数不同）
- 第 6 步有输出
- 联网重跑一次还失败，或者副本没有更新到第 7 步的提交
- git 提交后核对不上
- 需要改“允许改的文件”以外的东西
- 编译错误修了两轮还没好

## 汇报
命令里的 `-o codex-report.md` 会用你最后一条回复覆盖这个文件，所以下面这些都写在最后一条回复里：
- 第 1 步的核对结果；第 3、4 步改完后的原文
- 主机测试的 `PASS` 数；第 6 步的结果
- `Modules\local\DartLauncher` 的提交号；`DevC.elf` 的修改时间和 SHA-256；编译输出里的 FLASH、RAM 占用；模块文件有没有新警告
- 两个仓库的提交号、当前分支和 `git status --porcelain` 的输出；中途停下就写停在哪一步，附原文报错
