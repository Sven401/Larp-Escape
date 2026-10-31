#pragma once
#include "ProximityBleProtocol.h"
namespace larp { namespace proximity { namespace ble {
enum class RevokeReason:std::uint8_t { None=0,Owner=1,IngressFull=2,EgressFull=3,EgressAge=4,FrameEncode=5,TransferExhausted=6,NotifyBusy=7,NotifyError=8,Unsubscribe=9,HostFault=10,Challenge=11,IngressAge=12,Assembly=13,Proof=14,Command=15,Result=16,ProofTimeout=17,Telemetry=18,AccessCopy=19 };
struct Ingress { enum class Kind:std::uint8_t { Subscribed,Disconnected,Fragment };Kind kind=Kind::Fragment;std::uint32_t epoch=0,at=0;std::uint8_t size=0;std::array<std::uint8_t,244> bytes{}; };
class Transport {
public:
    virtual ~Transport()=default;
    virtual bool begin(const Token&)=0;
    virtual bool receive(Ingress&)=0;
    virtual bool send(std::uint32_t,const Message&)=0;
    virtual bool current(std::uint32_t)const=0;
    virtual void revoke(std::uint32_t)=0;
    virtual void revokeReason(std::uint32_t epoch,RevokeReason){revoke(epoch);}
    virtual bool ready()const=0;
    virtual bool fault()const=0;
};
}}}
