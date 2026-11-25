# 串口日志逐组提取：00:11:26–00:11:45（左待命位那一段）

只从已提供的原始日志中提取，未重新操作、未补造字段。本文件只做记录，不选 kStandby 候选值，不对自行移动的原因做判断。

## 0. 提取口径

- **拼接规则**：以 `Dart ` 开头为一行；被拆成 `D` + `art ...` 的片段按原顺序拼回，行首时间戳取该行**起始片段**的时间戳。
- **组边界**：固件每秒输出一组（mode / step / remote / sw / online / arm / pitch / arm cmd / ref）。串口工具按到达时间打戳，行被拆段时会把尾部推到下一秒，所以**某组的 mode 行时间戳可能比该组 arm 行早 1 秒**（如 00:11:25 的 mode 行 + 00:11:26 的 arm 行属于同一组）。下表"组"以 `Dart arm=` 行的实际时间戳标注，并另列该组 mode 行的实际时间戳。
- 行在导出里丢失时间戳前缀的，标 **时间戳缺失**（行文本完整）；某秒没有独立组的，标 **无独立组**。
- 本窗口恒定值（逐组核对，无例外）：`crank=1325`、`mag=0`、`online=1/1/1/0/1`、`sl=3.13/3.13mm`、`home=0/0/0/0/1`、`darts=1`、`latch=0`、`remote clr=0 bad=0`、`wheel` 除 00:11:28 外均为 0.00。

## 1. 逐组数据表

| # | 组（arm 行时间） | mode 行时间 | mode | fault | step | sticks（4 个） | arm ° | raw | cmd | act | rpm | tmp | stall |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 00:11:26 | 00:11:25 | 1 | 无故障 | 空闲 | 0.00 0.00 0.00 0.00 | −41.18 | 1005 | −6509 | −6494 | 0 | 25 | 0 |
| 2 | 00:11:27 | 00:11:27 | 1 | 无故障 | 空闲 | 0.00 0.00 0.00 0.00 | −41.18 | 1005 | −6509 | −6511 | 0 | 26 | 0 |
| 3 | 00:11:28 | 00:11:28 | 1 | 无故障 | 空闲 | 0.00 0.00 **0.15** 0.00 | −37.40 | 1091 | −1616 | −1595 | 0 | 26 | 0 |
| 4 | 00:11:29 | 00:11:29 | 1 | 无故障 | 空闲 | 0.00 0.00 0.00 0.00 | −36.65 | 1108 | −2112 | −2129 | 0 | 26 | 0 |
| 5 | 00:11:30 | 00:11:30 | 1 | 无故障 | 空闲 | 0.00 0.00 0.00 0.00 | −36.65 | 1108 | −2605 | −2627 | 0 | 26 | 0 |
| 6 | 00:11:31 | 00:11:31 | 1 | 无故障 | 空闲 | 0.00 0.00 0.00 0.00 | −36.74 | 1106 | −3071 | −3073 | 0 | 26 | 0 |
| 7 | 00:11:33 | 00:11:32 | 1 | 无故障 | 空闲 | 0.00 0.00 0.00 0.00 | −36.77 | 1105 | −3491 | −3473 | 0 | 26 | 0 |
| — | 00:11:33 | — | — | — | — | — | 无独立组（只有第 7 组的 arm/pitch/arm cmd/ref 落在此秒） | | | | | | |
| 8 | 00:11:34 | 00:11:34 | 1 | 无故障 | 空闲 | 0.00 0.00 0.00 0.00 | −35.59 | 1132 | −1315 | −1331 | 0 | 26 | 0 |
| 9 | 00:11:35 | 00:11:35 | 1 | 无故障 | 空闲 | 0.00 0.00 0.00 0.00 | −31.68 | 1221 | −2632 | −2626 | 0 | 26 | 0 |
| 10 | 00:11:36 | 00:11:36 | 1 | 无故障 | 空闲 | 0.00 0.00 0.00 0.00 | −30.28 | 1253 | −2464 | −2455 | 0 | 26 | 0 |
| 11 | 00:11:37 | 00:11:37 | 1 | 无故障 | 空闲 | 0.00 0.00 0.00 0.00 | −26.76 | 1333 | −771 | −765 | 0 | 26 | 0 |
| 12 | 00:11:38 | 00:11:38 | 1 | 无故障 | 空闲 | 0.00 0.00 0.00 0.00 | −26.80 | 1332 | −1274 | −1271 | 0 | 26 | 0 |
| 13 | 00:11:39（时间戳缺失） | 00:11:39 | 1 | 无故障 | 空闲 | 0.00 0.00 0.00 0.00 | −29.44 | 1272 | −3420 | −3547 | **2** | 26 | 0 |
| — | 00:11:40 | — | — | — | — | — | 无独立组（只有第 13 组的 arm cmd/ref 到达） | | | | | | |
| 14 | 00:11:41 | 00:11:41 | 1 | 无故障 | 空闲 | 0.00 0.00 0.00 0.00 | −28.29 | 1298 | −2834 | −2837 | 0 | 26 | 0 |
| 15 | 00:11:42 | 00:11:42 | 1 | 无故障 | 空闲 | 0.00 0.00 0.00 0.00 | −28.29 | 1298 | −2834 | −2837 | 0 | 26 | 0 |
| 16 | 00:11:43 | 00:11:43 | 1 | 无故障 | 空闲 | 0.00 0.00 0.00 0.00 | −28.29 | 1298 | −2834 | −2836 | 0 | 26 | 0 |
| 17 | 00:11:44（时间戳缺失） | 00:11:44 | 1 | 无故障 | 空闲 | 0.00 0.00 0.00 0.00 | −28.29 | 1298 | −2834 | −2846 | 0 | 26 | 0 |
| 18 | 00:11:45 | 00:11:45 | 1 | 无故障 | 空闲 | 0.00 0.00 0.00 0.00 | −28.29 | 1298 | −2834 | −2837 | 0 | 26 | 0 |

