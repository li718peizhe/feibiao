# 任务 3q4：自动取镖改成一次取一发、推杆只伸一次并慢慢抬起，右镖座目标降 0.5°，然后提交、编译固件

只读这一个文件就够了。不要读 `plans/dart-fw-libxr.md`、`handoff.md` 和别的任务文件，也不要运行 graphify（包括只读查询）。做完写汇报，然后停下。

## 背景
- 工作区 `C:\Users\Lenovo\Desktop\xrobot`（本身不是 git 仓库）。模块仓库 `xrobot-local\DartLauncher\`（分支 `main`，HEAD `792894d`），固件工程 `dart-fw\`（分支 `dart-launcher`，HEAD `0333670`）。两个仓库都在当前分支上直接提交，不新建、不切换分支。不要改 `dart-fw\Modules\local\DartLauncher` 这份副本，不要手改 `xrobot.lock`。板上现在跑的是 `792894d` 编的固件。
- 11-13 02:15 实车：左边自动取镖稳定了。用户要求：右边摆臂角度高了约 0.5°；推杆不要伸缩两次（现在每侧取起以后还要下探放回）；推杆伸到底以后慢一点上去。
- 磁铁一次只能吸一发，不放回就不能接着去另一侧。所以改成：装弹组拨轮往上 2 s 只取一发，第一发左边、以后左右轮流；推杆伸一次吸住，按 `kCrankLiftSpeed`（平常的三分之一，约 1 s 抬完）慢慢抬起，摆臂停在镖座上方举着。操作手拨轮往下 2 s 让磁铁通电放镖以后才允许取下一发，免得吸着镖去另一侧撞上那边的镖。`kFetchRight` 46.88° → 46.38°。`LauncherIo::SetCrank` 加了速度参数，`ServoRamp` / `ServoPort` 支持单段速度；自动换弹里的“取镖：抬起”也用慢速（自动换弹现在关着）。
- 这些已经在隔离副本 `xrobot-local\DartLauncher\build-host\3q4-lab\` 里做好并验证过（整套主机测试 `PASS: 22036 checks`）。适配层（`src\hw\`）主机不编译，靠这次的固件编译检查。副本和工作区只差下面 12 个文件，其余完全一样。这次把它们拷回去，然后提交、编译。

## 允许改的文件
- `xrobot-local\DartLauncher\`：下面第 2 步拷的 12 个文件，只从副本拷，不手改；只有固件编译报错时可以改 `src\hw\launcher_hw.cpp`、`src\hw\launcher_hw.hpp`、`src\hw\servo_port.cpp`、`src\hw\servo_port.hpp`。
- `dart-fw\`：`xrobot setup` 改动的 `xrobot.lock` 和它生成的文件，`build\` 下的产物，备份 `build\DevC-792894d.elf`。

## 步骤
1. 核对状态（PowerShell）：
   ```powershell
   Set-Location C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher
   git branch --show-current
   git rev-parse HEAD
   git status --porcelain
   Get-FileHash build-host\3q4-lab\src\dart_params.hpp, build-host\3q4-lab\src\launch_sequence.hpp, build-host\3q4-lab\src\launch_sequence.cpp, build-host\3q4-lab\src\launcher_io.hpp, build-host\3q4-lab\src\servo_ramp.hpp, build-host\3q4-lab\src\servo_ramp.cpp, build-host\3q4-lab\src\hw\launcher_hw.hpp, build-host\3q4-lab\src\hw\launcher_hw.cpp, build-host\3q4-lab\src\hw\servo_port.hpp, build-host\3q4-lab\src\hw\servo_port.cpp, build-host\3q4-lab\test\host_test.cpp, build-host\3q4-lab\README.md -Algorithm SHA256
   git -C C:\Users\Lenovo\Desktop\xrobot\dart-fw branch --show-current
   git -C C:\Users\Lenovo\Desktop\xrobot\dart-fw rev-parse HEAD
   git -C C:\Users\Lenovo\Desktop\xrobot\dart-fw status --porcelain
   Get-FileHash C:\Users\Lenovo\Desktop\xrobot\dart-fw\build\debug\DevC.elf -Algorithm SHA256
   ```
   应该是：分支 `main`；HEAD `792894daa7974c3b232e56ee56815117ea473a69`；`status` 为空；12 个副本文件的 SHA-256 依次是
   ```
   63FFF13D2F05ACB933D308FDD6FEB43674A9ADF5998844C6019B348405DF8214  src\dart_params.hpp
   9D67231B1CA69FD53114974E35E7206B1B20685B61F0E09A7FCCCFB8D54BBF47  src\launch_sequence.hpp
   C6FD448C3325FE09535A846A00183AAF3B0CC1A1C57CE545310F60B27708C233  src\launch_sequence.cpp
   9E9AA7F7069829AF33FE3FF09C67D46CA5AB73E0622E3D6B2C984E9AEDCF9471  src\launcher_io.hpp
   6CCE9112C7912D13F7AC522C20B158B8B77114E21B7127CD268E4B677633A31D  src\servo_ramp.hpp
   434963668FF6D180E0C011D6EC73118283940FB86FEBB09D6A55DE7D20389C39  src\servo_ramp.cpp
   334CF413CBD7A21F37DF357406F3983ACB14EDFB5941FA35C535DA1063151B74  src\hw\launcher_hw.hpp
   CA7D6FE3EE1DF81AC912EB936FFFD8C93F18922F618E74F85F454E0BE04BAFCD  src\hw\launcher_hw.cpp
   6294F035C2B11B1CFD169A29CCC85DB96BA8AA1E5FF91F2790519CD4C090986C  src\hw\servo_port.hpp
   C76DFA977003CD6445942E2997EA5568B26CE250CF451B8D8C7B656A1D0B4225  src\hw\servo_port.cpp
   DE45C5E448597C232825285B9B2BEB22E052291E52A7466FBE3CF13C455A1E84  test\host_test.cpp
   27BEC05DEFA6A19002607285012CF7C76A0FF1CFB2EBD30E5B8EE8BCEFCC92A7  README.md
   ```
   dart-fw 分支 `dart-launcher`，HEAD `033367081702e53d85e08fdef33b3c99e2d55f72`，`status` 为空；`DevC.elf` 的 SHA-256 是 `D9136DB156DB169B8ED3DF14EE21CDFBDEF78727FD1A40E2E0F1D549023CBF8F`（板上现在跑的那版）。有一项不对就停下写汇报。
2. 拷文件：
   ```powershell
   foreach ($f in 'src\dart_params.hpp', 'src\launch_sequence.hpp', 'src\launch_sequence.cpp', 'src\launcher_io.hpp', 'src\servo_ramp.hpp', 'src\servo_ramp.cpp', 'src\hw\launcher_hw.hpp', 'src\hw\launcher_hw.cpp', 'src\hw\servo_port.hpp', 'src\hw\servo_port.cpp', 'test\host_test.cpp', 'README.md') { Copy-Item (Join-Path 'build-host\3q4-lab' $f) $f }
   git status --porcelain
   ```
   `status` 应该正好是这 12 个文件修改。
3. 主机测试：
   ```powershell
   $env:PATH = "C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;" + $env:PATH
   $src = (Get-ChildItem src\*.cpp).FullName
   g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test\host_test.cpp $src -o build-host\host_test.exe
   .\build-host\host_test.exe
   ```
   应该打印 `PASS: 22036 checks`。
4. 框架引用检查，下面这条应该没有任何输出：
   ```powershell
   Select-String -Path src\*.hpp, src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'
   ```
5. 在 DartLauncher 的 `main` 上提交：先单独运行 `git add src test README.md`，再单独运行 `git commit -m "自动取镖一次取一发、左右轮流，推杆只伸一次，吸住后慢慢抬起，放镖前不许取下一发；右镖座目标降 0.5 度"`。提交后用 `git rev-parse HEAD`、`git branch --show-current`、`git status --porcelain` 核对（沙箱有时会吞掉 git 的写入）；`status` 应该为空。
6. 备份板上那版固件，更新模块并编译：
   ```powershell
   Set-Location C:\Users\Lenovo\Desktop\xrobot\dart-fw
   Copy-Item build\debug\DevC.elf build\DevC-792894d.elf
   xrobot setup --update local/DartLauncher
   git -C Modules\local\DartLauncher log --oneline -1
   cmake --build --preset debug
   Get-FileHash build\debug\DevC.elf -Algorithm SHA256
   ```
   - xrobot 提示安装版本 1.0.0 和固定版本 1.0.1 不同，这是已知的，退出码是 0 就继续，不要升级 xrobot。不用跑 `xrobot gen`。
   - `xrobot setup` 偶尔报 502 之类的临时网络错误，原样重跑一次；还不行就停下。不要手改 `xrobot.lock`。
   - 副本的 `log --oneline -1` 必须是第 5 步的提交；不是就停下，不要进副本 fetch 或 checkout。
   - 编译输出里模块文件（`launcher_hw.cpp`、`servo_port.cpp` 等）不能有新警告；referee.h 的 packed 警告、CMake 的 `enable_language` 和 Eigen `CMP0146` 警告是老的。适配层编译错误只能在上面列的四个 `src\hw\` 文件里修，修了要重新跑第 3~5 步（新提交）再更新副本；修两轮还不行就停下。
7. 在 dart-fw 的 `dart-launcher` 上提交：先单独运行 `git add xrobot.lock`，再单独运行 `git commit -m "更新 DartLauncher：自动取镖一次一发、慢慢抬起"`，同样核对。`git status --porcelain` 里还有别的文件，不要提交，写进汇报。不要烧录。

## 卡住就停
遇到下面任何一种情况，就停下写汇报，不要自己想办法绕过去：
- 第 1 步有一项对不上
- 主机测试不是 `PASS: 22036 checks`（失败，或者检查数不同）
- 第 4 步有输出
- 联网重跑一次还失败，或者副本没有更新到第 5 步的提交
- git 提交后核对不上
- 需要改“允许改的文件”以外的东西
- 编译错误修了两轮还没好

## 汇报
命令里的 `-o codex-report.md` 会用你最后一条回复覆盖这个文件，所以下面这些都写在最后一条回复里：
- 第 1 步的核对结果；第 2 步拷完以后 `git status --porcelain` 的输出
- 主机测试的 `PASS` 数；第 4 步的结果
- `Modules\local\DartLauncher` 的提交号；`DevC.elf` 的修改时间和 SHA-256；编译输出里的 FLASH、RAM 占用；模块文件有没有新警告；如果改过适配层，附改动的原文
- 两个仓库的提交号、当前分支和 `git status --porcelain` 的输出；中途停下就写停在哪一步，附原文报错
