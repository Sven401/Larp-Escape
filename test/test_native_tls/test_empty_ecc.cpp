#include <cstring>
#include <iostream>
#include <mbedtls/pk.h>
#include <psa/crypto.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

// Public DER SubjectPublicKeyInfo: id-ecPublicKey / prime256v1, followed by
// a BIT STRING with its unused-bit count but no ECPoint payload (CVE-2026-50583).
// Place its final byte against an inaccessible page: even a one-byte overread
// must fail this process instead of being hidden by an adjacent allocation.
int main() {
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
    static const unsigned char emptyPoint[] = {0x30, 0x18, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48,
                                               0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a, 0x86, 0x48,
                                               0xce, 0x3d, 0x03, 0x01, 0x07, 0x03, 0x01, 0x00};
    // Independently fixed SEC1 P-256 generator: prove ECC/SPKI parsing is on.
    static const unsigned char validPoint[] = {
        0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01,
        0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00,
        0x04, 0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47, 0xf8, 0xbc, 0xe6, 0xe5,
        0x63, 0xa4, 0x40, 0xf2, 0x77, 0x03, 0x7d, 0x81, 0x2d, 0xeb, 0x33, 0xa0, 0xf4,
        0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96, 0x4f, 0xe3, 0x42, 0xe2, 0xfe, 0x1a,
        0x7f, 0x9b, 0x8e, 0xe7, 0xeb, 0x4a, 0x7c, 0x0f, 0x9e, 0x16, 0x2b, 0xce, 0x33,
        0x57, 0x6b, 0x31, 0x5e, 0xce, 0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51, 0xf5};
    if (psa_crypto_init() != PSA_SUCCESS)
        return 2;
    mbedtls_pk_context key;
    mbedtls_pk_init(&key);
    const int valid = mbedtls_pk_parse_public_key(&key, validPoint, sizeof(validPoint));
    mbedtls_pk_free(&key);
    if (valid != 0) {
        std::cerr << "valid P-256 SPKI rejected: " << valid << '\n';
        return 3;
    }
#ifdef _WIN32
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    const std::size_t page = system.dwPageSize;
    auto *region = static_cast<unsigned char *>(
        VirtualAlloc(nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!region)
        return 4;
    DWORD previous = 0;
    if (!VirtualProtect(region + page, page, PAGE_NOACCESS, &previous)) {
        VirtualFree(region, 0, MEM_RELEASE);
        return 5;
    }
#else
    const long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0)
        return 4;
    const std::size_t page = static_cast<std::size_t>(pageSize);
    void *mapping =
        mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapping == MAP_FAILED)
        return 4;
    auto *region = static_cast<unsigned char *>(mapping);
    if (mprotect(region + page, page, PROT_NONE) != 0) {
        munmap(region, page * 2);
        return 5;
    }
#endif
    auto *der = region + page - sizeof(emptyPoint);
    std::memcpy(der, emptyPoint, sizeof(emptyPoint));
    mbedtls_pk_init(&key);
    const int result = mbedtls_pk_parse_public_key(&key, der, sizeof(emptyPoint));
    mbedtls_pk_free(&key);
#ifdef _WIN32
    VirtualFree(region, 0, MEM_RELEASE);
#else
    munmap(region, page * 2);
#endif
    if (result == 0) {
        std::cerr << "empty ECPoint accepted\n";
        return 6;
    }
    std::cout << "valid P-256 accepted; empty ECPoint rejected without guard-page overread\n";
    return 0;
}
