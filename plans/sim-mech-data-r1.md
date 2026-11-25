# 任务：仿真按机械回填数据更新（第一轮）

## 目标
把机械组 2026-10-11 回填的实测数据里已经确认的部分写进 MuJoCo 仿真（`simulation\dart_sim`）：
1. pitch 电机换成 M3508，pitch 丝杆导程改成 2 mm。
2. 按同步带和丝杆螺母的额定力，给滑块、yaw、pitch 三个电机轴加电流上限。
3. yaw 角度限制在 12° 以内。

其余回填值只写进注释。改完后全部测试通过。

## 背景（已确认）
- 工作区是 `C:\Users\Lenovo\Desktop\xrobot`，不是 git 仓库。测试命令：`python -m unittest discover -s simulation -p "test_*.py"`。
- 生成模型要用的 URDF 导出目录 `D:\download\local_mup2tyqw_1et5pw_urdf_stl` 已经不存在，所以不能重新生成模型，也不要运行 `python simulation\dart.py build`。已生成的模型在 `simulation\dart_sim\model\`，仿真和测试只读这里。需要导出目录的测试会自动跳过（`EXPORT_OK`）。
- 有几项参数是生成模型时写进 `model\dart_launcher.xml` 的，只改 `params.py` 不生效，要同步改 XML：
  - 电机型号对应丝杆关节的 `armature` 和电机执行器的 `ctrlrange`
  - 导程对应固定腱 `t_pitch_screw` 的系数

  M3508 的值直接照抄 XML 里的 `motor_r`：`armature="0.00368763"`、`ctrlrange="-6 6"`。
- 本轮要用的回填数据：
  - pitch：M3508，保留原装 19:1 减速箱。丝杆 BPGF-C7-12-2，导程 2 mm，传动比 1。
  - yaw：M2006。丝杆 SFUR1204，导程 4 mm，联轴器直连。yaw 极限 0~12°。
  - 滑块：两个 M3508，原装 19:1，S3M 40 齿带轮。节圆直径 = 40×3/π = 38.197 mm，半径 0.0191 m，和现在的值一样。机械填的 37.44 mm 是外径。
  - 丝杆螺母允许的最大推力 1000 N（yaw、pitch 都按这个），同步带允许的最大拉力 220 N（每根）。
- 本轮只写进注释、不改数值的回填（和旧 CAD 对不上，等新 CAD 重新导出或继续实测）：
  - 行程：滑块总行程 710 mm（仿真 700）；发射机构从锁住点到上止点 600 mm（仿真 500）；pitch 滑块行程 180 mm（仿真 250）。
  - yaw：滑台行程 310 mm；滑台和发射架用线轨加滑块连接；正对中线时滑台在 0 mm。
  - 皮筋：乳胶管 6 股，滚轮带轴承，绕法和仿真一样；自然长度填的是“内圈 650 / 外圈 800 mm”，上膛时比自然长度长约 600 mm；拉力曲线待测。
  - 换镖电机是 GM6020，具体驱动哪个机构待确认。
  - 和仿真一致的项：导轨仰角 37°、固定；摆臂摆动时飞镖一直和导轨平行。
  - 发射机构上止点有橡胶皮筋缓冲。

## 允许的改动
只改这 4 个文件：
- `simulation\dart_sim\params.py`
- `simulation\dart_sim\control.py`
- `simulation\dart_sim\model\dart_launcher.xml`：只改第 2 步列出的 3 行
- `simulation\test_dart_sim.py`

备份放在 `.backup\sim-mech-r1\`。其他文件一律不动。不运行 build，不开 GUI（console、demo、panel），不装任何包。

## 步骤

### 0. 检查和备份
```powershell
Set-Location C:\Users\Lenovo\Desktop\xrobot
python -m unittest discover -s simulation -p "test_*.py"
```
记下 Ran / skipped / failures 的数量。

然后确认 `simulation\dart_sim\model\dart_launcher.xml` 里下面 3 个字符串各出现正好 1 次：
- `<joint name="pitch_screw" type="hinge" axis="0 -0.7986355 -0.601815" armature="0.0007776" damping="0.0002" frictionloss="0.005"/>`
- `<joint joint="pitch_screw" coef="-0.0006366197724"/>`
- `<motor name="pitch" joint="pitch_screw" ctrlrange="-1.8 1.8"/>`

最后备份：
```powershell
New-Item -ItemType Directory -Force .backup\sim-mech-r1 | Out-Null
Copy-Item simulation\dart_sim\params.py, simulation\dart_sim\control.py, simulation\dart_sim\model\dart_launcher.xml, simulation\test_dart_sim.py .backup\sim-mech-r1\
```

### 1. params.py
- `pitch_motor` 改成 `"M3508"`，注释写“实车确认，保留原装 19:1 减速箱”。`yaw_motor` 加注释“实车确认 M2006”。
- `pitch_lead` 改成 `0.002`，注释写“BPGF-C7-12-2，导程 2 mm，直连（机械回填）”。
- `yaw_lead` 数值不变，注释改成“SFUR1204，导程 4 mm，联轴器直连（机械回填）”。
- `belt_radius` 数值不变，注释改成“S3M 40 齿，节圆直径 38.197 mm（机械填的 37.44 mm 是外径）”。
- 注释 `# 传动（待确认）` 改成 `# 传动（机械回填 2026-10-11）`。
- 在传动参数后面新增：
```python
    yaw_max_deg: float = 12.0                # 实车 yaw 极限（机械回填：0~12°，单边）
    # 受力上限（机械回填）。电流上限按这个换算，再乘 load_margin
    belt_force_max: float = 220.0            # 同步带允许最大拉力 N（每根）
    screw_force_max: float = 1000.0          # 丝杆螺母允许最大推力 N（yaw、pitch 都按这个）
    load_margin: float = 0.8
```
- `yaw_slide_range`、`pitch_range`、`slider_top`、`shuttle_top` 数值不变。在各自注释后面补上“背景”里对应的实车回填值，并注明“和旧 CAD 对不上，等新 CAD 重新导出后再改”。
- 皮筋三项数值不变。注释补上回填的规格，并写明拉力曲线待测，刚度和自然长度仍是估计值。
- 舵机/摆臂那段注释补一句：换镖电机是 GM6020，驱动哪个机构待确认，仿真暂时按舵机限速处理。
- 文件头的 docstring 补两件事：
  - 丝杆导程、带轮半径、电机型号已经按机械回填，皮筋、质量、舵机仍是估计值。
  - `belt_radius`、`yaw_lead`、`pitch_lead`、`yaw_motor`、`pitch_motor` 是生成模型时写进 XML 的。导出目录不在的时候改了这几项，要同步改 `model\dart_launcher.xml`，第 4 步新加的 `test_model_matches_params` 会检查两边是否一致。

