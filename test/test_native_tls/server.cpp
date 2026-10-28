#include "PinnedPeerTls.h"
#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
using namespace larp::companion;
// Synthetic test credentials only; this executable is never linked into firmware.
struct Lifetime final : ITlsLifetime {
    TlsOrigin original;
    bool live = true;
    bool current(const TlsOrigin &v) const noexcept override {
        return live && v == original;
    }
};
std::string file(const char *path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("missing fixture");
    return {std::istreambuf_iterator<char>(input), {}};
}
std::string hex(const unsigned char *bytes, std::size_t count) {
    const char *digits = "0123456789abcdef";
    std::string output;
    for (std::size_t i = 0; i < count; ++i) {
        output += digits[bytes[i] >> 4];
        output += digits[bytes[i] & 15];
    }
    return output;
}
int main(int argc, char **argv) {
    if (argc != 4)
        return 2;
    const auto cert = file(argv[1]), key = file(argv[2]), peer = file(argv[3]);
    const auto pem = [](const std::string &v) {
        return TlsPem{reinterpret_cast<const unsigned char *>(v.c_str()), v.size() + 1};
    };
    Lifetime lifetime;
    lifetime.original.registration = 1;
    lifetime.original.epoch = 1;
    PinnedPeerTls tls(lifetime);
    std::uint32_t now = UINT32_MAX - 1000;
    if (!tls.begin(lifetime.original, pem(cert), pem(key), pem(peer), now))
        return 3;
    std::string line;
    while (std::getline(std::cin, line)) {
        std::array<unsigned char, 20> input{}, output{};
        std::array<unsigned char, 128> plain{};
        bool accepted = true;
        if (line == "@REVOKE")
            lifetime.live = false;
        else if (line == "@PRESSURE") {
        } else if (line == "@INPUT_PRESSURE") {
            plain.fill(0xee);
            accepted = tls.feed(lifetime.original, plain.data(), plain.size(), now);
        } else if (line == "@EXPIRE")
            now += PinnedPeerTls::HandshakeMilliseconds;
        else if (line == "@CLOSE")
            tls.close();
        else if (line == "@STALE_BEGIN") {
            auto old = lifetime.original;
            --old.epoch;
            accepted = tls.begin(old, pem(cert), pem(key), pem(peer), now);
        } else if (line == "@REOPEN") {
            tls.close();
            ++lifetime.original.epoch;
            lifetime.live = true;
            accepted = tls.begin(lifetime.original, pem(cert), pem(key), pem(peer), now);
        } else if (line == "@STALE") {
            auto old = lifetime.original;
            --old.epoch;
            input[0] = 0xff;
            accepted = tls.feed(old, input.data(), 1, now);
        } else {
            if (line.size() > 40 || line.size() % 2)
                return 4;
            for (std::size_t i = 0; i < line.size() / 2; ++i)
                input[i] =
                    static_cast<unsigned char>(std::stoul(line.substr(i * 2, 2), nullptr, 16));
            if (!line.empty())
                accepted = tls.feed(lifetime.original, input.data(), line.size() / 2, now);
        }
        const bool pressure = line == "@PRESSURE";
        if (pressure) {
            plain.fill(7);
            accepted = tls.send(lifetime.original, plain.data(), plain.size(), now);
            plain.fill(9); // Caller bytes must not replace an accepted pending write.
        }
        if (line != "@INPUT_PRESSURE")
            tls.service(now);
        const auto got = (pressure || line == "@INPUT_PRESSURE")
                             ? 0
                             : tls.read(lifetime.original, plain.data(), plain.size(), now);
        if (got && !tls.send(lifetime.original, plain.data(), got, now))
            return 5;
        const auto sent =
            pressure ? 0 : tls.drain(lifetime.original, output.data(), output.size(), now);
        std::cout << "{\"out\":\"" << hex(output.data(), sent)
                  << "\",\"ready\":" << (tls.state() == TlsProgress::Ready ? "true" : "false")
                  << ",\"error\":" << tls.error()
                  << ",\"accepted\":" << (accepted ? "true" : "false") << ",\"plain\":\""
                  << hex(plain.data(), got) << "\"}" << std::endl;
    }
}
