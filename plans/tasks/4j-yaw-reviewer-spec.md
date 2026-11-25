# 4j yaw 双端寻端回中代码审查第1轮

你是独立 reviewer，只读审查，不修改源码、测试、配置、计划或报告，不提交、不推送、不烧录，不派其他 agent。审查目标是用户批准的定稿：
`C:/Users/Lenovo/Desktop/xrobot/plans/tasks/4j-yaw-dual-end-final-v2.md`

目标工作区：`C:/Users/Lenovo/orca/workspaces/DartLauncher/auto-4j-yaw-m2006`；实际分支 `auto/4j-yaw-m2006`；基线/HEAD `e17b490e8ffbefedc08c96d8d74396ba257487de`；当前未提交 diff SHA256 `4308cd4673d5ca6a23bf4c245ca66fd195acdaae4b883a24f9cca53de4002d49`。worker报告：`C:/Users/Lenovo/Desktop/xrobot/plans/tasks/4j-yaw-dual-worker-primary-r1.md`。工作区有8个授权文件改动，另有 `build-host/` 验证证据；不要把证据目录误判为源码改动。

审查轮次=1。报告必须写入：`C:/Users/Lenovo/Desktop/xrobot/plans/tasks/4j-yaw-m2006-review-r1.md`，并在最后明确一行 `VERDICT: APPROVED` 或 `VERDICT: CHANGES_NEEDED`。若需改动，逐项列出严重级别 P0/P1/P2、文件/行号、复现或理由、最小修复建议；不要替协调器改等级。若通过，仍列出未构建完整ELF、未烧录及现场验证风险，但不能因此把满足审查的实现判为未通过。

重点审查：

1. CAN2/C610 ID2/0x202是否保持，kYawInstalled启用是否与4i摆臂/推杆关闭、4g拉力、pitch/滑块配置相容；是否错误改了禁改模块。
2. MotorAxis双端阶段顺序：SeekFirst、BackoffFirst、SeekSecond、BackoffSecond、Centering；是否以新鲜反馈+命令电流+速度+位置窗口判断接触；是否能在起点位于端点、非对称端点、负多圈和编码器跨圈下正确计算原始counts中点。
3. 退离方向、退离超时/卡阻、跨度4~90圈校验、超程/总超时/回中阻塞故障、动态边界和重复回零是否安全；不自动升流；非法/过期反馈不能误认端点。
4. enabled=false、online=false、Stop、取消、ClearFault、重连是否清未完成阶段且不自启动；Homed是否只在中点稳定后置位。
5. LauncherHardware集成：Manual/SetYaw/Aimed/StopMotors/OnMonitor单位是否统一为输出轴rad/rev，是否仍有YawGeometry旧12°路径；MatchFlow/4f/3c是否等待yaw真正回中再启动滑块；滑块托持和既有安全门控没有削弱。
6. 检查主机动态测试是否真实覆盖定稿要求，不接受只改断言/只跑旧回归；核对worker报告中的 `PASS: 2335266 checks`、`PASS: 2193 HX711 checks`、25个ARM源码单元退出0是否与工作区证据路径一致、是否可复核。不要运行会修改工作区的命令；可做只读哈希/差异检查。
7. 检查未提交diff是否只包含授权的8个文件，是否有明显编译遗漏、未定义字段、越界/未初始化状态、日志并发读写或单位混淆。完整ELF未构建是交付后续门禁，不等同于代码审查失败；若源代码审查发现必须靠完整链接才能确认的风险，明确写出来。

审查证据与报告完成后，向协调器回报：报告绝对路径、VERDICT、P0/P1/P2问题清单、审查覆盖和未确认项。卡住就停止并说明具体只读阻塞，不要修改任何文件。

Placement: this machine (local), set up on this machine.
