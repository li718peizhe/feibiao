# 任务 3s：摆臂松手保持与右取镖微调

## 目标与范围

用户已授权修复松手后自行移动，并报告右侧取镖偏高约 0.2°。右取镖目标由 46.38° 改为 46.18°；左侧现场确认 −36.7° 有间隙、−28.3° 无间隙，右侧待命位尚未测量。
目标是复现向中间点动后松手的保持漂移，按最小控制改动修复；保持故障、离线、软限位和电流保护要求，验证后提交模块并编译固件。
文件范围：DartLauncher 的 `src/motor_axis.*`、必要的 `src/pid.*`、`src/dart_params.hpp`、`test/host_test.cpp`、`README.md`；固件仅同步生成 `xrobot.lock`；根目录任务与交接记录。
遥控器现场操作步骤仅在聊天输出，不在文档追加。无子代理、无 graphify、无安装、无烧录或 push。

## 进度

1. 已完成：检查模块 `main/0323ca8`、固件 `dart-launcher/31397c0`，开始时均干净；Referee 三个既存修改保留。复核 `DartLauncher_standby_001126-001145.md` 与现有控制和测试。
2. 已完成：复现向中间点动松手的漂移；修复为向中间松手保留原速度环积分、不把点动前馈并入保持积分；右取镖目标下调到 46.18°。
3. 已完成：新增回归在旧控制下失败，修复后完整主机回归 `PASS: 22072 checks`，平台依赖检查无输出、差异检查通过；单文件 ARM 和完整固件编译通过，两个仓库已本地提交。
4. 已完成：记录产物哈希、占用、提交与交接，现场复测阶段直接在聊天交付；实车松手保持、右取镖对准、右待命角与装填位置仍待现场验证。

## 证据与验证

- 实车日志：松手后第一平台 −36.65°～−36.77°，`cmd=-2112→-3491`；随后向中间移动到 −26.76°，终态 −28.29°，`stall=0`。串口是低频快照。
- 初步“保持时输出不低于重力前馈”候选使已有制动检查失败：`FAIL: stored support bleeds away when the arm surges past its target`。该输出限制已撤下，不进入最终实现。
- 复用现有 `ArmPlant`/`ArmRig`，重点验证双向点动松手、静摩擦和 rpm 量化、被挡住后松脱、MoveTo 与软限位。
- 基线主机回归为 `PASS: 22036 checks`。新增 `TestArmInwardRelease`：2 种惯量 × 左右方向 × 3 种点动速度，自动到取镖位后向中间点动 1 s，再松手保持 12 s，检查最大偏移小于 0.5°、未碰硬挡。
- 暂时使用旧 `HoldPosition` 判据运行新增回归，按预期失败：`FAIL: an inward jog released after fetching holds within half a degree for twelve seconds`。恢复修复后通过 `22072 checks`，原有制动、外侧松手支撑、堵转和 MoveTo 检查均通过。
- 诊断模型中原控制最大漂移约 1.20°，修复后约 0.22°；此为既有估计受力模型结果，实车角度与电流须复测。

## 产物与提交

- 模块提交：`dbfea3a836e675751bc95a6b5c043d69cf9666bf`，`main`，修复摆臂向中间点动松手保持并微调右取镖角。
- 固件提交：`f6f7013b96d5bfc5d0fdf3edcefa5dcff6fe9bf1`，`dart-launcher`，更新 DartLauncher：修复点动松手保持并微调右取镖角；仅由 `xrobot setup --update local/DartLauncher` 更新锁文件。
- 两个仓库及模块副本工作区干净；Referee 原有三个修改保留；未 push、未烧录。
- `cmake --build --preset debug` 退出 0，日志 `dart-fw/build/3s-build.log`，无编译警告；RAM 32472 B / 128 KB，CCMRAM 64 KB / 64 KB，FLASH 217700 B / 1 MB。
- ELF：`C:/Users/Lenovo/Desktop/xrobot/dart-fw/build/debug/DevC.elf`，2026-11-14 00:51:43，SHA-256 `C4BFBB047CF673F3197BD8065132F12EE35030BACA22C57AE6AA0891B89B2985`。
- 原 3r ELF 已备份 `dart-fw/build/DevC-0323ca8-before-3s.elf`，SHA-256 `25BA30E3967C5CC0406248A4ED8B6224892BF696616ECA269057269579BEEF25`。
- build-cmake 包装脚本预设查询失败：`python C:/Users/Lenovo/.agents/skills/build-cmake/scripts/cmake_builder.py --source C:/Users/Lenovo/Desktop/xrobot/dart-fw --list-presets`，`ModuleNotFoundError: No module named 'tool_config'`；使用已有 debug 预设直接构建通过，未安装依赖。
- 忽略目录内的诊断 harness 以 `-Werror` 编译曾因 `-Wsubobject-linkage` 失败；该诊断单独去掉 `-Werror` 运行。正式主机测试与 ARM 编译保留 `-Werror` 并通过。
