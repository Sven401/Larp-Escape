#include "GameRuntime/Pn532IsoDepAdapter.h"
#include <unity.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

using larp::runtime::IsoDepTarget;
using larp::runtime::Pn532Clock;
using larp::runtime::Pn532IsoDepAdapter;
using larp::runtime::Pn532RawPort;

void setUp() {}
void tearDown() {}

static std::vector<std::uint8_t> frame(std::initializer_list<std::uint8_t> body) {
    std::vector<std::uint8_t> out = {0,0,0xff,
        static_cast<std::uint8_t>(body.size()),
        static_cast<std::uint8_t>(0 - body.size())};
    std::uint8_t sum = 0;
    for (auto byte : body) { out.push_back(byte); sum = static_cast<std::uint8_t>(sum+byte); }
    out.push_back(static_cast<std::uint8_t>(0-sum));
    out.push_back(0);
    return out;
}

static std::vector<std::uint8_t> target() {
    return frame({0xd5,0x4b,1,1,0x04,0,0x20,4,1,2,3,4,2,0});
}
static std::vector<std::uint8_t> response(std::uint8_t code,
                                          const std::array<std::uint8_t,16> &bytes) {
    std::vector<std::uint8_t> body = {0xd5,0x41,0,0x4e,0x43,1};
    body.insert(body.end(),bytes.begin(),bytes.end());
    body.push_back(code); body.push_back(0);
    std::vector<std::uint8_t> out = {0,0,0xff,
        static_cast<std::uint8_t>(body.size()),
        static_cast<std::uint8_t>(0 - body.size())};
    std::uint8_t sum = 0;
    for (auto byte : body) { out.push_back(byte); sum = static_cast<std::uint8_t>(sum+byte); }
    out.push_back(static_cast<std::uint8_t>(0-sum)); out.push_back(0);
    return out;
}

struct FakeClock : Pn532Clock {
    std::uint32_t now = 100;
    std::uint32_t nowMs() const override { return now; }
};

struct FakePort : Pn532RawPort {
    FakeClock &clock;
    std::vector<std::vector<std::uint8_t>> answers;
    std::vector<std::uint8_t> lastCommand;
    std::uint32_t lastTimeout = 0;
    std::uint32_t elapsed = 0;
    unsigned calls = 0;
    bool succeed = true;
    explicit FakePort(FakeClock &value) : clock(value) {}
    bool command(const std::uint8_t *request, std::size_t requestLength,
                 std::uint32_t timeoutMs, std::uint8_t *out,
                 std::size_t capacity, std::size_t &received) override {
        ++calls;
        lastCommand.assign(request,request+requestLength);
        lastTimeout = timeoutMs;
        clock.now += elapsed;
        if (!succeed || answers.empty()) return false;
        const auto answer = answers.front(); answers.erase(answers.begin());
        if (answer.size() > capacity) return false;
        std::memcpy(out,answer.data(),answer.size());
        received = answer.size();
        return true;
    }
};

