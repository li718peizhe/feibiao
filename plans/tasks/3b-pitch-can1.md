# 任务 3b：pitch 改到 CAN1，状态打印每行间隔加长

只读这一个文件就够了。不要读 `plans/dart-fw-libxr.md`，也不要读别的任务文件，也不要运行 graphify（包括只读查询）。做完写汇报，然后停下。

## 背景
- 工作区：`C:\Users\Lenovo\Desktop\xrobot`
- 模块仓库：`xrobot-local\DartLauncher\`（git，分支 main，当前 HEAD 6305640），主机测试现在是 `PASS: 18720 checks`
- 固件工程：`dart-fw\`（git，分支 dart-launcher，当前 HEAD 8f254a9）。模块会检出到 `Modules\local\DartLauncher`，不要改这份副本，也不要手改 `xrobot.lock`。`User\xrobot_main.hpp` 是 `xrobot gen` 生成的，不进 git。
- 两个仓库都直接在当前分支上提交：DartLauncher 在 `main`，dart-fw 在 `dart-launcher`。不要新建分支，也不要切换分支。
- 代码规范：注释用中文；不新增宏；一行一条语句；写法和周围代码保持一致。

## 为什么要改
- 2026-10-20 上车时，pitch 在 CAN2 上一直不在线。用户定：pitch 电机改用 CAN1 收发。电调 ID 还是 3，反馈 ID 还是 0x203（515）。CAN1 上两台滑块是 0x201、0x202，不冲突，三台共用 0x200 控制帧。
- 状态打印第 5 行在 `pi=` 后面被截断了，`pI=` 和 `yaw=` 都没打出来。原因：STDIO 走 USB CDC，发送缓冲只有 128 字节，每行之间只隔 5 ms，上一行还没发完，下一行就按剩下的空间被截短。把每行之间的等待加到 20 ms。

## 要改的地方
1. `dart-fw\User\RobotConfig\artisans_dart.yaml`：
   - `pitch_motor` 里的 `can_bus: can2` 改成 `can_bus: can1`。`feedback_id: 515`、`reverse: false` 都不动。
   - `dart_launcher` 里的 `pitch_can: can2` 改成 `pitch_can: can1`。`pitch_id: 515` 不动。
   - 别的都不动：yaw、摆臂还在 can2，两台滑块不动。
2. `xrobot-local\DartLauncher\src\hw\launcher_hw.cpp`：
   - `LauncherHardware::OnMonitor()` 里，打印各行之间的 5 处 `LibXR::Thread::Sleep(5);` 都改成 `LibXR::Thread::Sleep(20);`。
   - 在第一处上面加一行注释：`// STDIO 走 USB CDC，发送缓冲只有 128 字节；每行之间等 20 ms，让上一行发完，免得下一行被截短。`
   - 打印的内容和格式都不动。
3. `xrobot-local\DartLauncher\README.md`：
   - 设备表里 pitch 那一行改成：`| pitch | can1 | M3508 / 0x203（电调 ID 3），和两台滑块共用 0x200 控制帧 |`。
   - 状态行说明里加一句：每行之间隔 20 ms，因为 USB CDC 发送缓冲只有 128 字节，隔得太近后面的行会被截短。
4. 测试不用改。

## 允许改的文件
- `xrobot-local\DartLauncher\`：`src\hw\launcher_hw.cpp`、`README.md`。
- `dart-fw\`：`User\RobotConfig\artisans_dart.yaml`、`xrobot setup` 改动的 `xrobot.lock`，以及 xrobot 生成的文件。其他文件都不碰。

## 步骤
1. 先确认分支：`git -C C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher branch --show-current` 应该是 `main`，`git -C C:\Users\Lenovo\Desktop\xrobot\dart-fw branch --show-current` 应该是 `dart-launcher`，两个仓库的 `git status --porcelain` 都应该是空的。不对就停下写汇报。
2. 改 DartLauncher（第 2、3 条）。
3. 跑主机测试，在 PowerShell 里：
   ```powershell
   $env:PATH = "C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;" + $env:PATH
   Set-Location C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher
   New-Item -ItemType Directory -Force build-host | Out-Null
   $src = (Get-ChildItem src\*.cpp).FullName
   g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test\host_test.cpp $src -o build-host\host_test.exe
   .\build-host\host_test.exe
   ```
   应该还是 `PASS: 18720 checks`。
4. 框架引用检查，下面这条命令应该没有任何输出：
   ```powershell
   Select-String -Path src\*.hpp, src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'
   ```
5. 在 DartLauncher 仓库的 `main` 上提交：先单独运行 `git add src README.md`，再单独运行 `git commit -m "pitch 改到 CAN1，状态打印每行间隔 20 ms"`。提交后用 `git rev-parse HEAD`、`git branch --show-current`、`git status --porcelain` 核对，因为沙箱有时会吞掉 git 的写入。
6. 改 dart-fw 的配置（第 1 条），然后更新固件并编译：
   ```powershell
   Set-Location C:\Users\Lenovo\Desktop\xrobot\dart-fw
   xrobot setup --update local/DartLauncher
   git -C Modules\local\DartLauncher log --oneline -1
   xrobot gen -c User/RobotConfig/artisans_dart.yaml
   Select-String -Path User\xrobot_main.hpp -Pattern 'pitch_motor\(|arm_motor, can'
   cmake --build --preset debug
   ```
   - xrobot 会提示安装版本 1.0.0 和固定版本 1.0.1 不同，这是已知的，退出码是 0 就继续，不要去升级 xrobot。
   - `git -C Modules\local\DartLauncher log --oneline -1` 必须是第 5 步的提交。不是的话就停下写汇报，不要编译，不要自己去副本里 fetch 或 checkout。
   - `Select-String` 应该看到 `pitch_motor(can1, ...)`，以及 `dart_launcher` 构造参数里的 `arm_motor, can1, can1, can2, can2`。不是的话就停下写汇报。
   - `build\debug\DevC.elf` 的修改时间应该是这次的。
   - 如果 `xrobot setup` 联网失败，就停下写汇报。不要手改 `xrobot.lock`。
7. 在 dart-fw 的 `dart-launcher` 上提交：先单独运行 `git add xrobot.lock User/RobotConfig/artisans_dart.yaml`，再单独运行 `git commit -m "pitch 改到 CAN1，更新 DartLauncher"`。提交后同样核对；`git status --porcelain` 里还有别的文件，不要提交，写进汇报。

## 卡住就停
遇到下面任何一种情况，就停下写汇报，不要自己想办法绕过去：
- 分支或工作区状态不对
- 主机测试没有 `PASS`
- 第 4 步有输出
- 联网失败，或者副本没有更新到第 5 步的提交，或者生成的文件和上面说的不一样
- git 提交后核对不上
- 需要改“允许改的文件”以外的东西
- 编译错误修了两轮还没好

## 汇报
命令里的 `-o codex-report.md` 会用你最后一条回复覆盖这个文件，所以下面这些都写在最后一条回复里：
- 改了哪些文件，每个文件一两句说改了什么
- 主机测试的 `PASS` 数；第 4 步的结果；第 6 步 `Select-String` 找到的行
- `Modules\local\DartLauncher` 的提交号；`DevC.elf` 的修改时间；`arm-none-eabi-size build\debug\DevC.elf` 的输出
- 两个仓库的提交号、当前分支和 `git status --porcelain` 的输出；如果中途停下了，写停在哪一步
