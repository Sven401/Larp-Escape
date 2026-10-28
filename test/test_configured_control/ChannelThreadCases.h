// Compiled only by run_channel_checks.py, not PlatformIO's Unity collector.
#include "FreeRtosGameControlChannel.h"
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>
namespace host_rtos {
thread_local unsigned depth = 0;
thread_local std::function<void(portMUX_TYPE *)> *afterEnter = nullptr, *onContended = nullptr;
std::atomic<unsigned> lockedCalls{0};
bool nonzeroWait = false;
} // namespace host_rtos
using namespace larp::companion;
TrustedAuthority granted(unsigned epoch) {
    TrustedAuthority value;
    value.principal = 7;
    value.epoch = epoch;
    value.gameMaster = true;
    return value;
}
TrustedGameRequest request(unsigned sequence = 1) {
    TrustedGameRequest value;
    value.principal = 7;
    value.connectionEpoch = 1;
    value.command.request = sequence;
    return value;
}
RoutedGameReply reply(unsigned sequence = 1) {
    RoutedGameReply value;
    value.principal = 7;
    value.connectionEpoch = 1;
    value.reply.request = sequence;
    return value;
}
RoutedConfiguredStatus snapshot(unsigned sequence = 1) {
    RoutedConfiguredStatus value;
    value.principal = 7;
    value.connectionEpoch = 1;
    value.status.sample = sequence;
    value.status.roundCount = sequence;
    value.status.devices[0].id = static_cast<std::uint16_t>(sequence);
    return value;
}
// First case is deliberately sequential: the old implementation must fail the
// context rule before we attempt any concurrent use of its simple queue model.
bool contextRule() {
    FreeRtosGameControlChannel channel;
    assert(channel.begin());
    assert(channel.updateAuthority(granted(1)) == AuthorityUpdate::Published);
    assert(channel.submit(request()));
    TrustedGameRequest command;
    assert(channel.receive(command));
    assert(channel.send(reply()));
    RoutedGameReply selected;
    assert(channel.takeCurrentReply(selected));
    assert(channel.publish(snapshot()));
    RoutedConfiguredStatus status;
    assert(channel.readCurrentStatus(status));
    std::printf("RTOS calls under lock: %u; nonzero wait: %s\n", host_rtos::lockedCalls.load(),
                host_rtos::nonzeroWait ? "true" : "false");
    return host_rtos::lockedCalls == 0 && !host_rtos::nonzeroWait;
}
// Force a real failed try_lock against the lock already held by the authority
// writer. Both actions must contend on that same lock, without timed sleeps.
template <typename Select> void revokeBeforeSelection(Select select) {
    FreeRtosGameControlChannel channel;
    assert(channel.begin());
    assert(channel.updateAuthority(granted(1)) == AuthorityUpdate::Published);
    assert(channel.send(reply()));
    assert(channel.publish(snapshot()));
    std::mutex gate;
    std::condition_variable changed;
    bool locked = false, contended = false;
    portMUX_TYPE *authorityLock = nullptr;
    std::thread writer([&] {
        std::function<void(portMUX_TYPE *)> hook = [&](portMUX_TYPE *lock) {
            std::unique_lock<std::mutex> guard(gate);
            authorityLock = lock;
            locked = true;
            changed.notify_all();
            changed.wait(guard, [&] { return contended; });
        };
        host_rtos::afterEnter = &hook;
        channel.suspendAuthority();
    });
    {
        std::unique_lock<std::mutex> guard(gate);
        changed.wait(guard, [&] { return locked; });
    }
    std::thread reader([&] {
        std::function<void(portMUX_TYPE *)> hook = [&](portMUX_TYPE *lock) {
            std::lock_guard<std::mutex> guard(gate);
            assert(lock == authorityLock);
            contended = true;
            changed.notify_all();
        };
        host_rtos::onContended = &hook;
        select(channel);
    });
    writer.join();
    reader.join();
    assert(contended);
}
void sharedAuthorityLock() {
    revokeBeforeSelection([](FreeRtosGameControlChannel &channel) {
        assert(!channel.submit(request()));
        TrustedGameRequest target;
        target.command.request = 999;
        assert(!channel.receive(target));
        assert(target.command.request == 999);
    });
    revokeBeforeSelection([](FreeRtosGameControlChannel &channel) {
        RoutedGameReply target;
        target.reply.request = 999;
        assert(!channel.takeCurrentReply(target));
        assert(target.reply.request == 999);
        assert(!channel.takeReply(target)); // Stale reply was removed.
    });
    revokeBeforeSelection([](FreeRtosGameControlChannel &channel) {
        RoutedConfiguredStatus target;
        target.status.sample = 999;
        assert(!channel.readCurrentStatus(target));
        assert(target.status.sample == 999);
        assert(channel.readStatus(target.status)); // Raw diagnostics stay available.
        assert(target.status.sample == 1);
    });
}
void concurrentCopies() {
    FreeRtosGameControlChannel channel;
    assert(channel.begin());
    assert(channel.updateAuthority(granted(1)) == AuthorityUpdate::Published);
    constexpr unsigned count = 20000;
    std::atomic<bool> published{false};
    std::atomic<unsigned> observed{0};
    std::thread producer([&] {
        for (unsigned i = 1; i <= count; ++i) {
            while (!channel.submit(request(i)))
                std::this_thread::yield();
            while (!channel.send(reply(i)))
                std::this_thread::yield();
            assert(channel.publish(snapshot(i)));
            if (i % 1000 == 0) {
                while (observed < i)
                    std::this_thread::yield();
            }
        }
        published = true;
    });
    std::thread consumer([&] {
        for (unsigned i = 1; i <= count; ++i) {
            TrustedGameRequest command;
            while (!channel.receive(command))
                std::this_thread::yield();
            assert(command.command.request == i);
            RoutedGameReply answer;
            while (!channel.takeCurrentReply(answer))
                std::this_thread::yield();
            assert(answer.reply.request == i);
        }
    });
    std::thread observer([&] {
        do {
            RoutedConfiguredStatus status;
            if (channel.readCurrentStatus(status)) {
                assert(status.status.sample == status.status.roundCount);
                assert(status.status.devices[0].id == status.status.sample);
                assert(status.principal == 7 && status.connectionEpoch == 1);
                observed = status.status.sample;
            }
        } while (!published);
    });
    producer.join();
    consumer.join();
    observer.join();
    RoutedConfiguredStatus last;
    assert(channel.readCurrentStatus(last));
    assert(last.status.sample == count);
}
int main() {
    if (!contextRule())
        return 1;
    sharedAuthorityLock();
    concurrentCopies();
    assert(host_rtos::lockedCalls == 0 && !host_rtos::nonzeroWait);
    std::printf("PASS: 3 forced authority-lock contentions; 20000 FIFO commands/replies; "
                "concurrent coherent status copies. Host mutex only, no ESP32 timing proof.\n");
}
