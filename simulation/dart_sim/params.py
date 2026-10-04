"""飞镖发射架仿真参数（新机械，来自 URDF 导出）。

CAD 里没有的量（皮筋、飞镖质量、丝杆导程、带轮半径、电机转子惯量）都是估计值，
注释里标了“待确认”。单位：m、kg、s、rad；名字带 _deg 的是角度。
大部分参数在加载模型时写入，改完不用重新生成模型；标了“生成时”的需要重新 build。
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

    # 传动（待确认）
    belt_radius: float = 0.0191              # 3508 同步带轮节圆半径（按 60T GT2 估）
    yaw_lead: float = 0.004                  # yaw 丝杆导程（按 SFU1204 估）
    pitch_lead: float = 0.004                # pitch 丝杆导程
    yaw_slide_range: tuple = (0.0, 0.16)     # yaw 滑台行程，CAD 导出给的是单边行程（待确认）
    pitch_range: tuple = (-0.15, 0.10)       # pitch 滑块行程（CAD）
    slider_top: float = 0.37                 # 3508 滑块上限（CAD）
    slider_park: float = 0.36                # 发射时滑块停放位置
    shuttle_top: float = 0.50                # 发射机构上止点（CAD）
    lock_open_deg: float = 45.0              # 锁止摇臂解锁角（CAD 上限）
    claw_open_deg: float = 15.0              # 摇臂解锁时夹爪张开角（CAD 上限，按线性联动）
    arm_range_deg: tuple = (-95.0, 95.0)
    # 取镖角（±45.5°）、取镖下探量、装填位 q_load、待命角都由 CAD 几何在生成模型时算出，存在 model/meta.json
    holder_order: tuple = (1, -1)            # 取镖顺序：+1 右侧镖座（+x），-1 左侧
    load_rise_speed: float = 0.05            # 装填时滑块带发射机构“慢慢抬升”的速度 m/s
    yaw_motor: str = "M2006"
    pitch_motor: str = "M2006"               # CAD 名叫 pitch2006，但电机外形是 Ø42（同 3508），待确认

    # 皮筋（CAD 里没有，参数待确认）。一整根，绕法按实物：pitch 滑块一端 → 上方滚轮 → 发射机构 → 另一侧滚轮
    # → pitch 滑块另一端；滚轮按无摩擦处理，整根张力相同，发射机构受两股。pitch 滑块往下 = 拉得更长 = 射得更远
    band_stiffness: float = 90.0             # N/m，整根
    band_rest_length: float = 0.80           # m，整根自然长度（上膛时约 1.65 m）
    band_damping: float = 0.2                # N·s/m

    # 飞镖：几何来自 STEP（四发：发射机构上 1、装弹机构磁吸 1、左右镖座各 1）
    darts_enabled: bool = False              # 默认不放飞镖：只看机构动作，发射时记录发射机构峰值速度
    dart_step: str = DART_STEP               # 生成时
    dart_mass: float = None                  # None = 按 STEP 体积估（PLA 1240 kg/m³ + 铁片）；实测后填这里
    dart_cda: float = 6.0e-4                 # 阻力系数 × 迎风面积 m²（镖头 Ø43，估计）
    dart_rail_friction: float = 0.2          # 飞镖与导轨的摩擦系数

    # 舵机 / 摆臂（型号未知，按常见 180° 舵机估）
    servo_speed: float = 7.0                 # rad/s（约 0.15 s/60°）
    arm_speed: float = 3.0                   # rad/s

    mass_override: dict = field(default_factory=dict)  # 刚体名 -> 质量 kg，如 {"shuttle": 0.15}
