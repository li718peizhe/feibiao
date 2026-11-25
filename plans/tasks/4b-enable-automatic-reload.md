# 任务 4b：打开自动换弹（2026-11-14）

## 背景

4a 烧录后用户反馈"现在自动装填大概没问题了"，独立自动装填现场初步通过。现在打开 `kLoadCalibrated=true`，启用自动换弹功能，允许连发模式执行完整的"第一发上膛→发射→自动换弹→第二发发射"流程。

## 为什么改

用户有两发镖，需要通过单次遥控触发自动执行两发的连续操作。现有机械和控制参数（放镖角、推杆到位、等待时间、继电器时序）已在 4a 验证通过；只需打开软件开关即可启用。

## 要改的文件

- `xrobot-local/DartLauncher/src/dart_params.hpp`：`kLoadCalibrated` 改为 `true`
- `xrobot-local/DartLauncher/test/host_test.cpp`：更新两处测试检查

## 改动内容

### dart_params.hpp

第 257 行：
```cpp
// 改前
constexpr bool kLoadCalibrated = false;

// 改后
constexpr bool kLoadCalibrated = true;
```

### host_test.cpp

第 258 行：
```cpp
// 改前
Check(!kReloadInstalled, "automatic reload is not installed");

// 改后
Check(kReloadInstalled, "automatic reload is enabled after independent load passed field test on 2026-11-14");
```

第 284-285 行：
```cpp
// 改前
Check(kMagnetInstalled && !kLoadCalibrated && !kReloadInstalled,
      "automatic reload stays off until the load pulse and rise height are measured");

// 改后
Check(kMagnetInstalled && kLoadCalibrated && kReloadInstalled,
      "automatic reload is enabled after independent load passed field test on 2026-11-14");
```

## 验证步骤

1. **主机测试**（在 `xrobot-local/DartLauncher` 目录）：
   ```powershell
   $env:PATH = "C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;" + $env:PATH
   $src = (Get-ChildItem src\*.cpp).FullName
   g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test\host_test.cpp $src -o build-host\host_test.exe
   .\build-host\host_test.exe
   ```
   预期：`PASS: 30517 checks`

2. **编译固件**（在 `dart-fw` 目录）：
   ```powershell
   cmake --build --preset debug
   Get-FileHash build\debug\DevC.elf -Algorithm SHA256
   ```
   预期：编译通过，产生新 ELF，无新的编译警告

## 烧录命令

```powershell
& "C:\Users\Lenovo\.eide\tools\openocd_7a1adfbec_mingw32\bin\openocd.exe" -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg -c "program C:/Users/Lenovo/Desktop/xrobot/dart-fw/build/debug/DevC.elf verify reset exit"
```

看到 `** Verified OK **` 才算烧好。

## 现场验证

烧录后复位，按以下顺序操作：

1. 回零：右拨杆中、左拨杆中，拨轮往上 2 s
2. 瞄准：左拨杆上，右摇杆调到要打的位置
3. 取第一发镖：左拨杆下，拨轮往上 2 s
4. **连发两发**：
   - 右拨杆拨到上（自动模式）
   - 左拨杆从中拨到下（连发手势）
   - 观察串口状态流，记录：
     * 第一发上膛是否完成（`step=` 和 `latch=`）
     * 第一发是否正常发射（`darts=` 计数）
     * 自动换弹是否执行（各步骤的进度）
     * 第二发是否正常发射
     * 每一发的落点位置

## 验收标准

- ✓ 主机测试全部 PASS
- ✓ 固件编译通过，无新警告
- ✓ 烧录校验通过
- ✓ 两发都能正常取上、发射
- ✓ 落点稳定、重复性可接受

## 待观察

- 分侧成功率（两侧各测几次）
- 连发过程中是否有故障或超时
- 推杆到底与继电器通电的先后顺序是否正确
- 自动换弹各步骤的时序和流程稳定性

## 卡住就停

如果现场验证过程中出现以下情况，停下来上报：
- 任意一发未能正常上膛或发射
- 连发中出现 `fault=` 故障
- 串口日志显示流程卡在某个步骤
- 落点明显偏离预期或极其不稳定
