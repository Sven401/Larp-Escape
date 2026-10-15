#include "AudioOutput.h"

namespace larp { namespace audio {

CommandFrame AudioOutput::encode(std::uint8_t command, std::uint16_t parameter) {
    CommandFrame frame{{0x7e, 0xff, 0x06, command, 0x00,
        static_cast<std::uint8_t>(parameter >> 8), static_cast<std::uint8_t>(parameter),
        0, 0, 0xef}};
    std::uint16_t sum = 0;
    for (unsigned i = 1; i != 7; ++i) sum += frame[i];
    const auto checksum = static_cast<std::uint16_t>(0U - sum);
    frame[7] = static_cast<std::uint8_t>(checksum >> 8);
    frame[8] = static_cast<std::uint8_t>(checksum);
    return frame;
}

bool AudioOutput::begin() {
    if (initialized_ || !clock_) return false;
    initialized_ = bus_.begin();
    if (!initialized_) { fail(); return false; }
    // Boot never sends a Play. Readiness only follows transmission of Stop.
    status_.pending = true;
    status_.busyValid = true;
    status_.busyLow = bus_.sampleBusyLow();
    return true;
}

bool AudioOutput::start(std::uint16_t track, std::uint8_t volume) {
    if (!initialized_ || !status_.ready || status_.fault || status_.pending ||
        !track || track > MaximumTrack || !volume || volume > MaximumVolume ||
        status_.revision == UINT32_MAX) return false;
    ++status_.revision;
    status_.pending = true;
    status_.completed = status_.timedOut = false;
    wantedPlay_ = true;
    wantedTrack_ = track;
    wantedVolume_ = volume;
    phase_ = Phase::Stop;
    return true;
}

bool AudioOutput::requestStop(bool timedOut) {
    if (!initialized_) return false;
    // Even an exhausted revision must not suppress a physical stop attempt.
    if (status_.revision != UINT32_MAX) ++status_.revision;
    else { status_.fault = true; status_.ready = false; }
    status_.pending = true;
    status_.completed = false;
    status_.timedOut = timedOut;
    wantedPlay_ = false;
    phase_ = Phase::Stop;
    bounded_ = false;
    return true;
}

bool AudioOutput::stop() { return requestStop(false); }

void AudioOutput::fail() {
    status_.fault = true;
    status_.ready = false;
    status_.pending = false;
    // Do not invent a stopped state when UART completion is unknown.
    if (initialized_ && !faultStopRequested_) {
        faultStopRequested_ = true;
        requestStop(false);
    } else if (initialized_ && !recoverySubmitted_) status_.pending = true;
}

void AudioOutput::completeTransfer(std::uint32_t now) {
    inFlight_ = false;
    haveCompletedTransfer_ = true;
    completedAt_ = now;
    if (sentPhase_ == Phase::Stop) {
        status_.active = false;
        status_.track = 0;
        seenBusyLow_ = bounded_ = false;
    } else if (sentPhase_ == Phase::Volume) {
        status_.volume = sentVolume_;
    } else {
        status_.active = true;
        status_.track = sentTrack_;
        status_.completed = false;
        seenBusyLow_ = false;
        bounded_ = true;
        playedAt_ = now;
    }
    // A superseded Play may already be on the wire. Its completion never
    // acknowledges the new Stop; the next bounded command sends that Stop.
    if (sentRevision_ != status_.revision || (!wantedPlay_ && sentPhase_ != Phase::Stop)) return;
    if (wantedPlay_ && sentPhase_ != Phase::Play) {
        phase_ = sentPhase_ == Phase::Stop ? Phase::Volume : Phase::Play;
        return;
    }
    status_.appliedRevision = sentRevision_;
    status_.pending = false;
    status_.ready = !status_.fault;
}

void AudioOutput::service() {
    if (!initialized_) return;
    const auto now = clock_();
    status_.busyLow = bus_.sampleBusyLow();
    status_.busyValid = true;
    if (status_.active && !status_.pending) {
        if (status_.busyLow) seenBusyLow_ = true;
        else if (seenBusyLow_) {
            status_.active = false;
            status_.completed = true;
        }
    }
    // The bound stays armed even if BUSY appears to finish: an unverified wire
    // must not disable the eventual Stop command.
    if (bounded_ && now - playedAt_ >= MaximumPlaybackMilliseconds)
        requestStop(true);
    if (inFlight_) {
        const auto result = bus_.poll();
        if (result == TxResult::Busy) {
            if (now - sentAt_ >= TransferTimeoutMilliseconds) fail();
            return;
        }
        if (result == TxResult::Failed) { inFlight_ = false; fail(); return; }
        completeTransfer(now);
    }
    if (!status_.pending || (haveCompletedTransfer_ &&
        now - completedAt_ < CommandGapMilliseconds)) return;
    const auto command = phase_ == Phase::Stop ? 0x16 : phase_ == Phase::Volume ? 0x06 : 0x12;
    const auto parameter = phase_ == Phase::Volume ? wantedVolume_ :
        phase_ == Phase::Play ? wantedTrack_ : 0;
    if (status_.fault) recoverySubmitted_ = true;
    if (!bus_.submit(encode(static_cast<std::uint8_t>(command), parameter))) {
        fail(); return;
    }
    sentPhase_ = phase_;
    sentRevision_ = status_.revision;
    sentTrack_ = wantedTrack_;
    sentVolume_ = wantedVolume_;
    sentAt_ = now;
    inFlight_ = true;
}

}} // namespace larp::audio
