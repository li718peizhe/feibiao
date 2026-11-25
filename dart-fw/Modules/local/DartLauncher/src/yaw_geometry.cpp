// 使用 s = R sin ψ；实车几何测完后替换本文件。
#include "yaw_geometry.hpp"
#include "dart_params.hpp"
#include <cmath>

namespace dart {
double YawGeometry::Slide(double angle) {
    if (!std::isfinite(angle)) { return 0; }
    return kYawRadius * std::sin(Clamp(angle, 0, kYawMax));
}
double YawGeometry::Angle(double slide) {
    if (!std::isfinite(slide)) { return 0; }
    return std::asin(Clamp(slide, 0, Slide(kYawMax)) / kYawRadius);
}
}
