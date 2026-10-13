#include "DiagnosticProtocol.h"
#include <cassert>
#include <cstdio>
#ifdef NDEBUG
#error Host diagnostic requires active assertions
#endif
int main() {
    diagnostic::Session s; s.boot = 0x12345678;
    std::uint8_t request[9] = {1,1,1,0,0,0,0,0,0}, reply[16] = {};
    s.receive(request,8,reply);
    assert(reply[1]==1 && diagnostic::get32(reply+2)==1 && diagnostic::get32(reply+6)==s.boot && s.processed==1);
    s.receive(request,8,reply); assert(reply[1]==2 && reply[14]==3 && s.processed==1);
    request[1]=127; request[2]=2; s.receive(request,8,reply);
    assert(reply[1]==2 && reply[14]==1 && s.processed==2 && diagnostic::get32(reply+2)==2);
    for (unsigned n=0;n<=9;++n) if(n!=8) {
        s.receive(request,n,reply); assert(reply[1]==2 && reply[14]==2 && s.processed==2);
    }
    request[2]=3; request[0]=2; s.receive(request,8,reply); assert(reply[14]==2 && s.processed==2);
    request[0]=1; request[6]=1; s.receive(request,8,reply); assert(reply[14]==2);
    request[6]=0; request[2]=0; s.receive(request,8,reply); assert(reply[14]==2);
    s.clearConnection(); request[1]=1; request[2]=1; s.receive(request,8,reply); assert(reply[1]==1 && s.processed==3);
    s.processed=UINT32_MAX; request[2]=2; s.receive(request,8,reply); assert(reply[14]==4 && s.processed==UINT32_MAX);
    std::puts("Diagnostic protocol: correlation, rejection, duplicate, bounds, reconnect, overflow PASS");
}
