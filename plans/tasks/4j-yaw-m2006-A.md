# 4j yaw M2006 接入——主对话独立方案 A

日期：2026-11-24。事项：主对话 item 5。本文件是独立草案，不是 worker 执行定稿；尚未读取 planner B。

## 目标与当前事实

用户确认 yaw 的 M2006 已安装，要求明确接线，并由主对话与 planner 各自设计后合并，交 worker 实现。

- `dart-fw/User/RobotConfig/artisans_dart.yaml:38-45,75-82` 已配置 yaw 为 CAN2、M2006、反馈 514（0x202），即 C610 电调编号 2；CAN1 上左滑块同为 0x202，不能将 yaw 直接接 CAN1。
- 接线是 C 板标注 CAN2 的 H 接电调 CAN H，L 接 CAN L；电调电源另接整车供电。电调编号按实际 C610 手册设为 2，不能按颜色或未经核实的插针顺序接线。
- `handoff.md:238-239` 与 README 的接口表同样记录 CAN2；C 板已有 CAN 收发器，不需要另装上一篇通用回复中的收发器。
- `src/dart_params.hpp:243` 当前 `kYawInstalled=false`；摆臂及推杆保持 false。不是缺一套 HAL CAN 驱动，应复用现有 RMMotor、MotorPort、MotorAxis、YawGeometry。
- `src/hw/launcher_hw.cpp:42-45` 轴单位为滑台 m，36:1、4 mm 导程，机械角显示采用旧几何 `s=0.6045*sin(angle)`，软件角范围 0~12°；不能视为新机构已经测准。
- `src/hw/launcher_hw.cpp:714-752` 已有瞄准组同时回 yaw/pitch、左摇杆横向 yaw 控制、未回零只许向回零方向点动。
- yaw 当前没有 `hold_on_release`；手动每次零输入会进入速度零模式，不能复用 pitch 的松手位置保持效果。

## 方案 A

1. 保持 YAML 中 yaw 的 CAN2 与 0x202，不另写 HAL 回调或第二套发送循环。
2. 打开 `kYawInstalled`，同步安装状态测试与 README，修正文档中 pitch 旧的 CAN2 注释，以当前 YAML 的 CAN1 为准。
3. `YawConfig()` 使用已有配置再置 `hold_on_release=true`。复用已有 MotorAxis 保持分支，不移植摆臂的重力补偿、摩擦补偿或保持积分逻辑。无力、故障、遥控离线仍按既有 yaw 停机规则处理。
4. 为 yaw 明确设置有限的回零搜索距离，建议采用 `kYawTravel + 0.010 m`，沿用现有 MotorAxis 超行程故障入口；不要更改 pitch 或滑块配置。
5. 初始回零速度、回零电流比例、PID、0~12°范围及几何沿用现有值，不将未经实测的参数声称为已标定。定稿前比较 B 对首次低速试车参数的建议。
6. 复核 RMMotor 当前 CAN 分组发送：未安装摆臂仍每拍 Send 零命令，yaw 控制组按 0x200 第二槽写入；总线和组不能混淆。只在发现真实接入缺陷时提出定稿修改，不顺手重构依赖库。
7. 现有主状态日志已能显示 yaw 在线、回零与角度。若实际无法据此验方向/回零，则定稿允许增补简短 yaw cmd/act/rpm 观测行；先核对 M2006 的实际反馈定义，不把保留字节写成温度。

## 候选授权修改文件

- `xrobot-local/DartLauncher/src/dart_params.hpp`
- `xrobot-local/DartLauncher/src/hw/launcher_hw.cpp`
- `xrobot-local/DartLauncher/test/host_test.cpp`
- `xrobot-local/DartLauncher/README.md`
- 集成产物：通过现有工具同步 `dart-fw/Modules/local/DartLauncher`，由 `xrobot setup` 维护 `dart-fw/xrobot.lock`，不得手改锁文件。
- 定稿及最终交付时更新 `handoff.md` 当前状态和下一步，不改历史调试记录。

不要求改 YAML 总线绑定、BSP CAN 初始化、发射/拉力控制或摆臂推杆开关。发现超出清单的必要修改先停下汇报。

## 已完成的只读预检

- 源模块 `git status --porcelain` 无输出，`git remote -v` 无输出（本地仓库）。
- dart-fw remote 是 `https://github.com/QDU-Robomaster/bsp-dev-c.git`。
- dart-fw 已有用户/历史未提交修改：`Core/Src/main.c` 的 USART1=9600、YAML 的 `tension_uart: usart1`；另有未跟踪 `build-4g-final.log`。全部保留，不纳入此次独立修改。
- 现有 graphify 图查询命中 MotorAxis/MotorPort，但混有旧工程且结果截断；上述实现判断已回查当前源码，不以旧图作为配置唯一依据。
- 本轮只做本地交付，不申请 PR、不 push、不提交，除非用户后续明确授权提交。

## 验证要求

定稿必须给出可执行命令。主机已有测试可按以下方式运行（路径依当前工程）：

```powershell
$env:PATH = 'C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;' + $env:PATH
$src = (Get-ChildItem src\*.cpp).FullName
.\build-host\host_test.exe
```

工作目录为 `C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher`。测试覆盖实际需求：安装状态、yaw 配置的回零方向/范围与限流、回零后双向微调与松手保持、搜索超行程故障、离线停机；保留原滑块托持和 pitch 回归。适配层不在主机编译范围内，必须固件编译。

模块同步须先核实当前工具能否将未提交本地源码安全带入构建；若只能同步提交而用户未授权提交，停下报告，不偷改 lock、不直接覆盖已存在模块工作区。不得把旧 ELF 的编译通过当新实现验证。

固件工作目录 `C:\Users\Lenovo\Desktop\xrobot\dart-fw`：`cmake --build --preset debug`。报告模块内容来源、完整构建结果和 ELF SHA-256，保留当前镜像备份；不烧录。

## 首次上车顺序（代码完成后分阶段指导）

1. 不驱动电机，先确认接 CAN2、电调编号 2，读取日志检查 yaw 在线。
2. 接入新版固件后，用瞄准组小幅朝回零方向点动，观察机械方向；若方向相反，按工程约定改 RMMotor reverse，不改 MotorAxis 符号。
3. 确认回零端确有可用挡块，再执行现有瞄准组回零手势（会同时动 pitch），验证 yaw 找端后退开。
4. 回零后只作小角度左右微调、松手观察，再测范围；记录实际角度与日志，后续才能回填几何和参数。

## 定稿前需处理的确认点

- 旧资料的 0~12°单侧范围、4 mm 导程和旧几何仍未由本次安装确认；先小范围验收，不以安装完成等同机械标定完成。
- 若 B 提出改变回零手势、停机保持、机械角范围或电流上限等已有行为，合并方案须明确提请用户确认。

卡住就停下汇报，不要自己乱改。