### 2. dart_launcher.xml（只改这 3 行，缩进保持原样）
- pitch_screw 关节那一行：`armature="0.0007776"` 改成 `armature="0.00368763"`，其余属性不动。
- `<joint joint="pitch_screw" coef="-0.0006366197724"/>` 改成 `<joint joint="pitch_screw" coef="-0.0003183098862"/>`
- `<motor name="pitch" joint="pitch_screw" ctrlrange="-1.8 1.8"/>` 改成 `<motor name="pitch" joint="pitch_screw" ctrlrange="-6 6"/>`

### 3. control.py
- `GAINS["pitch"]` 改成 `((0.2, 0.0, 0.0, 15000, 0), (15.0, 0.5, 0.0, 16384, 16384))`，注释写“3508 以转子惯量为主，速度环先照搬滑块的”。
- 加上 `from .params import MOTORS`，并新增一个模块级函数：
```python
def current_cap(spec, force, lever):
    """机构允许的力 → 电流指令上限。lever 是输出轴每弧度的直线位移（带轮半径，或 导程/2π）。
    按传动效率 100% 算，实际的力只会更小。"""
    return int(force * lever / spec.kt * spec.cmd_max / spec.i_max)
```
- 在 `Controller.__init__` 里建 Axis 之前，算三个电流上限：
  - 滑块：`MOTORS["M3508"]`，力 `belt_force_max * load_margin`，lever 用 `belt_radius`
  - yaw：`MOTORS[p.yaw_motor]`，力 `screw_force_max * load_margin`，lever 用 `yaw_lead / (2π)`
  - pitch：同 yaw 的算法，换成 `pitch_motor` 和 `pitch_lead`

  传给 Axis 的增益里，速度环的最大输出和积分限幅都取 `min(原值, 上限)`。`GAINS` 字典本身不改。上限预期是：滑块 9179、yaw 2829、pitch 695。
