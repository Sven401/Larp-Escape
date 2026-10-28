#include "FreeRtosGameControlChannel.h"
#include <cassert>
#include <cstdio>

namespace host_rtos {
void (*beforeEnter)() = nullptr;
unsigned depth = 0;
bool nonzeroWait = false;
unsigned lockedSend = 0, lockedReceive = 0, lockedPeek = 0;
unsigned totalSend = 0, totalReceive = 0, totalPeek = 0;
}

int main() {
    using namespace larp::companion;
    FreeRtosGameControlChannel channel;
    assert(channel.begin());
    TrustedAuthority authority;
    authority.principal = 7; authority.epoch = 1; authority.gameMaster = true;
    assert(channel.updateAuthority(authority) == AuthorityUpdate::Published);
    TrustedGameRequest request;
    request.principal = 7; request.connectionEpoch = 1;
    assert(channel.submit(request)); // Calls the actual private admit().
    TrustedGameRequest received;
    assert(channel.receive(received)); // Outside authority lock, control case.
    RoutedGameReply reply;
    reply.principal = 7; reply.connectionEpoch = 1;
    assert(channel.send(reply)); // Outside authority lock, control case.
    RoutedGameReply selected;
    assert(channel.takeCurrentReply(selected));
    RoutedConfiguredStatus status;
    status.principal = 7; status.connectionEpoch = 1;
    assert(channel.publish(status));
    RoutedConfiguredStatus copied;
    assert(channel.readCurrentStatus(copied));
    assert(host_rtos::depth == 0);
    assert(!host_rtos::nonzeroWait);
    assert(host_rtos::lockedSend == 1);
    assert(host_rtos::lockedReceive == 1);
    assert(host_rtos::lockedPeek == 1);
    assert(host_rtos::totalSend == 2);
    assert(host_rtos::totalReceive == 2);
    assert(host_rtos::totalPeek == 1);
    std::printf("{\"locked_send\":%u,\"locked_receive\":%u,\"locked_peek\":%u,"
                "\"total_send\":%u,\"total_receive\":%u,\"total_peek\":%u,"
                "\"nonzero_wait\":false,\"final_lock_depth\":%u}\n",
                host_rtos::lockedSend, host_rtos::lockedReceive, host_rtos::lockedPeek,
                host_rtos::totalSend, host_rtos::totalReceive, host_rtos::totalPeek,
                host_rtos::depth);
}
