# 任务 2k：写入锁止舵机标定值，上膛先锁着

只读这一个文件就够了。不要读 `plans/dart-fw-libxr.md`，也不要运行 graphify。做完写汇报，然后停下。

## 背景
- 工作区：`C:\Users\Lenovo\Desktop\xrobot`
- 模块仓库：`xrobot-local\DartLauncher\`（git，分支 main，当前 HEAD 101bda8）
  - `src\`：纯逻辑，不 include LibXR、HAL、FreeRTOS
  - `src\hw\`：LibXR 适配层
  - `test\host_test.cpp`：主机测试，现在是 `PASS: 17456 checks`
- 固件工程：`dart-fw\`（git，分支 dart-launcher，当前 HEAD 09cd1fe）
  - 模块会从 `xrobot-local\DartLauncher` 检出到 `Modules\local\DartLauncher`。不要改这份副本，也不要手改 `xrobot.lock`。
- 代码规范：注释用中文；不新增宏；一行一条语句；写法和周围代码保持一致。

## 为什么要改
- 锁止舵机是众灵 PM60S，270°，500~2500 µs 对应 0~270°。2026-10-16 在原始脉宽模式下实测：
  - 完全锁死：1500 µs，停 30 s 不发热。
  - 放开：2000 µs，发射机构能顺畅通过，停 30 s 不发热。
  - 脉宽变大是张开。
- 现在 `kLockServoCalibrated = false`，`kLockServoOffset = 0`、`kLockServoDirection = 1` 都是占位值。改成 true 以后：
  - 一上电就输出锁死脉宽（`src\hw\servo_port.cpp` 的构造函数），舵机直接转到锁死。
  - 手动只能在锁死和放开之间调（`Manual()` 把机构角夹在 `kLockClosed`~`kLockOpen`），开锁还要通过 `LockOpenAllowed()`。
  - 拨轮下拨的上膛请求不再被“锁止舵机未标定”拦住。
- 但现在还不能上膛：舵机从 C 板 5 V 取电，实测只有 4.92 V，低于 PM60S 5 V 的下限，顶住限位会卡死；`kLatchSlider` 也还是 CAD 估计值。所以加一个开关 `kCockEnabled = false`，标定以后上膛仍然先拒绝。

## 要改的地方
1. `src\dart_params.hpp`：
   - 删掉第 66 行 `constexpr double kLockServoOffset = 0; ...` 和第 68 行 `constexpr double kLockServoDirection = 1; ...` 这两行占位值。第 67、69 行推杆的 `kCrankServoOffset`、`kCrankServoDirection` 不动。
   - 第 64、65 行只改注释：
     - `kLockClosed` 的注释改成 `// rad，锁死的机构角，对应 kLockPulseClosed。`
     - `kLockOpen` 的注释改成 `// rad，放开的机构角，对应 kLockPulseOpen。`
   - 在 `kServoPulseMax` 那一行（第 74 行）后面加下面这几行。必须放在这里，因为要用到 `kServoRange`、`kServoPulseMin`、`kServoPulseMax`、`kLockClosed`、`kLockOpen`，而且方向要在偏置前面：
     ```cpp
     constexpr double kLockPulseClosed = 1500; // µs，2026-10-16 实测完全锁死，停 30 s 不发热。
     constexpr double kLockPulseOpen = 2000; // µs，2026-10-16 实测放开，发射机构能顺畅通过。
     // 锁止舵机按两个实测脉宽反推方向和偏置，换成 180° 或 270° 舵机都一样。
     constexpr double kLockServoDirection = (kLockPulseOpen - kLockPulseClosed) /
         (kServoPulseMax - kServoPulseMin) * kServoRange / (kLockOpen - kLockClosed);
     constexpr double kLockServoOffset = (kLockPulseClosed - kServoPulseMin) /
         (kServoPulseMax - kServoPulseMin) * kServoRange - kLockServoDirection * kLockClosed;
     ```
   - `kLockServoCalibrated` 改成 `true`，注释改成 `// 2026-10-16 已按 kLockPulseClosed、kLockPulseOpen 标定；重装舵盘后改回 false 重新量。`
   - 在 `kAutoEnabled` 那一行后面加一行：
     ```cpp
     constexpr bool kCockEnabled = false; // 锁止舵机单独供电、kLatchSlider 实测之前不许上膛。
     ```
   - `kLockRawPulseMin`、`kLockRawPulseMax`、`kLockBootPulseUs`、`kLockPulseRate` 保留不动：以后重装舵盘、改回未标定时还要用。
   - 其他常量都不动。
2. `src\hw\launcher_hw.cpp`，`Tick()` 里处理 `command_.cock_request` 的地方（第 172~181 行）：
   - 在 `if constexpr (!kLockServoCalibrated) { ... }` 和后面的 `else { ... }` 中间加一个分支，变成：
     ```cpp
     if constexpr (!kLockServoCalibrated) {
         // （原来的注释和 XR_LOG_WARN 不动）
     } else if constexpr (!kCockEnabled) {
         // 锁止舵机单独供电、锁止位置实测之前，先拒绝上膛。
         XR_LOG_WARN("上膛未开放");
     } else {
         // （原来 else 里的内容不动）
     }
     ```
   - 别的地方都不动。
