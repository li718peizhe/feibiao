# 任务 3q3：取镖位置按实测改，流程摆臂在终点附近卡住先加大推力，然后提交、编译固件

只读这一个文件就够了。不要读 `plans/dart-fw-libxr.md`、`handoff.md` 和别的任务文件，也不要运行 graphify（包括只读查询）。做完写汇报，然后停下。

## 背景
- 工作区 `C:\Users\Lenovo\Desktop\xrobot`（本身不是 git 仓库）。模块仓库 `xrobot-local\DartLauncher\`（分支 `main`，HEAD `be9af19`），固件工程 `dart-fw\`（分支 `dart-launcher`，HEAD `dca77fb`）。两个仓库都在当前分支上直接提交，不新建、不切换分支。不要改 `dart-fw\Modules\local\DartLauncher` 这份副本，不要手改 `xrobot.lock`。板上现在跑的是 `be9af19` 编的固件。
- 自动取镖试运行（任务 3q）上车：左边 10 次只吸上 2 次，右边一次没吸上。用户手动对准量了能吸住的位置：左摆臂 −41.65°（raw 994）、推杆 2288 µs；右摆臂 46.88°（raw 3009）、推杆 2395 µs。原来的参数是左 −40.5° / 2335 µs、右 46.8° / 2296 µs：左边是摆臂角度偏了，右边是推杆不够深。
- 左镖座从约 −40.3° 起有东西往回顶，要压到 −41.65°。压的时候摆臂一卡一卡地慢慢走，现在“卡在终点附近也算到”的规则看转速读数、卡 0.3 s 就停，主机弹性镖座模型里会停在 −40.77°，压不进去。改法：按位置判断卡住（0.3 s 内挪动不到 0.1°），卡住以后先像把摇杆推大一样加大速度目标（20°/s）推 1 s，还不动才按当时位置保持。
- 这些已经在隔离副本 `xrobot-local\DartLauncher\build-host\3q3-lab\` 里做好并验证过（整套主机测试 `PASS: 22033 checks`；弹性镖座模型压到 −41.28°~−41.47°）。副本和工作区只差 4 个文件，其余源文件完全一样。这次把这 4 个文件拷回去，再手改适配层三行和 README，然后提交、编译。

## 允许改的文件
- `xrobot-local\DartLauncher\`：`src\dart_params.hpp`、`src\motor_axis.hpp`、`src\motor_axis.cpp`、`test\host_test.cpp`（这四个只从副本拷，不手改），`src\hw\launcher_hw.cpp`（只加三行），`README.md`（只改下面列的几处）。
- `dart-fw\`：`xrobot setup` 改动的 `xrobot.lock` 和它生成的文件，`build\` 下的产物，备份 `build\DevC-be9af19.elf`。

## 步骤
1. 核对状态（PowerShell）：
   ```powershell
   Set-Location C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher
   git branch --show-current
   git rev-parse HEAD
   git status --porcelain
   Get-FileHash build-host\3q3-lab\src\dart_params.hpp, build-host\3q3-lab\src\motor_axis.hpp, build-host\3q3-lab\src\motor_axis.cpp, build-host\3q3-lab\test\host_test.cpp -Algorithm SHA256
   git -C C:\Users\Lenovo\Desktop\xrobot\dart-fw branch --show-current
   git -C C:\Users\Lenovo\Desktop\xrobot\dart-fw rev-parse HEAD
   git -C C:\Users\Lenovo\Desktop\xrobot\dart-fw status --porcelain
   Get-FileHash C:\Users\Lenovo\Desktop\xrobot\dart-fw\build\debug\DevC.elf -Algorithm SHA256
   ```
   应该是：分支 `main`；HEAD `be9af198be6ff177d7e266d8aac0e06ac0980a09`；`status` 为空；四个副本文件的 SHA-256 依次是
   ```
   A280F3872B99DFCE6D42999C3754C0C769C7733DC101E8190D993938EBD52E61  dart_params.hpp
   29D591A23AE5CE84923DC50CB40CA3731D1758C91C3CFDF73A48F4A25F9D97EF  motor_axis.hpp
   D1D65DC3B82978C01CECEC7513E51AE57998B1CD9178918F59A1E8480136B6FC  motor_axis.cpp
   81E0D9B3433AB5404A2C9278916EF222BB94C51B453CFD543D41276C192B1F33  host_test.cpp
   ```
   dart-fw 分支 `dart-launcher`，HEAD `dca77fb682e6037925ac96929e50eb7efc4c83d2`，`status` 为空；`DevC.elf` 的 SHA-256 是 `CF268D0BC9967E1EAF4C31FDBF8A970276BEA8FC2FF59A56D1D5B6CE57BF4BFE`（板上现在跑的那版）。有一项不对就停下写汇报。
2. 拷文件：
   ```powershell
   Copy-Item build-host\3q3-lab\src\dart_params.hpp src\dart_params.hpp
   Copy-Item build-host\3q3-lab\src\motor_axis.hpp src\motor_axis.hpp
   Copy-Item build-host\3q3-lab\src\motor_axis.cpp src\motor_axis.cpp
   Copy-Item build-host\3q3-lab\test\host_test.cpp test\host_test.cpp
   ```
3. `src\hw\launcher_hw.cpp` 的 `ArmConfig()` 里，`config.move_settle_time = kArmMoveSettleTime;` 那行后面加三行（和主机测试里的 `ArmAxisConfig()` 一致；漏了不会编译报错，但实车上卡住不会加大推力）：
   ```cpp
       config.move_still_band = kArmMoveStillBand;
       config.move_push = kArmMovePush;
       config.move_push_time = kArmMovePushTime;
   ```
4. `README.md`（自动取镖试运行那一节）：
   - “在终点前 1° 以内卡住不动 0.3 s 也算到位”改成“在终点前 1° 以内 0.3 s 里挪动不到 0.1° 就算卡住，先像把摇杆推大一样加大推力推 1 s，还不动才按当时位置保持”。
   - 参数表里 `kFetchLeft` / `kFetchRight` 那行的值改成“−41.65° / 46.88°，左右镖座摆臂位置（11-13 手动对准实测；左边要压进镖座约 1.35°）”。
   - `kCrankPulseFetchLeft` / `kCrankPulseFetchRight` 那行的值改成“2288 / 2395 µs，左右下探位置（11-13 手动压到能吸住）；方向 +1”。
   - `kArmMoveNear` / `kArmMoveSettleTime` 那一行换成下面两行（原样照抄）：
     ```
     | `kArmMoveNear` / `kArmMoveSettleTime` / `kArmMoveStillBand` | 1° / 0.3 s / 0.1°，离终点不到 1° 时 0.3 s 里挪动不到 0.1° 算卡住 |
     | `kArmMovePush` / `kArmMovePushTime` | 20°/s / 1 s，卡住以后加大推力推这么久，还不动就按当时位置保持 |
     ```
5. 主机测试：
   ```powershell
   $env:PATH = "C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;" + $env:PATH
   $src = (Get-ChildItem src\*.cpp).FullName
   g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test\host_test.cpp $src -o build-host\host_test.exe
   .\build-host\host_test.exe
   ```
   应该打印 `PASS: 22033 checks`。
6. 框架引用检查，下面这条应该没有任何输出：
   ```powershell
   Select-String -Path src\*.hpp, src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'
   ```
7. 在 DartLauncher 的 `main` 上提交：先单独运行 `git add src test README.md`，再单独运行 `git commit -m "取镖位置按 11-13 手动对准实测改（左 -41.65 度 / 2288 us，右 46.88 度 / 2395 us）；流程摆臂按位置判断卡住，卡住先加大推力"`。提交后用 `git rev-parse HEAD`、`git branch --show-current`、`git status --porcelain` 核对（沙箱有时会吞掉 git 的写入）；`status` 应该为空。
8. 备份板上那版固件，更新模块并编译：
   ```powershell
   Set-Location C:\Users\Lenovo\Desktop\xrobot\dart-fw
   Copy-Item build\debug\DevC.elf build\DevC-be9af19.elf
   xrobot setup --update local/DartLauncher
   git -C Modules\local\DartLauncher log --oneline -1
   cmake --build --preset debug
   Get-FileHash build\debug\DevC.elf -Algorithm SHA256
   ```
   - xrobot 提示安装版本 1.0.0 和固定版本 1.0.1 不同，这是已知的，退出码是 0 就继续，不要升级 xrobot。不用跑 `xrobot gen`。
   - `xrobot setup` 偶尔报 502 之类的临时网络错误，原样重跑一次；还不行就停下。不要手改 `xrobot.lock`。
   - 副本的 `log --oneline -1` 必须是第 7 步的提交；不是就停下，不要进副本 fetch 或 checkout。
   - 编译输出里模块文件（`launcher_hw.cpp` 等）不能有新警告；referee.h 的 packed 警告、CMake 的 `enable_language` 和 Eigen `CMP0146` 警告是老的。适配层编译错误只能在第 3 步加的三行里修，修了要重新跑第 5~7 步（新提交）再更新副本；修两轮还不行就停下。
9. 在 dart-fw 的 `dart-launcher` 上提交：先单独运行 `git add xrobot.lock`，再单独运行 `git commit -m "更新 DartLauncher：取镖位置实测，摆臂卡住先加大推力"`，同样核对。`git status --porcelain` 里还有别的文件，不要提交，写进汇报。不要烧录。

## 卡住就停
遇到下面任何一种情况，就停下写汇报，不要自己想办法绕过去：
- 第 1 步有一项对不上
- 主机测试不是 `PASS: 22033 checks`（失败，或者检查数不同）
- 第 6 步有输出
- 联网重跑一次还失败，或者副本没有更新到第 7 步的提交
- git 提交后核对不上
- 需要改“允许改的文件”以外的东西
- 编译错误修了两轮还没好

## 汇报
命令里的 `-o codex-report.md` 会用你最后一条回复覆盖这个文件，所以下面这些都写在最后一条回复里：
- 第 1 步的核对结果；第 3 步改完后 `ArmConfig()` 的原文；第 4 步改过的 README 原文
- 主机测试的 `PASS` 数；第 6 步的结果
- `Modules\local\DartLauncher` 的提交号；`DevC.elf` 的修改时间和 SHA-256；编译输出里的 FLASH、RAM 占用；模块文件有没有新警告
- 两个仓库的提交号、当前分支和 `git status --porcelain` 的输出；中途停下就写停在哪一步，附原文报错
