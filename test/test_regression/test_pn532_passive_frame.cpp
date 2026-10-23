#include <unity.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "GameRuntime/Pn532PassiveFrame.h"

namespace {
using larp::runtime::PassiveTarget;
using larp::runtime::PassiveTargetOutcome;
using larp::runtime::parsePassiveTarget;

// Synthetic protocol vectors independently assembled from NXP UM0701-02,
// Rev. 02, sections 6.2.1.1 (normal frame) and 7.3.5 (InListPassiveTarget).
// https://www.nxp.com/docs/en/user-guide/141520.pdf
// These are not captured cards or NXP's example UIDs. Fixed positive frames
// include literal LEN/LCS/DCS bytes; the product parser never generates them.
const std::uint8_t FourByteUidFrame[] = {
    0x00, 0x00, 0xff, 0x0c, 0xf4, 0xd5, 0x4b, 0x01, 0x01, 0x00,
    0x04, 0x08, 0x04, 0xde, 0xad, 0xbe, 0xef, 0x96, 0x00,
};
const std::uint8_t SevenByteUidFrame[] = {
    0x00, 0x00, 0xff, 0x0f, 0xf1, 0xd5, 0x4b, 0x01, 0x01, 0x00, 0x44,
    0x00, 0x07, 0x04, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x2a, 0x00,
};
// SEL_RES indicates ISO/IEC 14443-4; ATS = TL, T0, TA1, TB1, TC1, history.
const std::uint8_t SevenByteUidAtsFrame[] = {
    0x00, 0x00, 0xff, 0x15, 0xeb, 0xd5, 0x4b, 0x01, 0x01, 0x00,
    0x44, 0x20, 0x07, 0x04, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
    0x06, 0x75, 0x77, 0x81, 0x02, 0x80, 0x15, 0x00,
};
const std::uint8_t NoTargetFrame[] = {
    0x00, 0x00, 0xff, 0x03, 0xfd, 0xd5, 0x4b, 0x00, 0xe0, 0x00,
};
const std::uint8_t FourByteUid[] = {0xde, 0xad, 0xbe, 0xef};
const std::uint8_t SevenByteUid[] = {0x04, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

typedef std::vector<std::uint8_t> Bytes;

// Test-only normal-frame encoder. It implements framing checksums, not target
// parsing or acceptance rules. Negative payloads retain correct outer framing
// so semantic rejection cannot accidentally be credited to a broken checksum.
Bytes normalFrame(const Bytes &payload)
{
    const auto length = static_cast<std::uint8_t>(payload.size());
    Bytes frame = {0x00, 0x00, 0xff, length, static_cast<std::uint8_t>(0U - length)};
    frame.insert(frame.end(), payload.begin(), payload.end());
    unsigned sum = 0;
    for (const auto value : payload)
        sum += value;
    frame.push_back(static_cast<std::uint8_t>(0U - sum));
    frame.push_back(0x00);
    return frame;
}

Bytes sevenBytePayload()
{
    return Bytes(SevenByteUidFrame + 5, SevenByteUidFrame + sizeof(SevenByteUidFrame) - 2);
}

PassiveTarget previousCard()
{
    PassiveTarget target;
    target.uid.fill(0xa5);
    target.length = 0xff;
    return target;
}

void expectEmpty(PassiveTargetOutcome expected, const std::uint8_t *frame,
                 std::size_t received, const char *message)
{
    PassiveTarget target = previousCard();
    const auto actual = parsePassiveTarget(frame, received, target);
    TEST_ASSERT_EQUAL_INT_MESSAGE(static_cast<int>(expected), static_cast<int>(actual), message);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, target.length, message);
    const std::uint8_t empty[] = {0, 0, 0, 0, 0, 0, 0};
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(empty, target.uid.data(), sizeof(empty), message);
}

void expectEmpty(PassiveTargetOutcome expected, const Bytes &frame, const char *message)
{
    expectEmpty(expected, frame.data(), frame.size(), message);
}

void expectCard(const std::uint8_t *frame, std::size_t received,
                const std::uint8_t *uid, std::size_t uidLength)
{
    PassiveTarget target = previousCard();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(PassiveTargetOutcome::Card),
                          static_cast<int>(parsePassiveTarget(frame, received, target)));
    TEST_ASSERT_EQUAL_UINT8(uidLength, target.length);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(uid, target.uid.data(), uidLength);
    for (std::size_t i = uidLength; i < target.uid.size(); ++i)
        TEST_ASSERT_EQUAL_UINT8(0, target.uid[i]);
}

template <std::size_t N>
void expectEveryTruncatedPrefixInvalid(const std::uint8_t (&frame)[N])
{
    for (std::size_t available = 0; available < N; ++available) {
        const Bytes prefix(frame, frame + available);
        expectEmpty(PassiveTargetOutcome::InvalidFrame, prefix, "truncated frame prefix");
    }
}
} // namespace

