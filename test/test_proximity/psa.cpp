// Actual native PsaCrypto and wifi() key derivation, linked to qualified SDK PSA.
// Only radio/device APIs are replaced; no public test key is used by firmware.
#include "../../firmware/idf-proximity/main/app_main.cpp"
#include "proximity_psa_vectors.h"
#include <cstdio>
#include <stdexcept>
unsigned assertions=0;
void check(bool ok,const char *message){++assertions;if(!ok)throw std::runtime_error(message);}
int main(){
    try{
        Config config;config.peer={{2,1,2,3,4,6}};
        for(unsigned n=0;n<32;++n)config.secret[n]=n;
        PsaCrypto crypto;
        check(crypto.load(config.secret),"actual native PSA import/attributes failed");
        Secret digest{};
        check(crypto.mac(nullptr,0,digest)&&std::equal(digest.begin(),digest.end(),EmptyHmac),"PSA empty HMAC golden");
        check(wifi(config,crypto),"actual native wifi KDF/setup path failed");
        check(fake::pmkCalls==1&&std::equal(fake::pmk.begin(),fake::pmk.end(),Pmk),"actual PMK derivation/truncation");
        check(std::equal(fake::lmk.begin(),fake::lmk.end(),Lmk),"actual LMK derivation/truncation");
        check(fake::pmk!=fake::lmk,"PMK/LMK domains not separated");
        check(fake::peerAddress==config.peer&&fake::peerEncrypted&&fake::peerChannel==1&&fake::peerInterface==WIFI_IF_STA,"encrypted unicast peer binding");
        const Mac a{{2,1,2,3,4,5}},b=config.peer;
        Codec ab(crypto,a,b),ba(crypto,b,a);
        check(ab.initialize()&&ba.initialize(),"actual PSA pair identity");
        Token boot{},nonce{},remoteBoot{};boot.fill(1);nonce.fill(7);remoteBoot.fill(2);
        Packet request{},response{};
        check(ab.encode(false,boot,1,nonce,{},request)&&std::equal(request.begin(),request.end(),Request),"actual PSA signed request golden");
        check(ba.validate(request.data(),request.size(),a,b),"actual PSA valid request rejected");
        check(ba.encode(true,remoteBoot,1,nonce,boot,response)&&std::equal(response.begin(),response.end(),Response),"actual PSA signed response golden");
        check(ab.validate(response.data(),response.size(),b,a),"actual PSA valid response rejected");
        for(unsigned i=0;i<120;++i){auto tampered=response;tampered[i]^=1;check(!ab.validate(tampered.data(),tampered.size(),b,a),"actual PSA tampered response accepted");}
        for(unsigned n=0;n<120;++n)check(!ab.validate(response.data(),n,b,a),"actual PSA short response accepted");
        Config wrong=config;wrong.secret[0]^=1;PsaCrypto wrongKey;check(wrongKey.load(wrong.secret),"other PSA key import");
        Codec foreign(wrongKey,a,b);check(foreign.initialize()&&!foreign.validate(response.data(),response.size(),b,a),"actual PSA wrong pair accepted");
        check(!crypto.failed(),"rejected HMAC incorrectly faulted healthy PSA provider");
        PsaCrypto unloaded;check(!unloaded.mac(nullptr,0,digest)&&unloaded.failed(),"missing PSA key not fail-closed");
        const auto calls=fake::pmkCalls;check(!wifi(config,unloaded)&&fake::pmkCalls==calls,"crypto failure reached ESP-NOW key configuration");
        std::printf("PASS native PSA proximity: %u assertions; actual HMAC key attributes, PMK/LMK and packet vectors from qualified SDK\n",assertions);
        return 0;
    }catch(const std::exception &error){std::fprintf(stderr,"FAIL native PSA proximity: %s\n",error.what());return 1;}
}
