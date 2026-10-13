#pragma once
#include "FakeOwners.h"
#include "MemoryJournalStore.h"
#include "FreeRtosGameControlChannel.h"
#include "FreeRtosCommandFrameQueue.h"
#include "CompanionIntegration/GameControl.h"
#include "Companion/StatusFrameSender.h"

// Lab only: synthetic owners, RAM journal and explicitly injected authority.
// This does not authenticate a peer or validate any physical game device.
struct ContractLab {
    Owners owners;
    host_journal::MemoryStore store;
    larp::companion::FreeRtosGameControlChannel channel;
    larp::companion::FreeRtosCommandFrameQueue frames{channel};
    larp::companion::CommandStream stream{channel};
    larp::devices::ProfileApplication application{owners, owners, store};
    larp::integration::GameControl control{application, channel};
    bool begin(larp::companion::BootIdentity boot) {
        return channel.begin() && frames.begin() && bool(application.begin()) && control.begin(boot);
    }
    void tick(std::uint32_t now) {
        larp::companion::dispatchCommandFrame(frames, stream, now);
        owners.pump(now);
        application.service(now);
        control.service(now);
    }
};
