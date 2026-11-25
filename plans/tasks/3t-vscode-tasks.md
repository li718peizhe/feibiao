# 任务 3t：VS Code 编译与烧录快捷任务

目标：在打开 xrobot 或 dart-fw 文件夹时，默认构建任务编译当前 DartLauncher 固件，并提供 OpenOCD 烧录和顺序编译烧录任务。
范围：根目录 `.vscode/tasks.json`、`.vscode/settings.json`，固件仓库 `.vscode/tasks.json`；不改固件代码，不执行硬件烧录。
成功标准：配置可解析、依赖指向有效任务、默认构建为当前 CMake debug 配置、对应配置和构建命令成功、烧录路径指向现有工具与 ELF。

## 进度

1. 已完成：检查当前分支 dart-launcher、干净工作区和原有 VS Code 配置；根目录任务默认指向旧 Keil 工程，固件任务使用 STM32CubeProgrammer。
2. 已完成：增加当前 CMake 配置、默认编译、OpenOCD 烧录与顺序编译烧录任务；根目录 CMake 源路径改为 dart-fw；保留根目录旧 Keil 任务，取消其默认构建身份。
3. 已完成：两份 JSON 解析、依赖标签、默认构建、工作目录和工具/固件路径检查通过；按任务参数执行配置与编译退出 0；暂存差异审查通过，在 dart-launcher 提交 `5c1cf2d7a8a39dc71132cb9b893364301c348913`（配置 VS Code 编译与 OpenOCD 烧录任务），工作区干净；根目录无 Git，直接保存。

## 验证结果

- 构建日志：`dart-fw/build/3t-vscode-build.log`；沿用现有 enable_language 顺序和 Eigen CMP0146 警告，配置与编译成功。
- OpenOCD 只执行配置文件查找，CMSIS-DAP 和 STM32F4 配置均可解析，退出 0；未连接目标、未执行烧录。
- ELF 哈希保持 `C4BFBB047CF673F3197BD8065132F12EE35030BACA22C57AE6AA0891B89B2985`；本轮没有固件代码变更，未重跑主机逻辑测试。
- Referee 原有三个修改保留，未 push。

现场待验证：VS Code 快捷键触发任务、OpenOCD 实机烧录与校验。