static void test_select_challenge_are_exact_and_ordered() {
    FakeClock clock;
    FakePort port(clock);
    std::array<std::uint8_t,16> nonce = {};
    for (unsigned i=0;i<nonce.size();++i) nonce[i]=static_cast<std::uint8_t>(i+1);
    port.answers = {target(),response(0x90,nonce),response(0x90,nonce)};
    Pn532IsoDepAdapter adapter(port,clock);
    TEST_ASSERT_FALSE(adapter.begin(0,true,true,30000));
    TEST_ASSERT_FALSE(adapter.begin(3,false,true,30000));
    TEST_ASSERT_TRUE(adapter.begin(3,true,true,30000));
    IsoDepTarget selected;
    TEST_ASSERT_EQUAL_INT(Pn532IsoDepAdapter::Result::Ok,
        adapter.pollTarget(3,true,true,selected));
    TEST_ASSERT_EQUAL_UINT8(4,selected.uidLength);
    TEST_ASSERT_EQUAL_UINT8(0x4a,port.lastCommand[0]);
    TEST_ASSERT_EQUAL_UINT8(1,port.lastCommand[1]);
    TEST_ASSERT_EQUAL_UINT8(0,port.lastCommand[2]);
    std::array<std::uint8_t,21> out = {};
    TEST_ASSERT_EQUAL_INT(Pn532IsoDepAdapter::Result::Ok,
        adapter.selectApplication(3,true,true,out.data(),out.size()));
    const std::vector<std::uint8_t> select = {0x40,1,0x00,0xa4,0x04,0x00,
        0x08,0xf0,0x4c,0x41,0x52,0x50,0x4e,0x46,0x43,0};
    TEST_ASSERT_EQUAL_UINT32(select.size(),port.lastCommand.size());
    TEST_ASSERT_EQUAL_UINT8_ARRAY(select.data(),port.lastCommand.data(),select.size());
    TEST_ASSERT_EQUAL_UINT8(0x90,out[19]);
    TEST_ASSERT_EQUAL_INT(Pn532IsoDepAdapter::Result::Ok,
        adapter.challenge(3,true,true,nonce.data(),nonce.size(),out.data(),out.size()));
    TEST_ASSERT_EQUAL_UINT32(23,port.lastCommand.size());
    TEST_ASSERT_EQUAL_UINT8(0x40,port.lastCommand[0]);
    TEST_ASSERT_EQUAL_UINT8(1,port.lastCommand[1]);
    TEST_ASSERT_EQUAL_UINT8(0x80,port.lastCommand[2]);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(nonce.data(),port.lastCommand.data()+7,nonce.size());
    TEST_ASSERT_EQUAL_INT(Pn532IsoDepAdapter::State::Complete,adapter.state());
    TEST_ASSERT_EQUAL_UINT32(3,port.calls);
}

static void test_target_poll_no_target_and_bad_frame() {
    FakeClock clock;
    FakePort port(clock);
    port.answers = {frame({0xd5,0x4b,0}),target()};
    Pn532IsoDepAdapter adapter(port,clock);
    TEST_ASSERT_TRUE(adapter.begin(5,true,true,2500));
    IsoDepTarget out;
    TEST_ASSERT_EQUAL_INT(Pn532IsoDepAdapter::Result::NoTarget,
        adapter.pollTarget(5,true,true,out));
    TEST_ASSERT_EQUAL_INT(Pn532IsoDepAdapter::State::AwaitTarget,adapter.state());
    TEST_ASSERT_EQUAL_INT(Pn532IsoDepAdapter::Result::Ok,
        adapter.pollTarget(5,true,true,out));
    TEST_ASSERT_EQUAL_INT(Pn532IsoDepAdapter::State::AwaitSelect,adapter.state());
    auto bad=target(); bad[bad.size()-2]^=1;
    adapter.cancel(); port.answers={bad};
    TEST_ASSERT_TRUE(adapter.begin(6,true,true,2500));
    TEST_ASSERT_EQUAL_INT(Pn532IsoDepAdapter::Result::InvalidFrame,
        adapter.pollTarget(6,true,true,out));
    TEST_ASSERT_EQUAL_UINT8(0,out.uidLength);
    TEST_ASSERT_EQUAL_INT(Pn532IsoDepAdapter::State::Failed,adapter.state());
}

static void test_owner_and_game_fence_reject_before_io() {
    FakeClock clock;
    FakePort port(clock);
    Pn532IsoDepAdapter adapter(port,clock);
    TEST_ASSERT_TRUE(adapter.begin(7,true,true,1000));
    IsoDepTarget out;
    TEST_ASSERT_EQUAL_INT(Pn532IsoDepAdapter::Result::Busy,
        adapter.pollTarget(8,true,true,out));
    TEST_ASSERT_EQUAL_UINT32(0,port.calls);
    TEST_ASSERT_EQUAL_INT(Pn532IsoDepAdapter::State::Failed,adapter.state());
    TEST_ASSERT_TRUE(adapter.begin(9,true,true,1000));
    TEST_ASSERT_EQUAL_INT(Pn532IsoDepAdapter::Result::Busy,
        adapter.pollTarget(9,true,false,out));
    TEST_ASSERT_EQUAL_UINT32(0,port.calls);
}

