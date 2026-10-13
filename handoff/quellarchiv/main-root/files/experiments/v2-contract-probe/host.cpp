#include "ContractLab.h"
#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <vector>
#include <string>
namespace host_rtos { void (*beforeEnter)() = nullptr; unsigned depth = 0; bool nonzeroWait = false; }
using namespace larp::companion;
static ContractLab lab;
static std::uint32_t now = 0;
static std::uint16_t message = 0;
void require(bool ok, const char *why) { if (!ok) { std::fprintf(stderr,"FAIL: %s\n",why); std::exit(1); } }
struct Capture : IReplyFrameBearer {
    std::vector<RoutedReplyFrame> frames;
    ReplyFrameCopy copyReplyFrame(const RoutedReplyFrame &frame) noexcept override {
        require(frame.count>=9 && frame.count<=20,"outgoing frame size");frames.push_back(frame);return ReplyFrameCopy::Copied;
    }
    std::string document(bool status) {
        std::string result;std::uint16_t message=0,total=0;
        for(const auto &r:frames) {
            CommandFrame f;require((status?decodeStatusFrame(r.bytes.data(),r.count,f):decodeReplyFrame(r.bytes.data(),r.count,f))==FrameDecodeResult::Success,"outgoing decoder");
            if(!message) { message=f.message;total=f.total; }
            require(f.message==message && f.total==total && f.offset==result.size(),"outgoing sequence");result.append(f.payload,f.count);
        }
        require(result.size()==total,"complete outgoing document");return result;
    }
};
GameReply command(GameCommand c) {
    char json[512]; std::size_t count = 0;
    require(encodeGameCommand(c,json,sizeof json,count)==ConfiguredEncodeResult::Success,"encode command");
    TrustedAuthority a; require(lab.channel.readAuthority(a),"authority");
    ++message;
    for (std::size_t offset=0;offset<count;) {
        char bytes[20]; std::size_t written=0, part=std::min(std::size_t(12),count-offset);
        require(encodeCommandFrame(message,count,offset,json+offset,part,bytes,sizeof bytes,written),"frame");
        require(lab.frames.copyWrite(bytes,written,a.principal,a.epoch,now)==FrameCopyOutcome::Copied,"queue frame");
        lab.tick(now++); offset+=part;
    }
    RoutedGameReply routed; require(lab.channel.takeCurrentReply(routed),"reply");
    char reply[512]; require(encodeGameReply(routed.reply,reply,sizeof reply,count)==ConfiguredEncodeResult::Success,"reply json");
    Capture bearer;ReplyFrameSender sender(lab.channel,bearer);
    require(sender.load(routed,now)==ReplySendOutcome::Loaded,"reply sender load");
    while(sender.pending())sender.service(now++,20);
    require(bearer.document(false)==std::string(reply,count),"actual reply frames preserve codec JSON");
    std::puts(reply); return routed.reply;
}
int main() {
    BootIdentity boot; boot.high=0x12345678;boot.low=0x9abcdef0;
    require(lab.begin(boot),"begin");
    for(unsigned i=0;i<256;++i) lab.tick(now++);
    TrustedAuthority a; lab.channel.advanceAuthority(7,true,a);lab.tick(now++);
    ConfiguredStatus status;require(lab.channel.readStatus(status),"status");
    require(status.hardwareReady && status.hasActive && status.storedMatchesActive,"ready synthetic profile");
    char json[6144];std::size_t count=0;require(encodeConfiguredStatus(status,json,sizeof json,count)==ConfiguredEncodeResult::Success,"status json");std::puts(json);
    RoutedConfiguredStatus routedStatus;require(lab.channel.readCurrentStatus(routedStatus),"routed status");
    Capture capture;StatusFrameSender statusSender(lab.channel,capture);
    require(statusSender.load(routedStatus,now)==StatusSendOutcome::Loaded,"status load");
    while(statusSender.pending())statusSender.service(now++,20);
    require(capture.document(true)==std::string(json,count),"actual status frames preserve frozen snapshot");
    GameCommand c;c.boot=boot;c.lease=status.controlLease;c.request=1;c.expectedConfigurationRevision=status.configurationRevision;c.expectedStateRevision=status.stateRevision;
    auto first=command(c);require(first.outcome==GameReplyOutcome::Executed && first.consumed,"start");
    auto reply=command(c);require(reply.duplicate && reply.stateRevision==first.stateRevision,"duplicate does not execute");
    c.kind=GameCommandKind::Pause;reply=command(c);require(reply.outcome==GameReplyOutcome::RequestConflict && !reply.consumed,"conflict");
    c.request=2;c.expectedConfigurationRevision=0xffffffff;reply=command(c);require(reply.outcome==GameReplyOutcome::StaleConfiguration && reply.consumed && reply.nextRequest==3,"stale config consumes");
    c.request=3;c.expectedConfigurationRevision=reply.configurationRevision;c.expectedStateRevision=reply.stateRevision;c.boot.low^=1;
    reply=command(c);require(reply.outcome==GameReplyOutcome::WrongBoot && !reply.consumed,"wrong boot");c.boot=boot;
    c.request=4;reply=command(c);require(reply.outcome==GameReplyOutcome::OutOfOrder && !reply.consumed,"future request");
    c.request=3;c.expectedStateRevision=0xffffffff;reply=command(c);require(reply.outcome==GameReplyOutcome::StaleState && reply.consumed,"stale state");
    c.request=4;c.kind=GameCommandKind::Stop;c.expectedStateRevision=reply.stateRevision;reply=command(c);require(reply.outcome==GameReplyOutcome::Executed && !lab.application.gameRunning(),"stop");
    lab.channel.advanceAuthority(0,false,a);lab.tick(now++);lab.channel.advanceAuthority(7,true,a);lab.tick(now++);
    c.request=1;reply=command(c);require(reply.outcome==GameReplyOutcome::Unauthorized && !reply.consumed,"old lease after reconnect");
    require(!host_rtos::nonzeroWait,"zero wait queues");std::puts("PASS: real v2 core, 20-byte command frames, start/stop/replay/conflict/revisions/boot/order/reconnect");
    TrustedAuthority current;require(lab.channel.readAuthority(current),"current authority");
    char commandJson[512];std::size_t commandCount=0;encodeGameCommand(c,commandJson,sizeof commandJson,commandCount);
    char frame[20];std::size_t written=0;encodeCommandFrame(100,commandCount,0,commandJson,12,frame,sizeof frame,written);
    CommandStream stream(lab.channel);
    require(stream.receive(frame,written,current.principal,current.epoch,now).outcome==CommandStreamOutcome::Receiving,"first fragment");
    require(stream.receive(frame,written,current.principal,current.epoch,now+1).outcome==CommandStreamOutcome::DuplicateFragment,"duplicate fragment");
    frame[8]^=1;
    require(stream.receive(frame,written,current.principal,current.epoch,now+2).outcome==CommandStreamOutcome::FragmentConflict,"conflicting fragment");
    frame[8]^=1;
    require(stream.receive(frame,written,current.principal,current.epoch,now+3).outcome==CommandStreamOutcome::Receiving,"restart after conflict");
    require(stream.poll(now+2003).outcome==CommandStreamOutcome::Expired,"absolute command expiry");
    CommandFrame decoded;require(decodeCommandFrame(frame,8,decoded)==FrameDecodeResult::Invalid,"empty payload rejected");
    frame[1]=2;require(decodeCommandFrame(frame,written,decoded)==FrameDecodeResult::Unsupported,"wrong frame kind rejected");
    std::string invalid(commandJson,commandCount);invalid.insert(1,"\"principal\":7,");GameCommand destination;
    require(decodeGameCommand(invalid.data(),invalid.size(),destination).error==CommandDecodeError::UnknownField,"client authority injection rejected");
    std::puts("PASS: actual reply/status senders and decoders, immutable JSON, fragment duplicate/conflict/deadline/size/kind, client authority injection");
}
