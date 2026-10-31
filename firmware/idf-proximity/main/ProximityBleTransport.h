#pragma once
#include "ProximityBleProtocol.h"
namespace larp { namespace proximity { namespace ble {
struct Ingress { enum class Kind:std::uint8_t { Subscribed,Disconnected,Fragment };Kind kind=Kind::Fragment;std::uint32_t epoch=0,at=0;std::uint8_t size=0;std::array<std::uint8_t,20> bytes{}; };
class Transport {
public:
    virtual ~Transport()=default;
    virtual bool begin(const Token&)=0;
    virtual bool receive(Ingress&)=0;
    virtual bool send(std::uint32_t,const Message&)=0;
    virtual bool current(std::uint32_t)const=0;
    virtual void revoke(std::uint32_t)=0;
    virtual bool ready()const=0;
    virtual bool fault()const=0;
};
}}}