void test_pn532_fixed_four_and_seven_byte_uids()
{
    expectCard(FourByteUidFrame, sizeof(FourByteUidFrame), FourByteUid, sizeof(FourByteUid));
    expectCard(SevenByteUidFrame, sizeof(SevenByteUidFrame), SevenByteUid, sizeof(SevenByteUid));
}

void test_pn532_fixed_optional_ats_preserves_uid()
{
    expectCard(SevenByteUidAtsFrame, sizeof(SevenByteUidAtsFrame), SevenByteUid,
               sizeof(SevenByteUid));
}

void test_pn532_no_target_clears_previous_result()
{
    expectEmpty(PassiveTargetOutcome::NoTarget, NoTargetFrame, sizeof(NoTargetFrame), "no target");
}

void test_pn532_null_and_every_truncated_prefix_clear_result()
{
    expectEmpty(PassiveTargetOutcome::InvalidFrame, nullptr, 0, "null empty input");
    expectEmpty(PassiveTargetOutcome::InvalidFrame, nullptr, 64, "null nonempty input");
    expectEveryTruncatedPrefixInvalid(FourByteUidFrame);
    expectEveryTruncatedPrefixInvalid(SevenByteUidFrame);
    expectEveryTruncatedPrefixInvalid(SevenByteUidAtsFrame);
    expectEveryTruncatedPrefixInvalid(NoTargetFrame);
}

void test_pn532_corrupt_framing_and_checksums_clear_result()
{
    // Header, LEN, LCS, DCS and postamble each independently corrupted.
    const std::size_t positions[] = {0, 1, 2, 3, 4, sizeof(SevenByteUidFrame) - 2,
                                     sizeof(SevenByteUidFrame) - 1};
    for (const auto position : positions) {
        Bytes frame(SevenByteUidFrame, SevenByteUidFrame + sizeof(SevenByteUidFrame));
        frame[position] ^= 0x01;
        expectEmpty(PassiveTargetOutcome::InvalidFrame, frame, "corrupt framing/checksum");
    }
    Bytes payloadCorruption(SevenByteUidFrame, SevenByteUidFrame + sizeof(SevenByteUidFrame));
    payloadCorruption[13] ^= 0x80;
    expectEmpty(PassiveTargetOutcome::InvalidFrame, payloadCorruption, "UID corruption without new DCS");
}

void test_pn532_wrong_direction_and_response_code_clear_result()
{
    Bytes payload = sevenBytePayload();
    payload[0] = 0xd4;
    expectEmpty(PassiveTargetOutcome::InvalidFrame, normalFrame(payload), "host-to-chip direction");
    payload = sevenBytePayload();
    payload[1] = 0x4a;
    expectEmpty(PassiveTargetOutcome::InvalidFrame, normalFrame(payload), "request instead of response");
    payload[1] = 0x41;
    expectEmpty(PassiveTargetOutcome::InvalidFrame, normalFrame(payload), "different response command");
}

void test_pn532_short_declared_payloads_clear_result()
{
    const Bytes fullPayload = sevenBytePayload();
    for (std::size_t length = 0; length < 8; ++length) {
        const Bytes incompleteTarget(fullPayload.begin(), fullPayload.begin() + length);
        expectEmpty(PassiveTargetOutcome::InvalidFrame, normalFrame(incompleteTarget),
                     "complete normal frame with incomplete target metadata");
    }
}

void test_pn532_nonminimal_no_target_and_multiple_targets_clear_result()
{
    expectEmpty(PassiveTargetOutcome::InvalidFrame, normalFrame({0xd5, 0x4b, 0x00, 0x00}),
                 "no-target response has hidden tail");
    Bytes payload = sevenBytePayload();
    payload[2] = 0;
    expectEmpty(PassiveTargetOutcome::InvalidFrame, normalFrame(payload), "zero targets with target data");
    // Two complete target records: valid outer frame, unsupported target count.
    payload = sevenBytePayload();
    const Bytes secondTarget(payload.begin() + 3, payload.end());
    payload[2] = 2;
    payload.insert(payload.end(), secondTarget.begin(), secondTarget.end());
    payload[15] = 2;
    expectEmpty(PassiveTargetOutcome::InvalidFrame, normalFrame(payload), "two complete targets");
    payload = sevenBytePayload();
    payload[2] = 0xff;
    expectEmpty(PassiveTargetOutcome::InvalidFrame, normalFrame(payload), "invalid target count");
}

void test_pn532_invalid_target_reference_clears_result()
{
    const std::uint8_t references[] = {0, 2, 0xff};
    for (const auto reference : references) {
        Bytes payload = sevenBytePayload();
        payload[3] = reference;
        expectEmpty(PassiveTargetOutcome::InvalidFrame, normalFrame(payload), "target reference must be one");
    }
}