- `yaw_range` 算完以后，再夹到 `±radians(p.yaw_max_deg)` 以内。

### 4. test_dart_sim.py
- `test_pitch_step` 里的 `s.run(4.0, ...)` 改成 `s.run(8.0, ...)`。注释写“2 mm 导程加 3508，空载约 16 mm/s，走 50 mm 要 3 s 以上”。
- 在 `TestMechanism` 里新增 3 个测试：
  - `test_model_matches_params`：检查模型里生成时写入的值和 params 一致。
    - `pitch_screw`、`yaw_screw`、`motor_r` 三个关节的 `m.dof_armature` 等于对应电机的 `rotor_inertia * ratio**2`，误差 1e-7
    - 执行器 `pitch`、`yaw`、`motor_r` 的 `ctrlrange` 上限等于 `kt * i_max`，精确到 4 位小数
    - `abs(pl.coef("c_pitch_screw")[1])` 和 `abs(pl.coef("c_yaw_screw")[1])` 等于各自的导程除以 2π，误差 1e-9
    - `abs(pl.coef("c_belt_r")[1])` 等于 `belt_radius`，误差 1e-6
  - `test_current_caps_respect_ratings`：四个电机轴（`slider_r`、`slider_l`、`yaw`、`pitch`）的 `loop.spd.max_out` 换算成机构上的力（`max_out * i_max / cmd_max * kt / lever`），都不超过对应的额定值。
  - `test_yaw_range_within_mech_limit`：`ctrl.yaw_range` 两端的绝对值都不超过 `radians(yaw_max_deg)`。

## 验证
```powershell
python -m unittest discover -s simulation -p "test_*.py"
Compare-Object (Get-Content .backup\sim-mech-r1\dart_launcher.xml) (Get-Content simulation\dart_sim\model\dart_launcher.xml)
```

## 验收标准
- 测试结果是 OK。跳过的数量和第 0 步一样，Ran 比第 0 步多 3。
- 三个电流上限是：滑块 9179、yaw 2829、pitch 695，允许 ±1。
- XML 和备份相比，只有第 2 步那 3 行不同（每行在 Compare-Object 里出现新旧各 1 条）。允许改动以外的文件没有变化。

## 卡住就停
遇到下面任何一种情况就停下，把原始输出写进汇报，不要自己想办法绕过去：
- 第 0 步的基线测试有失败，或者 XML 里那 3 个字符串不是各出现 1 次。
- 改完后 `test_pitch_step` 过不了：只允许把 pitch 速度环的 kp 改成 8 和 25 各试一次，还不过就停。
- 其他原有测试失败，尤其是四发连射、锁止、yaw 的测试。不要为了让测试通过去放宽阈值或改别的参数。
- 需要改允许范围以外的文件，或者需要重新生成模型。

## 回滚
```powershell
Copy-Item .backup\sim-mech-r1\params.py, .backup\sim-mech-r1\control.py simulation\dart_sim\ -Force
Copy-Item .backup\sim-mech-r1\dart_launcher.xml simulation\dart_sim\model\ -Force
Copy-Item .backup\sim-mech-r1\test_dart_sim.py simulation\ -Force
```

## 汇报（写进 codex-report.md）
- 改了哪些文件，每个改了什么
- XML 那 3 行改动前后的对比
- 三个电流上限的数值
- 改动前后测试的 Ran / skipped / failures
- 遇到的问题
