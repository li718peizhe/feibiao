"""控制台右侧的状态文字。"""

MOTOR_NAMES = {"motor_r": "右 3508", "motor_l": "左 3508", "yaw": "yaw 2006", "pitch": "pitch 2006"}
DART_STATE = {"holder": "镖座", "held": "磁吸", "seated": "发射位", "flying": "飞行", "free": "掉落", "landed": "已发"}


def render(s):
    L = [f"时间 {s['t']:7.2f} s   实时倍率 {s.get('rtf', 1.0):4.2f}   状态：{s['state']}"]
    if s["blocker"]:
        L.append(f"不能发射：{s['blocker']}")
    L += ["", "电机          指令   电流 A  转矩 N·m   转子 rpm  编码器"]
    for k, mo in s["motors"].items():
        L.append(f"{MOTOR_NAMES[k]:<10s}{mo['cmd']:8d} {mo['amp']:8.2f} {mo['torque']:9.3f} {mo['rpm']:10.0f} {mo['ecd']:7d}")
    L += ["",
          f"3508 滑块      {s['slider_mm']:8.1f} mm",
          f"发射机构       {s['shuttle_mm']:8.1f} mm {s['shuttle_v']:7.2f} m/s  {'已锁止' if s['latched'] else '未锁止'}",
          f"皮筋           {s['band_len_mm']:8.0f} mm  张力 {s['band_n']:6.1f} N（发射机构受两股）",
          f"yaw 偏航角     {s['yaw_deg']:8.3f} °   滑台 {s['yaw_slide_mm']:6.1f} mm",
          f"pitch 滑块     {s['pitch_mm']:8.2f} mm",
          f"锁止摇臂 {s['lock_deg']:5.1f}°  摆臂 {s['arm_deg']:6.1f}°  曲柄 {s['crank_deg']:6.1f}°",
          f"推杆下探 {s['plunger_mm']:5.1f} mm", ""]
    where = " ".join(f"{i + 1}:{DART_STATE[st]}" for i, st in enumerate(s["darts"]))
    if s["darts_enabled"]:
        L += [f"飞镖 {where}  镖座剩 {s['holders']}", "", "射击记录   出膛速度    落点距离   飞行时间"]
        for k, sh in enumerate(s["shots"]):
            dist = "-" if sh["distance"] is None else f"{sh['distance']:.2f} m"
            ft = "-" if sh["flight_time"] is None else f"{sh['flight_time']:.2f} s"
            L.append(f"  第 {k + 1} 发  {sh['v_release']:6.2f} m/s  {dist:>9s}  {ft:>8s}")
    else:
        L += [f"未放飞镖，装填顺序照常记：{where}", "", "发射记录   发射机构峰值速度"]
        L += [f"  第 {k + 1} 次  {sh['v_release']:6.2f} m/s" for k, sh in enumerate(s["shots"])]
    return "\n".join(L)
