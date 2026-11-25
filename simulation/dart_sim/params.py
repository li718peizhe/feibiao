"""飞镖发射架仿真参数（新机械，来自 URDF 导出）。

丝杆导程、带轮半径、电机型号已经按机械回填，皮筋、质量、舵机仍是估计值。
注释里标了“待确认”。单位：m、kg、s、rad；名字带 _deg 的是角度。
大部分参数在加载模型时写入，改完不用重新生成模型；标了“生成时”的需要重新 build。
`belt_radius`、`yaw_lead`、`pitch_lead`、`yaw_motor`、`pitch_motor` 是生成模型时写进 XML 的。
导出目录不在的时候改了这几项，要同步改 `model\\dart_launcher.xml`，
第 4 步新加的 `test_model_matches_params` 会检查两边是否一致。
"""
from dataclasses import dataclass, field
import os

EXPORT_DIR = os.environ.get("DART_EXPORT_DIR", r"D:\download\local_mup2tyqw_1et5pw_urdf_stl")
DART_STEP = os.environ.get("DART_STEP", r"C:\Users\Lenovo\Downloads\机械镖 整.STEP")
MODEL_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "model")


@dataclass(frozen=True)
class MotorSpec:
    """DJI 电机 + 电调规格，转矩和转速都按减速箱输出轴计。"""
    name: str
    ratio: float          # 减速比（转子:输出）
    kt: float             # 转矩常数 N·m/A（输出轴，社区常用值）
    i_max: float          # 电调最大电流 A
    cmd_max: int          # 电调电流指令满量程
    noload_rpm: float     # 额定电压空载转速（输出轴）
    rated_torque: float   # 最大持续转矩 N·m
    rated_rpm: float      # 最大持续转矩下的转速（输出轴）
    rotor_inertia: float  # 转子转动惯量 kg·m²（转子侧，估计）

    @property
    def voltage_stall_torque(self):
        """由空载点和额定点外推的电压限制堵转转矩（输出轴）。"""
        return self.rated_torque / (1.0 - self.rated_rpm / self.noload_rpm)


# 规格来自 RoboMaster 官网（重量、空载转速、持续转矩）；kt 和转子惯量是估计值
MOTORS = {
    "M3508": MotorSpec("M3508", 3591 / 187, 0.3, 20.0, 16384, 482.0, 3.0, 469.0, 1.0e-5),
    "M2006": MotorSpec("M2006", 36.0, 0.18, 10.0, 10000, 500.0, 1.0, 416.0, 6.0e-7),
}


@dataclass
class Params:
    export_dir: str = EXPORT_DIR
    timestep: float = 0.0005
    control_dt: float = 0.001                # 控制周期（1 kHz）

    # 传动（机械回填 2026-10-11）
    belt_radius: float = 0.0191              # S3M 40 齿，节圆直径 38.197 mm（机械填的 37.44 mm 是外径）
    yaw_lead: float = 0.004                  # SFUR1204，导程 4 mm，联轴器直连（机械回填）
    pitch_lead: float = 0.002                # BPGF-C7-12-2，导程 2 mm，直连（机械回填）
    yaw_slide_range: tuple = (0.0, 0.16)     # yaw 滑台行程，机械回填 310 mm；和旧 CAD 对不上，等新 CAD 重新导出后再改
    pitch_range: tuple = (-0.15, 0.10)       # pitch 滑块行程，机械回填 180 mm；和旧 CAD 对不上，等新 CAD 重新导出后再改
    slider_top: float = 0.37                 # 3508 滑块上限，机械回填总行程 710 mm；和旧 CAD 对不上，等新 CAD 重新导出后再改
    slider_park: float = 0.36                # 发射时滑块停放位置
    shuttle_top: float = 0.50                # 发射机构上止点，机械回填从锁住点到上止点 600 mm；和旧 CAD 对不上，等新 CAD 重新导出后再改
    lock_open_deg: float = 45.0              # 锁止摇臂解锁角（CAD 上限）
    claw_open_deg: float = 15.0              # 摇臂解锁时夹爪张开角（CAD 上限，按线性联动）
    arm_range_deg: tuple = (-95.0, 95.0)
    # 取镖角（±45.5°）、取镖下探量、装填位 q_load、待命角都由 CAD 几何在生成模型时算出，存在 model/meta.json
    holder_order: tuple = (1, -1)            # 取镖顺序：+1 右侧镖座（+x），-1 左侧
    load_rise_speed: float = 0.05            # 装填时滑块带发射机构“慢慢抬升”的速度 m/s
    yaw_motor: str = "M2006"                 # 实车确认 M2006
    pitch_motor: str = "M3508"               # 实车确认，保留原装 19:1 减速箱

    yaw_max_deg: float = 12.0                # 实车 yaw 极限（机械回填：0~12°，单边）
    # 受力上限（机械回填）。电流上限按这个换算，再乘 load_margin
    belt_force_max: float = 220.0            # 同步带允许最大拉力 N（每根）
    screw_force_max: float = 1000.0          # 丝杆螺母允许最大推力 N（yaw、pitch 都按这个）
    load_margin: float = 0.8

    # 皮筋（CAD 里没有，参数待确认）。实车为乳胶管 6 股，滚轮带轴承，绕法和仿真一样。
    # 一整根，绕法按实物：pitch 滑块一端 → 上方滚轮 → 发射机构 → 另一侧滚轮
    # → pitch 滑块另一端；滚轮按无摩擦处理，整根张力相同，发射机构受两股。pitch 滑块往下 = 拉得更长 = 射得更远
    band_stiffness: float = 90.0             # N/m，整根；拉力曲线待测，刚度仍是估计值
    band_rest_length: float = 0.80           # m，内圈自然长度 0.65 m、外圈 0.80 m；刚度和自然长度仍是估计值，上膛时约 0.60 m 延长
    band_damping: float = 0.2                # N·s/m；拉力曲线待测

    # 飞镖：几何来自 STEP（四发：发射机构上 1、装弹机构磁吸 1、左右镖座各 1）
    darts_enabled: bool = False              # 默认不放飞镖：只看机构动作，发射时记录发射机构峰值速度
    dart_step: str = DART_STEP               # 生成时
    dart_mass: float = None                  # None = 按 STEP 体积估（PLA 1240 kg/m³ + 铁片）；实测后填这里
    dart_cda: float = 6.0e-4                 # 阻力系数 × 迎风面积 m²（镖头 Ø43，估计）
    dart_rail_friction: float = 0.2          # 飞镖与导轨的摩擦系数

    # 舵机 / 摆臂（型号未知，按常见 180° 舵机估）。换镖电机是 GM6020，驱动哪个机构待确认，仿真暂时按舵机限速处理
    servo_speed: float = 7.0                 # rad/s（约 0.15 s/60°）
    arm_speed: float = 3.0                   # rad/s

    mass_override: dict = field(default_factory=dict)  # 刚体名 -> 质量 kg，如 {"shuttle": 0.15}