void test_pn532_unsupported_uid_lengths_clear_result()
{
    const std::uint8_t lengths[] = {0, 3, 8, 10, 49, 52, 0xff};
    for (const auto uidLength : lengths) {
        Bytes payload = {0xd5, 0x4b, 0x01, 0x01, 0x00, 0x44, 0x00, uidLength};
        // Complete UID data for small lengths, including a valid 10-byte UID
        // shape. Overlarge declared UIDs are not copied even in a short frame.
        if (uidLength <= 10)
            for (unsigned i = 0; i < uidLength; ++i)
                payload.push_back(static_cast<std::uint8_t>(0x10 + i));
        expectEmpty(PassiveTargetOutcome::UnsupportedUid, normalFrame(payload), "unsupported UID length");
    }
}

void test_pn532_uid_cannot_extend_beyond_declared_payload()
{
    const std::uint8_t lengths[] = {4, 7};
    for (const auto uidLength : lengths) {
        for (unsigned actual = 0; actual < uidLength; ++actual) {
            Bytes payload = {0xd5, 0x4b, 0x01, 0x01, 0x00, 0x44, 0x00, uidLength};
            payload.insert(payload.end(), actual, 0x42);
            expectEmpty(PassiveTargetOutcome::InvalidFrame, normalFrame(payload),
                         "supported UID length but missing UID bytes");
        }
    }
}

void test_pn532_inconsistent_ats_length_clears_result()
{
    const Bytes validPayload(SevenByteUidAtsFrame + 5,
                             SevenByteUidAtsFrame + sizeof(SevenByteUidAtsFrame) - 2);
    const std::uint8_t wrongLengths[] = {0, 1, 5, 7, 0xff};
    for (const auto atsLength : wrongLengths) {
        Bytes payload = validPayload;
        payload[15] = atsLength;
        expectEmpty(PassiveTargetOutcome::InvalidFrame, normalFrame(payload), "ATS length inconsistent with tail");
    }
    Bytes missingAtsByte = validPayload;
    missingAtsByte.pop_back();
    expectEmpty(PassiveTargetOutcome::InvalidFrame, normalFrame(missingAtsByte), "ATS byte missing");
    Bytes hiddenTail = validPayload;
    hiddenTail.push_back(0x55);
    expectEmpty(PassiveTargetOutcome::InvalidFrame, normalFrame(hiddenTail), "hidden byte after ATS");
}

void test_pn532_declared_length_57_boundary_and_58_rejection()
{
    // Capacity vectors for the project's restricted frame contract. The ATS
    // tail is opaque to this parser; this is not ISO 14443-4 ATS validation or
    // a claim that a physical card emits an ATS of this length.
    Bytes payload = sevenBytePayload();
    payload[6] = 0x20;
    payload.push_back(42);
    payload.insert(payload.end(), 41, 0x55);
    const Bytes maximumFrame = normalFrame(payload);
    TEST_ASSERT_EQUAL_UINT(64, maximumFrame.size());
    TEST_ASSERT_EQUAL_UINT8(57, maximumFrame[3]);
    expectCard(maximumFrame.data(), maximumFrame.size(), SevenByteUid, sizeof(SevenByteUid));
    expectEmpty(PassiveTargetOutcome::InvalidFrame, maximumFrame.data(), 63,
                 "maximum frame missing postamble");
    payload[15] = 43;
    payload.push_back(0x55);
    const Bytes oversizedFrame = normalFrame(payload);
    TEST_ASSERT_EQUAL_UINT8(58, oversizedFrame[3]);
    expectEmpty(PassiveTargetOutcome::InvalidFrame, oversizedFrame, "declared payload exceeds 57");
}

void test_pn532_ack_nack_extended_and_error_frames_clear_result()
{
    // NXP frame kinds other than the supported normal InListPassiveTarget
    // response must not expose a stale card.
    const Bytes ack = {0x00, 0x00, 0xff, 0x00, 0xff, 0x00};
    const Bytes nack = {0x00, 0x00, 0xff, 0xff, 0x00, 0x00};
    const Bytes error = {0x00, 0x00, 0xff, 0x01, 0xff, 0x7f, 0x81, 0x00};
    const Bytes extended = {0x00, 0x00, 0xff, 0xff, 0xff, 0x00, 0x03, 0xfd,
                            0xd5, 0x4b, 0x00, 0xe0, 0x00};
    expectEmpty(PassiveTargetOutcome::InvalidFrame, ack, "ACK is not target data");
    expectEmpty(PassiveTargetOutcome::InvalidFrame, nack, "NACK is not target data");
    expectEmpty(PassiveTargetOutcome::InvalidFrame, error, "error frame is not target data");
    expectEmpty(PassiveTargetOutcome::InvalidFrame, extended, "extended frame not supported");
}

void test_pn532_available_read_padding_does_not_change_declared_frame()
{
    // The SPI caller may have read a full 64-byte buffer. Bytes after the
    // declared DCS/postamble are read padding, not additional UID or ATS data.
    Bytes padded(SevenByteUidFrame, SevenByteUidFrame + sizeof(SevenByteUidFrame));
    padded.resize(64, 0xa5);
    expectCard(padded.data(), padded.size(), SevenByteUid, sizeof(SevenByteUid));
}
