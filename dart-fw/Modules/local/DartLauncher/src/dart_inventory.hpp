// 四发库存推断：发射机构、磁铁和左右镖座各一发。
#pragma once
#include <cstdint>

namespace dart {
enum class DartLocation : std::uint8_t { Empty, Seated, Held, RightHolder, LeftHolder };
// 不依赖吸附或落点传感器，只记录流程推断的转移。
class DartInventory {
public:
    DartInventory();
    // 操作手恢复满装，开机磁铁上的镖来源记为 0。
    void ResetFull();
    // 两发测试：发射座和左镖座各一发。
    void ResetSeatedLeft();
    // 没装换弹机构时用：只记发射机构上那一发，摆臂和两侧镖座都记为空。
    void ResetManual();
    // 独立装填试运行：操作手确认接镖座为空，将取镖试运行举着的一发及其来源转入库存。
    void ResetHeld(int side);
    // 锁止确认或探测判定已锁住时调用：手动放镖时认为操作手会放上一发；自动换弹时不变。
    void Latched();
    // 查询指定位置是否有镖，Empty 表示库存为空。
    bool Has(DartLocation location) const;
    // 从 side（右 +1、左 -1）镖座吸到磁铁，非法转移返回 false。
    bool Grip(int side);
    // 磁铁上的镖转移到发射机构，非法转移返回 false。
    bool Release();
    // 发射机构上的镖出膛，非法转移返回 false。
    bool Fire();
    // 下一侧镖座优先右侧，没有返回 0。
    int NextHolder() const;
    // 剩余库存数量，0~4。
    int Remaining() const;
    // 读取发射机构、磁铁及其来源和非法转移故障。
    bool Seated() const { return seated_; }
    bool Held() const { return held_; }
    int HeldSide() const { return held_side_; }
    bool Fault() const { return fault_; }
private:
    bool seated_ = false, held_ = false, right_ = false, left_ = false, fault_ = false;
    bool manual_ = false;
    int held_side_ = 0;
};
}
