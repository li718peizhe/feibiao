# 4j-yaw-m2006 本地提交与完整固件 ELF 构建报告

## 1. 隔离分支本地提交
- **仓库**：`C:/Users/Lenovo/orca/workspaces/DartLauncher/auto-4j-yaw-m2006`
- **分支**：`auto/4j-yaw-m2006`
- **Commit SHA**：`17566ccb17c63534685de8999f083feebe27a2f4`
- **提交信息**：`feat(yaw): adapt yaw axis to M2006 and auto-zero on power-up`
- **提交文件（8 个授权文件）**：
  - `README.md`
  - `src/dart_params.hpp`
  - `src/motor_axis.hpp`
  - `src/motor_axis.cpp`
  - `src/hw/launcher_hw.hpp`
  - `src/hw/launcher_hw.cpp`
  - `src/launcher_io.hpp`
  - `test/host_test.cpp`

## 2. `dart-fw` 同步与构建状态
- **构建命令**：`cmake --build --preset debug`（工作目录 `C:/Users/Lenovo/Desktop/xrobot/dart-fw`），0 错误完成编译与链接。
- **备份产物**：`C:/Users/Lenovo/Desktop/xrobot/dart-fw/build/DevC-before-4j.elf`（8,295,152 字节）
- **`dart-fw` `git status --short`**：
  ```text
   M Core/Src/main.c
   M User/RobotConfig/artisans_dart.yaml
  ?? build-4g-final.log
  ```
- **`Modules/local/DartLauncher` `git status --short`**：
  ```text
   M README.md
   M src/dart_params.hpp
   M src/hw/launcher_hw.cpp
   M src/hw/launcher_hw.hpp
   M src/launcher_io.hpp
   M src/motor_axis.cpp
   M src/motor_axis.hpp
   M test/host_test.cpp
  ```

## 3. `DevC.elf` 核验结果
- **路径**：`C:/Users/Lenovo/Desktop/xrobot/dart-fw/build/debug/DevC.elf`
- **文件大小**：`8362144` bytes
- **SHA-256**：`e1617fcef53a6faf51ef4b7b5ee9cb7eeb5a6ab94215f44a83039651ef42df95`
- **`arm-none-eabi-size`**：
  ```text
     text	   data	    bss	    dec	    hex	filename
   220608	    552	  98672	 319832	  4e158	build/debug/DevC.elf
  ```
