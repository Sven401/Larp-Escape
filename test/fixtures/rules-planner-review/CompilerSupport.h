#pragma once
#if defined(RULES_PLANNER_REVIEW_PSA) && !defined(PACKAGE_MODULE_REVIEW_PSA)
#define PACKAGE_MODULE_REVIEW_PSA
#endif
// Independent audit support: real BCrypt, or production PSA on native CI.
#include "GameConfiguration/PackageModules.h"

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#ifdef PACKAGE_MODULE_REVIEW_PSA
#include "PsaPackageDigest.h"
#else
// Windows types must precede the public BCrypt header.
// clang-format off
#include <windows.h>
#include <bcrypt.h>
// clang-format on
#endif
using namespace larp::configuration;
using namespace larp::configuration::module_ir;
class Bytes : public IPackageByteSource {
  public:
    std::string bytes;
    std::size_t index = 0, failure = static_cast<std::size_t>(-1);
    PackageReadStatus readByte(std::uint8_t &byte) override {
        if (index == failure)
            return PackageReadStatus::Failure;
        if (index == bytes.size())
            return PackageReadStatus::End;
        byte = static_cast<std::uint8_t>(bytes[index++]);
        return PackageReadStatus::Byte;
    }
};
#ifdef PACKAGE_MODULE_REVIEW_PSA
using Hash = PsaPackageDigest;
#else
// Independent real SHA-256 backend using the Windows public crypto API.
class Hash : public IPackageDigest {
    BCRYPT_ALG_HANDLE algorithm_ = nullptr;
    BCRYPT_HASH_HANDLE hash_ = nullptr;

  public:
    ~Hash() {
        abort();
        if (algorithm_)
            BCryptCloseAlgorithmProvider(algorithm_, 0);
    }
    bool begin() override {
        if (!abort())
            return false;
        if (!algorithm_ &&
            BCryptOpenAlgorithmProvider(&algorithm_, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
            return false;
        return BCryptCreateHash(algorithm_, &hash_, nullptr, 0, nullptr, 0, 0) >= 0;
    }
    bool update(const std::uint8_t *bytes, std::size_t count) override {
        return hash_ &&
               BCryptHashData(hash_, const_cast<PUCHAR>(bytes), static_cast<ULONG>(count), 0) >= 0;
    }
    bool finish(std::uint8_t digest[32], std::size_t &written) override {
        written = 0;
        if (!hash_ || BCryptFinishHash(hash_, digest, 32, 0) < 0)
            return false;
        written = 32;
        return abort();
    }
    bool abort() override {
        if (!hash_)
            return true;
        const NTSTATUS r = BCryptDestroyHash(hash_);
        hash_ = nullptr;
        return r >= 0;
    }
};
#endif
// Faults decorate the real backend and are used only for rejection vectors.
class FaultHash : public IPackageDigest {
    Hash real_;
    int failure_, document_, active_ = 0;
    bool selected(int failure) const {
        return failure_ == failure && (document_ == 0 || active_ == document_);
    }

  public:
    FaultHash(int failure, int document) : failure_(failure), document_(document) {}
    bool begin() override {
        ++active_;
        return !selected(1) && real_.begin();
    }
    bool update(const std::uint8_t *b, std::size_t n) override {
        return !selected(2) && real_.update(b, n);
    }
    bool finish(std::uint8_t d[32], std::size_t &n) override {
        if (selected(3))
            return false;
        const bool ok = real_.finish(d, n);
        if (ok && selected(6))
            std::memset(d, 0, 32);
        if (ok && selected(4))
            n = 31;
        return ok;
    }
    bool abort() override {
        const bool ok = real_.abort();
        return ok && !selected(5);
    }
};
const char *errorName(ModuleError error) {
    switch (error) {
#define MODULE_ERROR(name)                                                                         \
    case ModuleError::name:                                                                        \
        return #name
        MODULE_ERROR(None);
        MODULE_ERROR(CanonicalInput);
        MODULE_ERROR(Declarations);
        MODULE_ERROR(FieldSet);
        MODULE_ERROR(Type);
        MODULE_ERROR(Value);
        MODULE_ERROR(Reference);
        MODULE_ERROR(Range);
        MODULE_ERROR(ListOrder);
        MODULE_ERROR(ModuleConflict);
        MODULE_ERROR(MissingModule);
        MODULE_ERROR(CryptoFailure);
        MODULE_ERROR(DigestMismatch);
        MODULE_ERROR(SourceFailure);
        MODULE_ERROR(Cycle);
        MODULE_ERROR(Budget);
        MODULE_ERROR(OperatingProfileLimit);
        MODULE_ERROR(Constraint);
        MODULE_ERROR(Conflict);
        MODULE_ERROR(UnsupportedScope);
        MODULE_ERROR(SinkFailure);
#undef MODULE_ERROR
    }
    return "InvalidError";
}
struct Record {
    std::string id, version, digest;
    Bytes source;
    bool opened = false, closed = false;
};
class Provider : public IModuleSourceProvider {
  public:
    std::vector<Record> records;
    bool open(const ModulePin &pin, IPackageByteSource *&source) override {
        source = nullptr;
        for (auto &r : records)
            if (r.id == pin.id && r.version == pin.version && r.digest == pin.digest) {
                if (r.opened)
                    return false;
                r.opened = true;
                source = &r.source;
                return true;
            }
        return false;
    }
    void close(IPackageByteSource &source) override {
        for (auto &r : records)
            if (&r.source == &source) {
                if (r.closed)
                    throw 1;
                r.closed = true;
            }
    }
};
class Sink : public IExpandedPackageSink {
    int failure_;

  public:
    explicit Sink(int failure = 0) : failure_(failure) {}
    std::vector<std::string> names, documents;
    bool committed = false, aborted = false;
    bool begin() override {
        return failure_ != 1;
    }
    bool beginStation(const char *id) override {
        if (failure_ == 2)
            return false;
        names.push_back(id);
        documents.emplace_back();
        return true;
    }
    bool write(const char *bytes, std::size_t count) override {
        if (failure_ == 3 || documents.empty())
            return false;
        documents.back().append(bytes, count);
        return true;
    }
    bool endStation() override {
        return failure_ != 4;
    }
    bool commit() override {
        if (failure_ == 5)
            return false;
        committed = true;
        return true;
    }
    void abort() override {
        aborted = true;
        names.clear();
        documents.clear();
    }
};
template <class T> void storage(Pool<T> &p, std::vector<T> &v, std::size_t count) {
    v.resize(count);
    p.data = v.data();
    p.capacity = v.size();
    p.count = 0;
}
