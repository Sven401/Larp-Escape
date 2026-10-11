#include "GameRuntime/Pn532IsoDepFrame.h"
#include <unity.h>
#include <array>
#include <cstdint>
#include <vector>

using larp::runtime::IsoDepFrameResult;
using larp::runtime::IsoDepFrameSession;
using larp::runtime::IsoDepTarget;
using larp::runtime::parseIsoDepExchange;
using larp::runtime::parseIsoDepTarget;

void setUp() {}
void tearDown() {}

static std::vector<std::uint8_t> frame(std::initializer_list<std::uint8_t> data) {
    std::vector<std::uint8_t> result = {0, 0, 0xff,
        static_cast<std::uint8_t>(data.size()),
        static_cast<std::uint8_t>(0 - data.size())};
    std::uint8_t sum = 0;
    for (auto byte : data) { result.push_back(byte); sum = static_cast<std::uint8_t>(sum + byte); }
    result.push_back(static_cast<std::uint8_t>(0 - sum));
    result.push_back(0);
    return result;
}

static std::vector<std::uint8_t> target() {
    // NXP InListPassiveTarget: D5 4B NbTg Tg SENS_RES SEL_RES NFCIDLength
    // NFCID1 ATS (TL T0). SEL_RES bit 5 advertises ISO-DEP.
    return frame({0xd5,0x4b,1,1,0x04,0x00,0x20,4,0x11,0x22,0x33,0x44,2,0x00});
}

static std::vector<std::uint8_t> exchange(std::size_t apduLength) {
    std::vector<std::uint8_t> data = {0xd5,0x41,0};
    for (std::size_t i = 0; i < apduLength; ++i)
        data.push_back(static_cast<std::uint8_t>(i));
    auto result = frame({});
    result = {0,0,0xff,static_cast<std::uint8_t>(data.size()),
              static_cast<std::uint8_t>(0 - data.size())};
    std::uint8_t sum = 0;
    for (auto byte : data) { result.push_back(byte); sum = static_cast<std::uint8_t>(sum + byte); }
    result.push_back(static_cast<std::uint8_t>(0 - sum));
    result.push_back(0);
    return result;
}

static void test_target_normal_frame_and_ats() {
    IsoDepTarget parsed;
    auto valid = target();
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::Ok, parseIsoDepTarget(valid.data(), valid.size(), parsed));
    TEST_ASSERT_EQUAL_UINT8(4, parsed.uidLength);
    TEST_ASSERT_EQUAL_UINT8(0x44, parsed.uid[3]);
    TEST_ASSERT_EQUAL_UINT8(2, parsed.atsLength);
    TEST_ASSERT_EQUAL_UINT8(2, parsed.ats[0]);
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::NoTarget,
                          parseIsoDepTarget(frame({0xd5,0x4b,0}).data(), 10, parsed));
    TEST_ASSERT_EQUAL_UINT8(0, parsed.uidLength);
    auto seven = frame({0xd5,0x4b,1,1,0,0,0x20,7,1,2,3,4,5,6,7,2,0});
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::Ok,
                          parseIsoDepTarget(seven.data(), seven.size(), parsed));
    TEST_ASSERT_EQUAL_UINT8(7, parsed.uidLength);
    auto ten = frame({0xd5,0x4b,1,1,0,0,0x20,10,1,2,3,4,5,6,7,8,9,10,2,0});
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::Ok,
                          parseIsoDepTarget(ten.data(), ten.size(), parsed));
    TEST_ASSERT_EQUAL_UINT8(10, parsed.uidLength);
}

static void test_target_rejects_malformed_and_unsupported() {
    IsoDepTarget parsed;
    auto good = target();
    for (std::size_t index : {std::size_t(0),std::size_t(2),std::size_t(4),
                              std::size_t(5),std::size_t(6),good.size()-2,good.size()-1}) {
        auto bad = good; bad[index] ^= 1;
        TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::InvalidFrame,
                              parseIsoDepTarget(bad.data(), bad.size(), parsed));
        TEST_ASSERT_EQUAL_UINT8(0, parsed.uidLength);
    }
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::InvalidFrame,
                          parseIsoDepTarget(good.data(), good.size()-1, parsed));
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::InvalidFrame,
                          parseIsoDepTarget(good.data(), 8, parsed));
    good.push_back(0);
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::InvalidFrame,
                          parseIsoDepTarget(good.data(), good.size(), parsed));
    auto noAts = frame({0xd5,0x4b,1,1,0,0,0x20,4,1,2,3,4});
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::InvalidFrame,
                          parseIsoDepTarget(noAts.data(), noAts.size(), parsed));
    auto truncatedAts = frame({0xd5,0x4b,1,1,0,0,0x20,4,1,2,3,4,3,0});
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::InvalidFrame,
                          parseIsoDepTarget(truncatedAts.data(), truncatedAts.size(), parsed));
    auto noIsoDep = frame({0xd5,0x4b,1,1,0,0,0x00,4,1,2,3,4,2,0});
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::UnsupportedTarget,
                          parseIsoDepTarget(noIsoDep.data(), noIsoDep.size(), parsed));
    auto twoTargets = frame({0xd5,0x4b,2,1,0,0,0x20,4,1,2,3,4,2,0});
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::InvalidFrame,
                          parseIsoDepTarget(twoTargets.data(), twoTargets.size(), parsed));
    auto wrongTg = frame({0xd5,0x4b,1,2,0,0,0x20,4,1,2,3,4,2,0});
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::InvalidFrame,
                          parseIsoDepTarget(wrongTg.data(), wrongTg.size(), parsed));
    auto huge = good; huge.resize(65, 0);
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::InvalidFrame,
                          parseIsoDepTarget(huge.data(), huge.size(), parsed));
}

