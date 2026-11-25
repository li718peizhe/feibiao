# 任务 2j：锁止舵机原始脉宽只在实测行程里调，唤醒先转到行程中间

只读这一个文件就够了。不要读 `plans/dart-fw-libxr.md`，也不要运行 graphify。做完写汇报，然后停下。

## 背景
- 工作区：`C:\Users\Lenovo\Desktop\xrobot`
- 模块仓库：`xrobot-local\DartLauncher\`（git，分支 main，当前 HEAD 0ae800f）
  - `src\`：纯逻辑，不 include LibXR、HAL、FreeRTOS
  - `src\hw\`：LibXR 适配层
  - `test\host_test.cpp`：主机测试，现在是 `PASS: 17457 checks`
- 固件工程：`dart-fw\`（git，分支 dart-launcher，当前 HEAD 56e5add）
  - 模块会从 `xrobot-local\DartLauncher` 检出到 `Modules\local\DartLauncher`。不要改这份副本，也不要手改 `xrobot.lock`。
- 代码规范：注释用中文；不新增宏；一行一条语句；写法和周围代码保持一致。

## 为什么要改
- 锁止舵机是众灵 PM60S，270°，500~2500 µs 对应 0~270°。`kLockServoCalibrated` 还是 false，所以走的是原始脉宽模式，在 `src\hw\launcher_hw.cpp` 的 `LauncherHardware::Manual()` 里：
  - 左摇杆第一次离开死区时，把脉宽设成 `kLockBootPulseUs`（现在是 1300 µs）并使能 PWM。
  - 之后按摇杆调脉宽，夹在 500~2500 µs。
- 2026-10-15 实车：舵盘重新装过以后，实测最大锁止是 1469 µs，最大张开是 2166 µs，脉宽变大是张开。
  - 1300 在锁止限位外面 169 µs，所以一唤醒，舵机就顶到锁止限位上。
  - 摇杆推过 1469 或 2166，也一样会顶住。
- 这只舵机顶住限位以后会卡死：一直出力、发热，500~2500 都不再响应，要断电才能恢复。手册上写着它没有防堵转。
- 改法：唤醒时转到实测行程的中间；原始脉宽只在实测行程里调。

## 要改的地方
1. `src\dart_params.hpp`：
   - `kLockBootPulseUs` 那一行（现在是 1300）换成下面三行。顺序不能变，第三行要用到前两行：
     ```cpp
     constexpr double kLockRawPulseMin = 1469; // µs，2026-10-15 舵盘重装后实测的最大锁止，未标定时原始脉宽不低于它。
     constexpr double kLockRawPulseMax = 2166; // µs，2026-10-15 实测的最大张开，脉宽变大是张开，未标定时原始脉宽不高于它。
     constexpr double kLockBootPulseUs = (kLockRawPulseMin + kLockRawPulseMax) / 2; // µs，首次唤醒转到实测行程中间；PM60S 顶住限位会卡死不响应。
     ```
   - 删掉 `kLockReleasePulseRef` 那一行。它是旧车 1611 µs 的参考值，`src` 里没有用到，换了舵机和舵盘以后已经不对了。
   - 其他常量都不动，尤其是 `kServoPulseMin`、`kServoPulseMax`：`servo_ramp.cpp` 标定以后的角度换算要用它们。
2. `src\hw\launcher_hw.cpp` 的 `LauncherHardware::Manual()`，锁止舵机未标定的那个分支（第 430~446 行附近）：
   - 第 433 行的注释改成：`// 首次非零输入时才使能通道，先转到实测行程中间，避免上电猛转，也不顶限位。`
   - 第 441 行的注释改成：`// 按 kLockPulseRate 调整脉宽，夹在实测行程内，摇杆推过头也不顶限位。`
   - 第 442~444 行 `Clamp(...)` 的上下限，从 `kServoPulseMin, kServoPulseMax` 改成 `kLockRawPulseMin, kLockRawPulseMax`。
   - 别的地方都不动。
3. `test\host_test.cpp` 第 186~191 行附近：
   - 删掉 `Check(kLockBootPulseUs == 1300, "lock boot pulse uses the old vehicle lock value");`，在原位置换成下面两条：
     ```cpp
     Check(kServoPulseMin <= kLockRawPulseMin && kLockRawPulseMin < kLockRawPulseMax &&
               kLockRawPulseMax <= kServoPulseMax, "raw lock travel is inside servo range");
     Check(kLockRawPulseMin < kLockBootPulseUs && kLockBootPulseUs < kLockRawPulseMax,
           "raw lock boot pulse stays inside the measured travel");
     ```
   - 删掉两条用到 `kLockReleasePulseRef` 的检查：`"raw lock release reference is within servo range"` 和 `"lock boot pulse is below release reference"`。
   - `"raw lock boot pulse is within servo range"` 和 `"raw lock pulse rate is positive"` 这两条保留。
4. `README.md` 第 56~66 行，锁止舵机未标定的那一段：
   - 把“首次输出 `kLockBootPulseUs=1300 µs`，以后按摇杆比例以最多 `kLockPulseRate=200 µs/s` 调脉宽，夹在 500~2500 µs。”改成：“首次输出 `kLockBootPulseUs`，也就是实测行程 `kLockRawPulseMin=1469 µs` 和 `kLockRawPulseMax=2166 µs` 的中间（约 1818 µs），以后按摇杆比例以最多 `kLockPulseRate=200 µs/s` 调脉宽，夹在这段实测行程内。”
   - 把“确认首次 1300 µs 的转动方向和幅度”改成“确认首次转到行程中间”。
   - 在这一段末尾加一段：“2026-10-15：锁止舵机是众灵 PM60S（270°，5~13 V）。舵盘重装后实测最大锁止 1469 µs、最大张开 2166 µs，脉宽变大是张开。顶住限位时舵机会卡死，不再响应信号，要断电才能恢复，所以原始模式也只在这段行程里调。以后重装舵盘，要重新量这两个数。”
   - 这一段别的句子不动。

## 允许改的文件
- `xrobot-local\DartLauncher\`：`src\dart_params.hpp`、`src\hw\launcher_hw.cpp`、`test\host_test.cpp`、`README.md`。
- `xrobot setup` 改动的 `dart-fw\xrobot.lock`，以及 xrobot 生成的文件。
- 其他文件都不碰。`kLockServoCalibrated` 保持 false。

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
   应该打印 `PASS: N checks`。删了 3 条、加了 2 条，N 应该是 17456。只要是 `PASS` 就行，把 N 写进汇报。
3. 框架引用检查。下面这条命令应该没有任何输出：
   ```powershell
   Select-String -Path src\*.hpp, src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'
   ```
4. 确认旧常量删干净了。下面这条命令也应该没有任何输出：
   ```powershell
   Select-String -Path src\*.hpp, src\*.cpp, src\hw\*.hpp, src\hw\*.cpp, test\*.cpp, README.md -Pattern 'kLockReleasePulseRef'
   ```
5. 在 DartLauncher 仓库提交：
   ```powershell
   git add src/dart_params.hpp src/hw/launcher_hw.cpp test/host_test.cpp README.md
   git commit -m "锁止舵机原始脉宽限在实测行程，唤醒转到行程中间"
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
7. 在 dart-fw 提交：先 `git add xrobot.lock`，再 `git commit -m "更新 DartLauncher"`。提交后同样核对。`git status --porcelain` 里如果还有别的文件，不要提交，写进汇报。

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
