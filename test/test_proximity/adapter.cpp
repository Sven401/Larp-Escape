// Compile and exercise the actual native adapter with deterministic SDK failures.
#include "../../firmware/idf-proximity/main/app_main.cpp"
#include <stdexcept>
#include <cstdio>
unsigned assertions=0;
void check(bool ok,const char *message){++assertions;if(!ok)throw std::runtime_error(message);}
Config config(){Config c;c.peer={{2,1,2,3,4,5}};for(unsigned i=0;i<32;++i)c.secret[i]=i;return c;}
void storageTests(){
    Storage storage;Config c;bool paired=true;
    fake::initFail=true;check(!storage.begin(c,paired),"NVS init failure repaired/ignored");fake::initFail=false;
    fake::openFail=true;check(!storage.begin(c,paired),"NVS open failure ignored");fake::openFail=false;
    check(storage.begin(c,paired)&&!paired,"missing config must be dark/unpaired");
    auto saved=config();fake::setFail=true;check(!storage.save(saved),"NVS set failure accepted");fake::setFail=false;
    fake::commitFail=true;check(!storage.save(saved)&&fake::missing,"NVS commit failure accepted");fake::commitFail=false;
    fake::readFail=true;check(!storage.save(saved),"NVS readback failure accepted");fake::readFail=false;
    fake::readCorrupt=true;check(!storage.save(saved),"NVS readback corruption accepted");fake::readCorrupt=false;
    saved.armed=true;check(storage.save(saved),"persist ARM");
    Storage reboot;check(reboot.begin(c,paired)&&paired&&c.armed,"persisted ARM not recovered");
    saved.armed=false;check(storage.save(saved),"persist STOP");
    Storage stopped;check(stopped.begin(c,paired)&&paired&&!c.armed,"STOP not reboot-locked");
    fake::committed.push_back(0);Storage malformed;check(!malformed.begin(c,paired),"oversized storage blob accepted");
}
unsigned decode(unsigned byte){unsigned out=0;for(unsigned b=0;b<8;++b){const auto&s=fake::symbols.at(byte*8+b);check(s.level0==1&&s.level1==0,"RMT bit polarity");check((s.duration0==24&&s.duration1==26)||(s.duration0==10&&s.duration1==40),"RMT timing");out=(out<<1)|(s.duration0==24);}return out;}
void outputTests(){
    fake::now=0;Output output;check(output.begin(),"RMT begin");auto *original=fake::channels.back();
    check(fake::rmtConfig.gpio_num==21&&fake::rmtConfig.resolution_hz==40000000&&fake::symbols.size()==25,"board channel");
    check(!output.known()&&!output.black()&&!output.idle(),"boot black fabricated before TXdone");
    check(decode(0)==0&&decode(1)==0&&decode(2)==0,"boot pixel not black");
    check(fake::symbols.back().duration0==6000&&fake::symbols.back().duration1==6000,"black latch");
    fake::complete(original);output.service(1);check(output.black()&&output.known()&&output.idle(),"boot confirmation");
    larp::led::Pixel pixel;pixel.red=24;pixel.green=8;pixel.blue=2;
    check(output.submit(pixel,10)&&decode(0)==24&&decode(1)==8&&decode(2)==2,"native RGB order");
    const auto frozen=fake::symbols;check(!output.submit({},11),"inflight buffer reused");
    for(unsigned i=0;i<25;++i)check(fake::retained[i].duration0==frozen[i].duration0,"inflight buffer mutated");
    fake::complete(original);output.service(12);check(!output.black()&&output.confirmed().red==24,"colour confirmation");
    check(output.submit(pixel,20),"timeout setup");output.service(2020);
    check(output.fault()&&!output.known()&&!output.black(),"timeout fabricated black");
    auto *recovery=fake::channels.back();check(recovery!=original&&original->deleted,"recovery channel not separate");
    fake::complete(original);output.service(2021);check(!output.black()&&!output.known(),"old completion confirmed recovery black");
    fake::complete(recovery);output.service(2022);check(output.black()&&output.fault(),"recovery black confirmation/fault latch");
    fake::rmtNewFail=true;Output failed;check(!failed.begin()&&failed.fault()&&!failed.black(),"new channel failure fabricated ready");fake::rmtNewFail=false;
    fake::rmtTransmitFail=true;Output txFailed;check(!txFailed.begin()&&txFailed.fault()&&!txFailed.black(),"TX failure fabricated black");fake::rmtTransmitFail=false;
}
void callbacks(){
    events=xQueueCreate(16,sizeof(Event));dropped.store(false);fake::now=123;
    std::uint8_t src[]={2,1,2,3,4,5},dst[]={2,1,2,3,4,6};wifi_pkt_rx_ctrl_t ctrl;ctrl.rssi=-65;
    esp_now_recv_info_t info;info.src_addr=src;info.des_addr=dst;info.rx_ctrl=&ctrl;Packet packet{};packet[0]=42;
    rx(&info,packet.data(),119);check(events->values.empty(),"short callback frame queued");
    rx(&info,packet.data(),120);src[5]=99;ctrl.rssi=-20;packet[0]=10;Event e;
    check(xQueueReceive(events,&e,0)==pdTRUE&&e.at==123&&e.source[5]==5&&e.packet[0]==42&&e.rssi==-65,"callback borrowed metadata or changed timestamp");
    check(xQueueReceive(events,&e,0)==0,"empty queue not empty");
    for(unsigned n=0;n<17;++n)rx(&info,packet.data(),120);
    check(events->values.size()==16&&dropped.load(),"queue overflow not latched");
    dropped.store(false);tx(nullptr,ESP_NOW_SEND_SUCCESS);check(dropped.load(),"missing TX metadata ignored");
}
void rePairOwner(){
    fake::now=0;fake::serial.clear();fake::input.clear();fake::restarted=false;fake::sends=0;fake::psaMacOk=true;
    fake::initFail=fake::openFail=fake::setFail=fake::commitFail=fake::readFail=fake::readCorrupt=false;
    auto old=config();const auto blob=encodeConfig(old);fake::committed.assign(blob.begin(),blob.end());fake::missing=false;dropped.store(false);
    Mac local{{2,4,6,8,10,12}};PsaCrypto crypto;check(crypto.load(old.secret),"owner fake crypto");Codec remote(crypto,old.peer,local);check(remote.initialize(),"owner remote codec");
    Token boot,nonce;boot.fill(1);nonce.fill(9);Packet request;check(remote.encode(false,boot,1,nonce,{},request),"owner peer request");
    struct Finished{};
    fake::tick=[&](){
        fake::complete(fake::channels.back());
        if(fake::now==20||fake::now==21){Event e;e.source=old.peer;e.destination=local;e.packet=request;e.rssi=-40;e.at=fake::now;check(xQueueSend(events,&e,0)==pdTRUE,"owner request queue");}
        if(fake::now==22)fake::input="PAIR 02:01:02:03:04:05 0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20\n";
        if(fake::now==24){Event e;e.kind=Event::Kind::Tx;e.destination=old.peer;e.ok=true;e.at=fake::now;check(xQueueSend(events,&e,0)==pdTRUE,"owner delayed TX queue");}
        if(fake::now==26)fake::input="INFO\n";
        if(fake::now==28)fake::input="REBOOT\n";
        if(fake::restarted||fake::now>=200)throw Finished{};
    };
    try{app_main();}catch(const Finished&){}
    fake::tick={};
    check(fake::restarted,"re-pair did not allow safe setup reboot");
    check(fake::sends==1,"old-context queued reply sent after PAIR");
    check(fake::serial.find("\"op\":\"PAIR\",\"durable\":true")!=std::string::npos,"PAIR ACK missing");
    check(fake::serial.find("\"restartRequired\":true")!=std::string::npos&&fake::serial.find("\"fault\":0")!=std::string::npos,"re-pair unexpectedly faulted");
    check(fake::serial.find("0102030405060708")==std::string::npos,"PAIR secret echoed");
    fake::psaMacOk=false;
}
int main(){try{storageTests();outputTests();callbacks();rePairOwner();std::printf("PASS native adapters: %u assertions; NVS faults/readback, boot/RGB/TXdone, timeout/late-callback recovery, RXcopy/queueoverflow, re-pair owner context\n",assertions);return 0;}catch(const std::exception&e){std::fprintf(stderr,"FAIL adapter %s\n",e.what());return 1;}}