static void test_exchange_exact_status_length_capacity() {
    std::array<std::uint8_t,21> out = {};
    auto longReply = exchange(21);
    auto shortReply = exchange(2);
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::Ok,
                          parseIsoDepExchange(longReply.data(), longReply.size(),21,out.data(),out.size()));
    TEST_ASSERT_EQUAL_UINT8(20, out[20]);
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::Ok,
                          parseIsoDepExchange(shortReply.data(), shortReply.size(),2,out.data(),out.size()));
    TEST_ASSERT_EQUAL_UINT8(0, out[20]);
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::WrongLength,
                          parseIsoDepExchange(shortReply.data(),shortReply.size(),21,out.data(),out.size()));
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::InsufficientCapacity,
                          parseIsoDepExchange(longReply.data(),longReply.size(),21,out.data(),20));
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::InsufficientCapacity,
                          parseIsoDepExchange(longReply.data(),longReply.size(),21,nullptr,21));
    auto status = frame({0xd5,0x41,1,0x90,0});
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::DeviceStatus,
                          parseIsoDepExchange(status.data(),status.size(),2,out.data(),out.size()));
    auto chaining = frame({0xd5,0x41,0x40,0x90,0});
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::DeviceStatus,
                          parseIsoDepExchange(chaining.data(),chaining.size(),2,out.data(),out.size()));
    auto corrupt = longReply; corrupt[corrupt.size()-2] ^= 1;
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::InvalidFrame,
                          parseIsoDepExchange(corrupt.data(),corrupt.size(),21,out.data(),out.size()));
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::InvalidFrame,
                          parseIsoDepExchange(longReply.data(),longReply.size()-1,21,out.data(),out.size()));
    longReply.push_back(0);
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::InvalidFrame,
                          parseIsoDepExchange(longReply.data(),longReply.size(),21,out.data(),out.size()));
}

static void test_session_rejects_stale_late_and_partial_frames() {
    IsoDepFrameSession session;
    IsoDepTarget read;
    auto valid = target();
    auto reply = exchange(21);
    std::array<std::uint8_t,21> out = {};
    TEST_ASSERT_FALSE(session.begin(0,100,1000));
    TEST_ASSERT_TRUE(session.begin(17,100,1000));
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::Ok,
                          session.onTarget(17,101,valid.data(),valid.size()));
    TEST_ASSERT_TRUE(session.target(read));
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::InvalidFrame,
                          session.onExchange(18,102,reply.data(),reply.size(),21,out.data(),out.size()));
    TEST_ASSERT_FALSE(session.target(read));
    TEST_ASSERT_EQUAL_UINT8(0, read.uidLength);
    TEST_ASSERT_EQUAL_INT(IsoDepFrameSession::Stage::Failed, session.stage());
    TEST_ASSERT_TRUE(session.begin(19,100,1000));
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::InvalidFrame,
                          session.onTarget(19,101,valid.data(),valid.size()-1));
    TEST_ASSERT_FALSE(session.target(read));
    TEST_ASSERT_TRUE(session.begin(20,100,1000));
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::Ok,
                          session.onTarget(20,101,valid.data(),valid.size()));
    session.tick(1101);
    TEST_ASSERT_FALSE(session.target(read));
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::InvalidFrame,
                          session.onExchange(20,1102,reply.data(),reply.size(),21,out.data(),out.size()));
    TEST_ASSERT_TRUE(session.begin(21,100,1000));
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::Ok,
                          session.onTarget(21,101,valid.data(),valid.size()));
    TEST_ASSERT_EQUAL_INT(IsoDepFrameResult::Ok,
                          session.onExchange(21,102,reply.data(),reply.size(),21,out.data(),out.size()));
    TEST_ASSERT_EQUAL_INT(IsoDepFrameSession::Stage::Complete, session.stage());
    TEST_ASSERT_FALSE(session.target(read));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_target_normal_frame_and_ats);
    RUN_TEST(test_target_rejects_malformed_and_unsupported);
    RUN_TEST(test_exchange_exact_status_length_capacity);
    RUN_TEST(test_session_rejects_stale_late_and_partial_frames);
    return UNITY_END();
}
