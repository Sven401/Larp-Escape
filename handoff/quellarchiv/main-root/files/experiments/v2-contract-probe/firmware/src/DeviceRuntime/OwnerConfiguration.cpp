#include "DeviceRuntime/OwnerConfiguration.h"
#include <initializer_list>
namespace larp {
namespace devices {
bool OwnerConfiguration::begin(const Control &control) {
    if (control.ticket.owner != owner_ || control.ticket.kind != ControlKind::Begin ||
        control.ticket.generation == 0 || control.ticket.generation < generation_ ||
        control.expectedComponents > Limits::Components)
        return false;
    generation_ = control.ticket.generation;
    expected_ = control.expectedComponents;
    i2c_ = control.i2c;
    peerChannel_ = control.peerChannel;
    count_ = 0;
    ready_ = false;
    preparing_ = true;
    return true;
}
const OwnedComponent *OwnerConfiguration::get(std::uint8_t index) const {
    return index < count_ ? &components_[index] : nullptr;
}
const OwnedComponent *OwnerConfiguration::find(ComponentId id) const {
    for (std::uint8_t index = 0; index < count_; ++index)
        if (components_[index].settings.id == id)
            return &components_[index];
    return nullptr;
}
bool OwnerConfiguration::append(const Control &control) {
    const auto &component = control.component;
    if (!preparing_ || control.ticket.owner != owner_ ||
        control.ticket.kind != ControlKind::Component || control.ticket.generation != generation_ ||
        count_ >= expected_ || control.componentSlot >= Limits::Components || !component.enabled ||
        component.id.value == 0 || componentOwner(component.kind) != owner_ || find(component.id))
        return false;
    for (std::uint8_t index = 0; index < count_; ++index)
        if (components_[index].slot == control.componentSlot)
            return false;
    if (component.kind == ComponentKind::Matrix) {
        // Expanders must already be configured in this owner's generation.
        for (const auto *endpoints : {&component.rows, &component.columns})
            for (std::size_t index = 0; index < endpoints->size(); ++index) {
                const auto *expander = find(endpoints->get(index)->expander);
                if (!expander || expander->settings.kind != ComponentKind::IoExpander)
                    return false;
            }
        for (std::size_t index = 0; index < component.wakeInputs.size(); ++index) {
            const auto *expander = find(component.wakeInputs.get(index)->expander);
            if (!expander || expander->settings.kind != ComponentKind::IoExpander)
                return false;
        }
    }
    components_[count_].settings = component;
    components_[count_++].slot = control.componentSlot;
    return true;
}
bool OwnerConfiguration::commit(const Control &control) {
    if (!preparing_ || control.ticket.owner != owner_ ||
        control.ticket.kind != ControlKind::Commit || control.ticket.generation != generation_ ||
        count_ != expected_)
        return false;
    preparing_ = false;
    ready_ = true;
    return true;
}
bool OwnerConfiguration::ready(std::uint32_t generation) const {
    return ready_ && generation == generation_;
}
} // namespace devices
} // namespace larp