3. `test\host_test.cpp`：
   - 第 185 行 `Check(!kLockServoCalibrated, "lock servo is not calibrated for stage 2e");` 改成 `Check(kLockServoCalibrated, "lock servo is calibrated for stage 2k");`
   - 在 `Check(kLockPulseRate > 0, "raw lock pulse rate is positive");` 后面加三条：
     ```cpp
     Check(!kCockEnabled, "cocking stays disabled until the lock servo has its own supply");
     Check(kLockRawPulseMin < kLockPulseClosed && kLockPulseClosed < kLockPulseOpen &&
               kLockPulseOpen < kLockRawPulseMax, "calibrated lock pulses stay inside the measured travel");
     Check(Near(ServoRamp::PulseUs(kLockClosed, kLockServoOffset, kLockServoDirection), kLockPulseClosed, 1e-6) &&
               Near(ServoRamp::PulseUs(kLockOpen, kLockServoOffset, kLockServoDirection), kLockPulseOpen, 1e-6),
           "calibrated lock pulses match the measured values");
     ```
4. `README.md`：
   - 开关表（第 38~45 行）最后加一行：
     `| \`kCockEnabled\` | \`false\` | 拨轮下拨的上膛请求；锁止舵机单独供电、\`kLatchSlider\` 实测后改 \`true\` |`
   - 第 55~60 行 `kLockServoCalibrated` 那一条：
     - 第一句“锁死/解锁脉宽、方向和偏置确认后改为 `true`。”后面接一句：“2026-10-16 已标定为 `true`：锁死 1500 µs、放开 2000 µs（`kLockPulseClosed`、`kLockPulseOpen`），偏置和方向由这两个数算出来。标定后一上电舵机就转到锁死；手动只能在锁死和放开之间调，开锁仍受 `LockOpenAllowed()` 限制。”
     - 最后一句“锁止标定后，上膛可用，不要求换弹机构安装。”改成：“锁止标定后，上膛还要 `kCockEnabled` 为 `true`，否则提示“上膛未开放”；不要求换弹机构安装。”
   - 其余句子都不动，未标定时原始模式的说明保留，以后重装舵盘还要用。

## 允许改的文件
- `xrobot-local\DartLauncher\`：`src\dart_params.hpp`、`src\hw\launcher_hw.cpp`、`test\host_test.cpp`、`README.md`。
- `xrobot setup` 改动的 `dart-fw\xrobot.lock`，以及 xrobot 生成的文件。
- 其他文件都不碰。`kAutoEnabled` 和各个 `kXxxInstalled` 都不动。

## 步骤
1. 改代码（第 1~4 条）。
2. 跑主机测试，在 PowerShell 里：
   ```powershell
   $env:PATH = "C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;" + $env:PATH
   Set-Location C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher
   New-Item -ItemType Directory -Force build-host | Out-Null
   $src = (Get-ChildItem src\*.cpp).FullName
   g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test\host_test.cpp $src -o build-host\host_test.exe
   .\build-host\host_test.exe
   ```
   应该打印 `PASS: N checks`。加了 3 条，N 应该是 17459。只要是 `PASS` 就行，把 N 写进汇报。
3. 框架引用检查。下面这条命令应该没有任何输出：
   ```powershell
   Select-String -Path src\*.hpp, src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'
   ```
4. 确认占位值删干净了。下面这条命令也应该没有任何输出：
   ```powershell
   Select-String -Path src\dart_params.hpp -Pattern 'kLockServoOffset = 0|kLockServoDirection = 1'
   ```
5. 在 DartLauncher 仓库提交。下面两条分开运行：
   ```powershell
   git add src/dart_params.hpp src/hw/launcher_hw.cpp test/host_test.cpp README.md
   ```
   ```powershell
   git commit -m "写入锁止舵机标定值，上膛先锁着"
   ```
   提交后用 `git rev-parse HEAD` 和 `git status --porcelain` 核对，因为沙箱有时会吞掉 git 的写入。
6. 更新固件并编译：
   ```powershell
   Set-Location C:\Users\Lenovo\Desktop\xrobot\dart-fw
   xrobot setup --update local/DartLauncher
   xrobot gen -c User/RobotConfig/artisans_dart.yaml
   cmake --build --preset debug
   ```
   - xrobot 会提示安装版本 1.0.0 和固定版本 1.0.1 不同。这是已知的，退出码是 0 就继续，不要去升级 xrobot。
   - 用 `git -C Modules\local\DartLauncher log --oneline -1` 确认副本已经是第 5 步的提交。
   - 编译输出里应该能看到重新编译了 `launcher_hw.cpp`；`build\debug\DevC.elf` 的修改时间应该是这次的。
   - 如果 `xrobot setup` 联网失败（比如 GitHub SSL 握手失败），就停下写汇报，说明停在这一步。不要手改 `xrobot.lock`，也不要自己去 `Modules\local\DartLauncher` 里 fetch 或 checkout。
7. 在 dart-fw 提交：先 `git add xrobot.lock`，再单独运行 `git commit -m "更新 DartLauncher"`。提交后同样核对。`git status --porcelain` 里如果还有别的文件，不要提交，写进汇报。

## 卡住就停
遇到下面任何一种情况，就停下写汇报，不要自己想办法绕过去：
- 主机测试没有 `PASS`
- 第 3 步或第 4 步有输出
- 联网失败
- git 提交后核对不上
- 需要改“允许改的文件”以外的东西
- 编译错误修了两轮还没好

## 汇报
命令里的 `-o codex-report.md` 会用你最后一条回复覆盖这个文件，所以下面这些都写在最后一条回复里：
- 改了哪些文件
- 主机测试的 `PASS` 数；第 3、4 步的结果
- `Modules\local\DartLauncher` 的提交号；固件编译有没有重新编译 `launcher_hw.cpp`；`DevC.elf` 的修改时间；`arm-none-eabi-size build\debug\DevC.elf` 的输出
- 两个仓库的提交号和 `git status --porcelain` 的输出；如果中途停下了，写停在哪一步
