#include "DeviceRuntime/Plan.h"
namespace larp {
namespace devices {
Owner componentOwner(ComponentKind kind) {
    switch (kind) {
    case ComponentKind::NfcSpi:
        return Owner::Nfc;
    case ComponentKind::IoExpander:
    case ComponentKind::Matrix:
        return Owner::I2c;
    case ComponentKind::AudioUart:
        return Owner::Audio;
    case ComponentKind::LedStrip:
        return Owner::Led;
    case ComponentKind::EspNow:
        return Owner::Peer;
    }
    return Owner::Count;
}
namespace {
std::uint8_t slot(const Profile &profile, ComponentId id) {
    for (std::size_t index = 0; index < profile.components.size(); ++index)
        if (profile.components.get(index)->id == id)
            return static_cast<std::uint8_t>(index);
    return 255;
}
} // namespace
const char *ownerName(Owner owner) {
    static const char *const names[] = {"nfc", "i2c", "audio", "led", "peer"};
    const unsigned index = static_cast<unsigned>(owner);
    return index < ChannelLimits::Owners ? names[index] : "unknown_owner";
}
const char *planErrorName(PlanError error) {
    static const char *const names[] = {"none", "invalid_profile", "radio_channel_conflict"};
    const unsigned index = static_cast<unsigned>(error);
    return index < sizeof(names) / sizeof(names[0]) ? names[index] : "unknown_plan_error";
}
PlanResult compilePlan(const Profile &profile, Plan &plan) {
    PlanResult result;
    result.validation = configuration::validateProfile(profile);
    if (!result.validation) {
        result.error = PlanError::InvalidProfile;
        return result;
    }
    // ESP-NOW peers share one radio; contradictory fixed channels cannot run
    // concurrently. Automatic channel 0 follows the one selected fixed channel.
    std::uint8_t channel = 0;
    ComponentId channelOwner;
    for (std::size_t index = 0; index < profile.components.size(); ++index) {
        const configuration::Component &component = *profile.components.get(index);
        if (!component.enabled || component.kind != ComponentKind::EspNow || component.channel == 0)
            continue;
        if (channel != 0 && channel != component.channel) {
            result.error = PlanError::RadioChannelConflict;
            result.component = component.id;
            result.conflictingComponent = channelOwner;
            return result;
        }
        channel = component.channel;
        channelOwner = component.id;
    }
    plan.profileRevision = profile.revision;
    plan.routeCount = static_cast<std::uint8_t>(profile.components.size());
    plan.enabledCount = 0;
    plan.ownerComponents.fill(0);
    plan.i2c.fill(I2cBus());
    plan.totalLeds = 0;
    plan.peerChannel = channel;
    // I/O timeout plus worst-case FIFO waiting behind one full owner queue.
    // Driver jobs still need their own bounded service/cancellation contract.
    plan.replyTimeoutMilliseconds =
        static_cast<std::uint32_t>(profile.inputs.cardTimeoutMilliseconds) *
            (ChannelLimits::CommandsPerOwner + 1U) +
        100U;
    for (std::size_t index = 0; index < profile.components.size(); ++index) {
        const configuration::Component &component = *profile.components.get(index);
        Route &route = plan.routes[index];
        route = Route();
        route.id = component.id;
        route.kind = component.kind;
        route.owner = componentOwner(component.kind);
        route.enabled = component.enabled;
        route.profileSlot = static_cast<std::uint8_t>(index);
        route.rowExpanders.fill(255);
        route.columnExpanders.fill(255);
        route.wakeExpanders.fill(255);
        if (!component.enabled)
            continue;
        ++plan.ownerComponents[static_cast<unsigned>(route.owner)];
        if (component.kind == ComponentKind::IoExpander) {
            I2cBus &bus = plan.i2c[component.bus];
            bus.used = true;
            bus.sda = component.pins[0];
            bus.scl = component.pins[1];
        } else if (component.kind == ComponentKind::LedStrip) {
            plan.totalLeds = static_cast<std::uint16_t>(plan.totalLeds + component.ledCount);
        } else if (component.kind == ComponentKind::Matrix) {
            for (std::size_t position = 0; position < component.rows.size(); ++position)
                route.rowExpanders[position] =
                    slot(profile, component.rows.get(position)->expander);
            for (std::size_t position = 0; position < component.columns.size(); ++position)
                route.columnExpanders[position] =
                    slot(profile, component.columns.get(position)->expander);
            for (std::size_t position = 0; position < component.wakeInputs.size(); ++position)
                route.wakeExpanders[position] =
                    slot(profile, component.wakeInputs.get(position)->expander);
        }
    }
    // Expanders before dependent matrices, independently of JSON table order.
    // Bus setup belongs to the I2C owner before these component steps.
    for (unsigned phase = 0; phase < 3; ++phase)
        for (std::uint8_t index = 0; index < plan.routeCount; ++index) {
            const Route &route = plan.routes[index];
            const unsigned rank = route.kind == ComponentKind::IoExpander ? 0
                                  : route.kind == ComponentKind::Matrix   ? 2
                                                                          : 1;
            if (route.enabled && rank == phase)
                plan.initializationOrder[plan.enabledCount++] = index;
        }
    return result;
}
} // namespace devices
} // namespace larp
