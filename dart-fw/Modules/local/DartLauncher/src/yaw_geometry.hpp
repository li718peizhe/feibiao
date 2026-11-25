// 待测量的 yaw 销轴几何，正角对应滑台从中线向外移动。
#pragma once

namespace dart {
// 使用旧 CAD 半径，角度限制为 0~12°。
class YawGeometry {
public:
    // rad 转 m，超出角度限制时夹紧。
    static double Slide(double angle);
    // m 转 rad，位移夹紧到几何允许的范围。
    static double Angle(double slide);
};
}