## 2. 三个阶段汇总（仅统计，不解释原因）

| 阶段 | 组 | 时间 | arm 范围 | raw 范围 | 极差 | sticks 第 3 通道 | cmd 变化 |
|---|---|---|---|---|---|---|---|
| ① 静止平台（约 −36.7°） | 4–7 | 00:11:29–00:11:33 | −36.65 ~ −36.77 | 1108→1105 | 0.12° | 全 0.00 | −2112→−3491（逐秒变负） |
| ② 摇杆回中后的自行移动 | 8–13 | 00:11:34–00:11:39 | −35.59 → −26.76 → −29.44 | 1132→1333→1272 | 8.83°（升）/2.68°（回） | 全 0.00 | −1315→−3420，唯一一次 rpm=2 |
| ③ 静止平台（约 −28.29°） | 14–18 | 00:11:41–00:11:45 | −28.29（5 读相同） | 1298（5 读相同） | 0.00° | 全 0.00 | 恒 −2834 |

## 3. 逐组原始行（拼接后）

```
# 组 1（arm 行 00:11:26，mode 行在 00:11:25；sw 行跨 25→26 拆分）
[00:11:25] Dart mode=1 fault=无故障 darts=1 home=0 latch=0
[00:11:25] Dart step=空闲
[00:11:25→26] Dart sw R=M L=D sticks 0.00 0.00 0.00 0.00 wheel 0.00
[00:11:26] Dart arm=-41.18 raw=1005 lock=0.00deg/1500 crank=1325 mag=0 tick 419/543us late=0
[00:11:26] Dart arm cmd=-6509 act=-6494 rpm=0 tmp=25 stall=0 bus=465144

# 组 2（00:11:27）
[00:11:27] Dart mode=1 fault=无故障 darts=1 home=0 latch=0
[00:11:27] Dart step=空闲
[00:11:27] Dart sw R=M L=D sticks 0.00 0.00 0.00 0.00 wheel 0.00
[00:11:27] Dart arm=-41.18 raw=1005 lock=0.00deg/1500 crank=1325 mag=0 tick 419/544us late=0
[00:11:27] Dart arm cmd=-6509 act=-6511 rpm=0 tmp=26 stall=0 bus=466304

# 组 3（00:11:28，摇杆第 3 通道 0.15）
[00:11:28] Dart mode=1 fault=无故障 darts=1 home=0 latch=0
[00:11:28] Dart step=空闲
[00:11:28] Dart sw R=M L=D sticks 0.00 0.00 0.15 0.00 wheel 0.00
[00:11:28] Dart arm=-37.40 raw=1091 lock=0.00deg/1500 crank=1325 mag=0 tick 421/543us late=0
[00:11:28] Dart arm cmd=-1616 act=-1595 rpm=0 tmp=26 stall=0 bus=467464

# 组 4（00:11:29，阶段① 起点）
[00:11:29] Dart mode=1 fault=无故障 darts=1 home=0 latch=0
[00:11:29] Dart step=空闲
[00:11:29] Dart sw R=M L=D sticks 0.00 0.00 0.00 0.00 wheel 0.00
[00:11:29] Dart arm=-36.65 raw=1108 lock=0.00deg/1500 crank=1325 mag=0 tick 437/652us late=0
[00:11:29] Dart arm cmd=-2112 act=-2129 rpm=0 tmp=26 stall=0 bus=468624

# 组 5（00:11:30）
[00:11:30] Dart mode=1 fault=无故障 darts=1 home=0 latch=0
[00:11:30] Dart step=空闲
[00:11:30] Dart sw R=M L=D sticks 0.00 0.00 0.00 0.00 wheel 0.00
[00:11:30] Dart arm=-36.65 raw=1108 lock=0.00deg/1500 crank=1325 mag=0 tick 427/559us late=0
[00:11:30] Dart arm cmd=-2605 act=-2627 rpm=0 tmp=26 stall=0 bus=469784

# 组 6（00:11:31）
[00:11:31] Dart mode=1 fault=无故障 darts=1 home=0 latch=0
[00:11:31] Dart step=空闲
[00:11:31] Dart sw R=M L=D sticks 0.00 0.00 0.00 0.00 wheel 0.00
[00:11:31] Dart arm=-36.74 raw=1106 lock=0.00deg/1500 crank=1325 mag=0 tick 429/547us late=0
[00:11:31] Dart arm cmd=-3071 act=-3073 rpm=0 tmp=26 stall=0 bus=470944

# 组 7（mode 行 00:11:32，arm 行 00:11:33）
[00:11:32] Dart mode=1 fault=无故障 darts=1 home=0 latch=0
[00:11:32] Dart step=空闲
[00:11:32] Dart sw R=M L=D sticks 0.00 0.00 0.00 0.00 wheel 0.00
[00:11:33] Dart arm=-36.77 raw=1105 lock=0.00deg/1500 crank=1325 mag=0 tick 430/548us late=0
[00:11:33] Dart arm cmd=-3491 act=-3473 rpm=0 tmp=26 stall=0 bus=472104

# 组 8（00:11:34，阶段② 起点；arm cmd 行无时间戳前缀）
[00:11:34] Dart mode=1 fault=无故障 darts=1 home=0 latch=0
[00:11:34] Dart step=空闲
[00:11:34] Dart sw R=M L=D sticks 0.00 0.00 0.00 0.00 wheel 0.00
[00:11:34] Dart arm=-35.59 raw=1132 lock=0.00deg/1500 crank=1325 mag=0 tick 431/553us late=0
[时间戳缺失] Dart arm cmd=-1315 act=-1331 rpm=0 tmp=26 stall=0 bus=473264

# 组 9（00:11:35）
[00:11:35] Dart mode=1 fault=无故障 darts=1 home=0 latch=0
[00:11:35] Dart step=空闲
[00:11:35] Dart sw R=M L=D sticks 0.00 0.00 0.00 0.00 wheel 0.00
[00:11:35] Dart arm=-31.68 raw=1221 lock=0.00deg/1500 crank=1325 mag=0 tick 440/653us late=0
[00:11:35] Dart arm cmd=-2632 act=-2626 rpm=0 tmp=26 stall=0 bus=474424

# 组 10（00:11:36）
[00:11:36] Dart mode=1 fault=无故障 darts=1 home=0 latch=0
[00:11:36] Dart step=空闲
[00:11:36] Dart sw R=M L=D sticks 0.00 0.00 0.00 0.00 wheel 0.00
[00:11:36] Dart arm=-30.28 raw=1253 lock=0.00deg/1500 crank=1325 mag=0 tick 434/643us late=0
[00:11:36] Dart arm cmd=-2464 act=-2455 rpm=0 tmp=26 stall=0 bus=475584

# 组 11（00:11:37）
[00:11:37] Dart mode=1 fault=无故障 darts=1 home=0 latch=0
[00:11:37] Dart step=空闲
[00:11:37] Dart sw R=M L=D sticks 0.00 0.00 0.00 0.00 wheel 0.00
[00:11:37] Dart arm=-26.76 raw=1333 lock=0.00deg/1500 crank=1325 mag=0 tick 427/546us late=0
[00:11:37] Dart arm cmd=-771 act=-765 rpm=0 tmp=26 stall=0 bus=476744

# 组 12（00:11:38）
[00:11:38] Dart mode=1 fault=无故障 darts=1 home=0 latch=0
[00:11:38] Dart step=空闲
[00:11:38] Dart sw R=M L=D sticks 0.00 0.00 0.00 0.00 wheel 0.00
[00:11:38] Dart arm=-26.80 raw=1332 lock=0.00deg/1500 crank=1325 mag=0 tick 437/649us late=0
[00:11:38] Dart arm cmd=-1274 act=-1271 rpm=0 tmp=26 stall=0 bus=477904

# 组 13（mode/step 行 00:11:39；sw 与 arm 行无时间戳前缀；arm cmd 行 00:11:40）
[00:11:39] Dart mode=1 fault=无故障 darts=1 home=0 latch=0
[00:11:39] Dart step=空闲
[时间戳缺失] Dart sw R=M L=D sticks 0.00 0.00 0.00 0.00 wheel 0.00
[时间戳缺失] Dart arm=-29.44 raw=1272 lock=0.00deg/1500 crank=1325 mag=0 tick 443/654us late=0
[00:11:40] Dart arm cmd=-3420 act=-3547 rpm=2 tmp=26 stall=0 bus=479064

# 组 14（00:11:41，阶段③ 起点）
[00:11:41] Dart mode=1 fault=无故障 darts=1 home=0 latch=0
[00:11:41] Dart step=空闲
[00:11:41] Dart sw R=M L=D sticks 0.00 0.00 0.00 0.00 wheel 0.00
[00:11:41] Dart arm=-28.29 raw=1298 lock=0.00deg/1500 crank=1325 mag=0 tick 431/658us late=0
[00:11:41] Dart arm cmd=-2834 act=-2837 rpm=0 tmp=26 stall=0 bus=480224

# 组 15（00:11:42）
[00:11:42] Dart mode=1 fault=无故障 darts=1 home=0 latch=0
[00:11:42] Dart step=空闲
[00:11:42] Dart sw R=M L=D sticks 0.00 0.00 0.00 0.00 wheel 0.00
[00:11:42] Dart arm=-28.29 raw=1298 lock=0.00deg/1500 crank=1325 mag=0 tick 422/551us late=0
[00:11:42] Dart arm cmd=-2834 act=-2837 rpm=0 tmp=26 stall=0 bus=481384

# 组 16（00:11:43）
[00:11:43] Dart mode=1 fault=无故障 darts=1 home=0 latch=0
[00:11:43] Dart step=空闲
[00:11:43] Dart sw R=M L=D sticks 0.00 0.00 0.00 0.00 wheel 0.00
[00:11:43] Dart arm=-28.29 raw=1298 lock=0.00deg/1500 crank=1325 mag=0 tick 421/551us late=0
[00:11:43] Dart arm cmd=-2834 act=-2836 rpm=0 tmp=26 stall=0 bus=482544

# 组 17（00:11:44；arm 行无时间戳前缀）
[00:11:44] Dart mode=1 fault=无故障 darts=1 home=0 latch=0
[00:11:44] Dart step=空闲
[00:11:44] Dart sw R=M L=D sticks 0.00 0.00 0.00 0.00 wheel 0.00
[时间戳缺失] Dart arm=-28.29 raw=1298 lock=0.00deg/1500 crank=1325 mag=0 tick 421/551us late=0
[00:11:44] Dart arm cmd=-2834 act=-2846 rpm=0 tmp=26 stall=0 bus=483703

# 组 18（00:11:45；arm cmd 行无时间戳前缀）
[00:11:45] Dart mode=1 fault=无故障 darts=1 home=0 latch=0
[00:11:45] Dart step=空闲
[00:11:45] Dart sw R=M L=D sticks 0.00 0.00 0.00 0.00 wheel 0.00
[00:11:45] Dart arm=-28.29 raw=1298 lock=0.00deg/1500 crank=1325 mag=0 tick 421/551us late=0
[时间戳缺失] Dart arm cmd=-2834 act=-2837 rpm=0 tmp=26 stall=0 bus=484863
```

## 4. 缺失与断组清单

| 位置 | 情况 |
|---|---|
| 00:11:33 | 无独立组（无 mode/step/remote/sw/online 行）；本秒只有组 7 的 arm / pitch / arm cmd / ref |
| 00:11:40 | 无独立组；本秒只有组 13 的 arm cmd / ref |
| 00:11:34 | `Dart arm cmd=-1315 ...` 行导出时无时间戳前缀 |
| 00:11:37 | `Dart online=...` 行无时间戳前缀（行文本完整） |
| 00:11:39 | `Dart sw ...` 与 `Dart arm=-29.44 ...` 两行无时间戳前缀 |
| 00:11:44 | `Dart arm=-28.29 ...` 行无时间戳前缀 |
| 00:11:45 | `Dart arm cmd=-2834 ...` 行无时间戳前缀 |
| 00:11:26 组 | mode/step/remote 行时间戳为 00:11:25；sw 行跨 25→26 拆段 |

## 5. 未做的事

- 未选 kStandby 候选值。
- 未把阶段②的移动归因为积分、重力、机械或其它原因。
- 未使用 00:11:26 之前的任何缓存数据。
