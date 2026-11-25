// 库存按流程更新，不检查仿真中的磁铁吸附和飞镖落点。
#include "dart_inventory.hpp"
namespace dart {
DartInventory::DartInventory() {
    ResetFull();
}
void DartInventory::ResetFull() {
    seated_ = true;
    held_ = true;
    right_ = true;
    left_ = true;
    held_side_ = 0;
    fault_ = false;
    manual_ = false;
}
void DartInventory::ResetSeatedLeft() {
    ResetFull();
    held_ = false;
    right_ = false;
}
void DartInventory::ResetManual() {
    seated_ = true;
    held_ = false;
    right_ = false;
    left_ = false;
    held_side_ = 0;
    fault_ = false;
    manual_ = true;
}
void DartInventory::ResetHeld(int side) {
    seated_ = false;
    held_ = true;
    right_ = false;
    left_ = false;
    held_side_ = side;
    fault_ = false;
    manual_ = false;
}
void DartInventory::Latched() {
    if (manual_) { seated_ = true; }
}
bool DartInventory::Has(DartLocation location) const {
    switch (location) {
    case DartLocation::Seated: return seated_;
    case DartLocation::Held: return held_;
    case DartLocation::RightHolder: return right_;
    case DartLocation::LeftHolder: return left_;
    case DartLocation::Empty: return Remaining() == 0;
    }
    return false;
}
bool DartInventory::Grip(int side) {
    if (held_ || (side != 1 && side != -1)) {
        fault_ = true;
        return false;
    }
    bool &available = side > 0 ? right_ : left_;
    if (!available) {
        fault_ = true;
        return false;
    }
    available = false;
    held_ = true;
    held_side_ = side;
    return true;
}
bool DartInventory::Release() {
    if (!held_ || seated_) {
        fault_ = true;
        return false;
    }
    held_ = false;
    held_side_ = 0;
    seated_ = true;
    return true;
}
bool DartInventory::Fire() {
    if (!seated_) {
        fault_ = true;
        return false;
    }
    seated_ = false;
    return true;
}
int DartInventory::NextHolder() const { return right_ ? 1 : left_ ? -1 : 0; }
int DartInventory::Remaining() const { return seated_ + held_ + right_ + left_; }
}