static void test_deadline_checked_before_and_after_io() {
    FakeClock clock;
    FakePort port(clock);
    Pn532IsoDepAdapter adapter(port,clock);
    TEST_ASSERT_TRUE(adapter.begin(10,true,true,1500));
    clock.now=1400;
    port.answers={target()}; port.elapsed=201;
    IsoDepTarget out;
    TEST_ASSERT_EQUAL_INT(Pn532IsoDepAdapter::Result::Timeout,
        adapter.pollTarget(10,true,true,out));
    TEST_ASSERT_EQUAL_UINT32(200,port.lastTimeout);
    TEST_ASSERT_EQUAL_UINT8(0,out.uidLength);
    TEST_ASSERT_TRUE(adapter.begin(11,true,true,1500));
    clock.now+=1500;
    TEST_ASSERT_EQUAL_INT(Pn532IsoDepAdapter::Result::Timeout,
        adapter.pollTarget(11,true,true,out));
    TEST_ASSERT_EQUAL_UINT32(1,port.calls);
}

static void test_truncation_and_failed_exchange_zero_output() {
    FakeClock clock;
    FakePort port(clock);
    auto truncated=response(0x90,{}); truncated.pop_back();
    port.answers={target(),truncated};
    Pn532IsoDepAdapter adapter(port,clock);
    TEST_ASSERT_TRUE(adapter.begin(12,true,true,30000));
    IsoDepTarget selected;
    TEST_ASSERT_EQUAL_INT(Pn532IsoDepAdapter::Result::Ok,
        adapter.pollTarget(12,true,true,selected));
    std::array<std::uint8_t,21> out; out.fill(0xaa);
    TEST_ASSERT_EQUAL_INT(Pn532IsoDepAdapter::Result::InvalidFrame,
        adapter.selectApplication(12,true,true,out.data(),out.size()));
    for (auto byte:out) TEST_ASSERT_EQUAL_UINT8(0,byte);
    TEST_ASSERT_EQUAL_INT(Pn532IsoDepAdapter::State::Failed,adapter.state());
}

static void test_apdu_status_and_echo_fail_closed() {
    FakeClock clock;
    FakePort port(clock);
    std::array<std::uint8_t,16> nonce = {};
    nonce[0]=7;
    port.answers={target(),response(0x6a,nonce)};
    Pn532IsoDepAdapter adapter(port,clock);
    TEST_ASSERT_TRUE(adapter.begin(13,true,true,30000));
    IsoDepTarget selected;
    std::array<std::uint8_t,21> out = {};
    TEST_ASSERT_EQUAL_INT(Pn532IsoDepAdapter::Result::Ok,
        adapter.pollTarget(13,true,true,selected));
    TEST_ASSERT_EQUAL_INT(Pn532IsoDepAdapter::Result::InvalidApdu,
        adapter.selectApplication(13,true,true,out.data(),out.size()));
    TEST_ASSERT_EQUAL_UINT32(2,port.calls);
    for (auto byte:out) TEST_ASSERT_EQUAL_UINT8(0,byte);
    port.answers={target(),response(0x90,nonce),response(0x90,{})};
    TEST_ASSERT_TRUE(adapter.begin(14,true,true,30000));
    TEST_ASSERT_EQUAL_INT(Pn532IsoDepAdapter::Result::Ok,
        adapter.pollTarget(14,true,true,selected));
    TEST_ASSERT_EQUAL_INT(Pn532IsoDepAdapter::Result::Ok,
        adapter.selectApplication(14,true,true,out.data(),out.size()));
    TEST_ASSERT_EQUAL_INT(Pn532IsoDepAdapter::Result::InvalidApdu,
        adapter.challenge(14,true,true,nonce.data(),nonce.size(),out.data(),out.size()));
    for (auto byte:out) TEST_ASSERT_EQUAL_UINT8(0,byte);
    TEST_ASSERT_EQUAL_INT(Pn532IsoDepAdapter::State::Failed,adapter.state());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_select_challenge_are_exact_and_ordered);
    RUN_TEST(test_target_poll_no_target_and_bad_frame);
    RUN_TEST(test_owner_and_game_fence_reject_before_io);
    RUN_TEST(test_deadline_checked_before_and_after_io);
    RUN_TEST(test_truncation_and_failed_exchange_zero_output);
    RUN_TEST(test_apdu_status_and_echo_fail_closed);
    return UNITY_END();
}
