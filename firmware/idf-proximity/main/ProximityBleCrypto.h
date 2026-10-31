#pragma once
#include "ProximityBleProtocol.h"
namespace larp { namespace proximity { namespace ble {
class NativeCrypto final:public Crypto {
public:
    bool hmac(const Secret&,const std::uint8_t*,std::size_t,Secret&)override;
    bool hash(const std::uint8_t*,std::size_t,Secret&)override;
};
}}}
