#define main storageFixtureMain
#include "check.cpp"
#undef main
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#define CONFERENCE_CLOCK_TEST_EXTERNAL_STRING
#include "M5Unified.h"
#include <sys/time.h>
static int portalSettimeofday(const timeval *value,const void*){testNow=value->tv_sec;return 0;}
static time_t portalTime(time_t *value){if(value)*value=testNow;return testNow;}
#define settimeofday portalSettimeofday
#define time(...) portalTime(__VA_ARGS__)
#include "../../firmware/devices_badge/conference_clock.h"
#undef settimeofday
#undef time
#include "../../firmware/devices_badge/conference_portal.h"
#include <chrono>
#include <sys/ioctl.h>
struct WireClient {
  ConferencePortal &portal;
  int peer=-1,responseCode=0;
  String response;
  std::shared_ptr<TestSocket> socket;
  unsigned ticks=0;bool eof=false;
  explicit WireClient(ConferencePortal &p):portal(p){}
  ~WireClient(){disconnect();}
  void disconnect(){if(peer>=0)::close(peer);peer=-1;socket.reset();}
  void connect(bool local=true){disconnect();eof=false;response="";responseCode=0;socket=NetworkServer::last->connect(peer,local);}
  void step(){
    auto start=std::chrono::steady_clock::now();portal.tick();
    auto elapsed=std::chrono::steady_clock::now()-start;
    // This bounds time spent waiting on network progress; filesystem/JPEG
    // callbacks are independently bounded by their production input limits.
    assert(elapsed<std::chrono::milliseconds(250));ticks++;
  }
  void sendBytes(const uint8_t *bytes,size_t count){
    size_t offset=0;unsigned attempts=0;
    while(offset<count){assert(++attempts<10000);ssize_t sent=::send(peer,bytes+offset,count-offset,MSG_DONTWAIT);
      if(sent<0){assert(errno==EAGAIN||errno==EWOULDBLOCK);step();continue;}assert(sent>0);offset+=size_t(sent);
    }
  }
  void sendText(const std::string &text){sendBytes(reinterpret_cast<const uint8_t*>(text.data()),text.size());}
  void finish(){
    std::string all;
    for(unsigned attempts=0;attempts<10000;attempts++){
      step();char block[4096];ssize_t count=::recv(peer,block,sizeof(block),MSG_DONTWAIT);
      if(count>0){all.append(block,size_t(count));continue;}
      if(count==0){eof=true;break;}assert(errno==EAGAIN||errno==EWOULDBLOCK);
    }
    assert(eof);auto split=all.find("\r\n\r\n");
    if(split==std::string::npos){responseCode=0;response="";return;}
    assert(sscanf(all.c_str(),"HTTP/1.1 %d",&responseCode)==1);response=all.substr(split+4);
    auto length=all.find("Content-Length: ");assert(length!=std::string::npos);assert(std::stoul(all.substr(length+16))==response.length());
  }
  void get(const std::string &path){connect();sendText("GET "+path+" HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n");finish();}
  void post(const char *path,const std::vector<uint8_t> &bytes,const String &nonce,const char *type="application/json",int announced=-1){
    connect();size_t length=announced<0?bytes.size():size_t(announced);
    sendText(std::string("POST ")+path+" HTTP/1.1\r\nHost: 192.168.4.1\r\nContent-Length: "+std::to_string(length)+"\r\nContent-Type: "+type+"\r\nX-Conference-Nonce: "+nonce.c_str()+"\r\n\r\n");
    if(!bytes.empty())sendBytes(bytes.data(),bytes.size());finish();
  }
  void post(const char *path,const std::string &body,const String &nonce,const char *type="application/json"){post(path,std::vector<uint8_t>(body.begin(),body.end()),nonce,type);}
};
String getNonce(WireClient &web){web.get("/");assert(web.responseCode==200);std::string html=web.response.c_str();std::string prefix="const nonce='";auto start=html.find(prefix);assert(start!=std::string::npos);return html.substr(start+prefix.size(),32);}
std::string form(const char *name,const char *github,const char *x,const char *linkedin,const char *image="keep",const String &token=""){
  JsonDocument json;json["name"]=name;json["github"]=github;json["x"]=x;json["linkedin"]=linkedin;json["image"]=image;json["imageToken"]=token;std::string body;serializeJson(json,body);return body;
}
void clockEndpointTests(ConferencePortal &portal,ConferenceProfileStore &store,WireClient &wire,const String &nonce){
  const auto original=read(root+conference_store_detail::RECORD);const auto revision=store.revision();
  const auto unchanged=[&](){assert(read(root+conference_store_detail::RECORD)==original&&store.revision()==revision&&portal.active()&&portal.outcome()==ConferencePortalOutcome::None);};
  const std::string valid=R"({"epoch":1789651234,"offset_minutes":345,"timezone":"Asia/Kathmandu"})";
  const auto originalClock=testNow;
  wire.post("/clock",valid,"00000000000000000000000000000000");assert(wire.responseCode==403&&testNow==originalClock);unchanged();
  for(const char *bad:{"[]","{}",R"({"epoch":"1789651234","offset_minutes":345})",R"({"epoch":1789651234.5,"offset_minutes":345})",
    R"({"epoch":1789651234,"offset_minutes":true})",R"({"epoch":0,"offset_minutes":345})",R"({"epoch":1789651234,"offset_minutes":841})",
    R"({"epoch":1789651234,"offset_minutes":345,"timezone":false})",R"({"epoch":1789651234,"offset_minutes":345,"timezone":"Asia/Kathmandu\u0000Other"})",
    R"({"epoch":1789651234,"offset_minutes":345,"timezone":"Invalid zone"})",R"({"epoch":1789651234,"offset_minutes":345,"name":"Unexpected"})"}){
    wire.post("/clock",std::string(bad),nonce);assert(wire.responseCode==400&&testNow==originalClock);unchanged();
  }
  wire.post("/clock",std::string(513,'x'),nonce);assert(wire.responseCode==413);unchanged();
  wire.post("/clock",valid,nonce);assert(wire.responseCode==200);JsonDocument result;assert(!deserializeJson(result,wire.response.c_str()));
  assert(result["ok"].as<bool>()&&result["valid"].as<bool>()&&std::string(result["source"].as<const char*>())=="phone"&&result["rtc_epoch"].as<int64_t>()==1789651234&&result["offset_minutes"].as<int>()==345);unchanged();
  // A timezone name never overrides the explicit checked numerical offset.
  wire.post("/clock",std::string(R"({"epoch":1789651234,"offset_minutes":345,"timezone":"Pacific/Honolulu"})"),nonce);
  assert(wire.responseCode==200&&conference_clock::offsetMinutes==345);unchanged();
  M5.Rtc.enabled=false;wire.post("/clock",valid,nonce);assert(wire.responseCode==400);assert(!deserializeJson(result,wire.response.c_str()));
  assert(!result["ok"].as<bool>()&&std::string(result["error"].as<const char*>())=="rtc_unavailable");unchanged();M5.Rtc.enabled=true;
  wire.post("/clock",valid,nonce);assert(wire.responseCode==200&&conferenceClockValid()&&conferenceClockEpoch()==1789651234);unchanged();
}
void beginUpload(WireClient &wire,const String &nonce,size_t length){
  wire.connect();wire.sendText("POST /image HTTP/1.1\r\nHost: 192.168.4.1\r\nContent-Length: "+std::to_string(length)+"\r\nContent-Type: image/jpeg\r\nX-Conference-Nonce: "+nonce.c_str()+"\r\n\r\n");wire.step();assert(wire.socket->descriptor>=0);
}
void responsivenessTests(ConferencePortal &portal,ConferenceProfileStore &store,WireClient &wire){
  const auto original=read(root+conference_store_detail::RECORD);const auto revision=store.revision();
  const auto unchanged=[&](){assert(read(root+conference_store_detail::RECORD)==original&&store.revision()==revision);};
  // A fully authorized JPEG request stays incomplete while a real socket peer
  // sends a byte every500ms. Tick/UI work runs every100ms and the ABSOLUTE
  // deadline still wins, although each byte resets the idle-progress clock.
  assert(portal.start());auto nonce=getNonce(wire);beginUpload(wire,nonce,32000);
  unsigned before=wire.ticks;uint32_t start=testMillis;
  for(unsigned i=0;i<100;i++){
    if(i%5==0)wire.sendText("x");testMillis+=100;wire.step();
    if(i<99)assert(wire.socket->descriptor>=0&&portal.active());
    if(i==4){const auto &d=portal.transportDiagnostics();assert(d.active&&d.currentMethod==2&&d.currentPhase==2&&d.currentExpectedBody==32000&&d.currentBodyBytes==1&&d.currentAgeMs==500&&d.currentIdleMs==400);}
  }
  assert(testMillis-start==ConferenceTransport::REQUEST_TIMEOUT_MS&&wire.ticks-before==100&&wire.socket->descriptor<0&&portal.active());unchanged();
  {const auto &d=portal.transportDiagnostics();assert(std::string(d.lastClose)=="request_timeout"&&d.lastErrno==0&&d.lastAgeMs==10000&&d.lastMethod==2&&d.lastPhase==2&&d.lastStatus==0&&d.lastBodyBytes==20&&d.lastExpectedBody==32000);}
  auto postCloses=portal.transportDiagnostics().postCloses;
  getNonce(wire);
  {const auto &d=portal.transportDiagnostics();assert(!d.active&&d.lastMethod==1&&d.lastStatus==200&&d.postCloses==postCloses&&std::string(d.lastPostClose)=="request_timeout"&&d.lastPostAgeMs==10000&&d.lastPostBodyBytes==20&&d.lastPostExpectedBody==32000);}
  portal.stop();

  // Physical Back is handled by the main loop between ticks. It can cancel a
  // stalled transfer immediately without waiting for another incoming byte.
  assert(portal.start());nonce=getNonce(wire);beginUpload(wire,nonce,32000);wire.sendText("x");wire.step();
  portal.stop();assert(!portal.active()&&wire.socket->descriptor<0&&WiFi.modeValue==WIFI_OFF&&portal.outcome()==ConferencePortalOutcome::Cancelled);unchanged();
  assert(std::string(portal.transportDiagnostics().lastClose)=="stopped");

  // Idle and header deadlines are separate from the body absolute deadline.
  assert(portal.start());nonce=getNonce(wire);beginUpload(wire,nonce,32000);
  testMillis+=ConferenceTransport::IDLE_TIMEOUT_MS;wire.step();assert(wire.socket->descriptor<0&&portal.active());unchanged();
  assert(std::string(portal.transportDiagnostics().lastClose)=="request_idle_timeout");
  wire.connect();wire.sendText("G");wire.step();
  for(unsigned i=0;i<10;i++){wire.sendText("x");testMillis+=500;wire.step();if(i<9)assert(wire.socket->descriptor>=0);}
  assert(wire.socket->descriptor<0&&portal.active());portal.stop();
  assert(std::string(portal.transportDiagnostics().lastClose)=="header_timeout");

  // Session timeout runs before network processing even with upload progress.
  assert(portal.start());nonce=getNonce(wire);testMillis+=599700;beginUpload(wire,nonce,32000);
  for(unsigned i=0;i<3;i++){wire.sendText("x");testMillis+=100;wire.step();}
  assert(!portal.active()&&wire.socket->descriptor<0&&portal.outcome()==ConferencePortalOutcome::TimedOut&&WiFi.modeValue==WIFI_OFF);unchanged();

  // Actual socket consumption verifies the per-tick budget independently of
  // deadline helpers. A queued burst cannot monopolize a UI iteration.
  assert(portal.start());nonce=getNonce(wire);beginUpload(wire,nonce,32000);
  std::string burst(4096,'x');wire.sendText(burst);int queued=0,remaining=0;
  assert(ioctl(wire.socket->descriptor,FIONREAD,&queued)==0&&queued==4096);wire.step();
  assert(ioctl(wire.socket->descriptor,FIONREAD,&remaining)==0&&queued-remaining==int(ConferenceTransport::IO_BUDGET));
  portal.stop();unchanged();

  // A nonreading phone cannot block response writes. Fill the actual outbound
  // socket first, then let the production transport receive a normal GET.
  assert(portal.start());wire.connect();wire.step();int small=1024;assert(setsockopt(wire.socket->descriptor,SOL_SOCKET,SO_SNDBUF,&small,sizeof(small))==0);
  uint8_t filler[2048]={};unsigned filled=0;
  for(;;){ssize_t sent=::send(wire.socket->descriptor,filler,sizeof(filler),MSG_DONTWAIT);if(sent<0){assert(errno==EAGAIN||errno==EWOULDBLOCK);break;}assert(sent>0&&++filled<10000);}
  wire.sendText("GET / HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n");wire.step();
  before=wire.ticks;for(unsigned i=0;i<15;i++){testMillis+=100;wire.step();}
  assert(wire.ticks-before==15&&wire.socket->descriptor<0&&portal.active());portal.stop();unchanged();
}
int main(int argc,char **argv){
  const auto gate=[](const std::string &request){return conference_http_gate::inspect(request.data(),request.size());};
  assert(gate("GET / HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n")==0);
  assert(gate("POST /save HTTP/1.1\r\nContent-Length: 200\r\nContent-Type: application/json\r\n\r\n")==0);
  assert(gate("POST /image HTTP/1.1\r\nContent-Length: 131072\r\nContent-Type: image/jpeg\r\n\r\n")==0);
  assert(gate("POST /clock HTTP/1.1\r\nContent-Length: 512\r\nContent-Type: application/json\r\n\r\n")==0);
  assert(gate("POST /clock HTTP/1.1\r\nContent-Length: 513\r\nContent-Type: application/json\r\n\r\n")==413);
  assert(gate("POST /clock HTTP/1.1\r\nContent-Length: 20\r\nContent-Type: image/jpeg\r\n\r\n")==415);
  for(const char *method:{"PUT","PATCH","DELETE","OPTIONS"})assert(gate(std::string(method)+" /save HTTP/1.1\r\nContent-Length: 9999999\r\n\r\n")==405);
  assert(gate("POST /save HTTP/1.1\r\nContent-Length: 20\r\nContent-Type: multipart/form-data; boundary=x\r\n\r\n")==415);
  assert(gate("POST /save HTTP/1.1\r\nContent-Length: 2049\r\nContent-Type: application/json\r\n\r\n")==413);
  assert(gate("POST /image HTTP/1.1\r\nContent-Length: 131073\r\nContent-Type: image/jpeg\r\n\r\n")==413);
  assert(gate("POST /save HTTP/1.1\r\nContent-Length: 100\r\nContent-Length: 999\r\nContent-Type: application/json\r\n\r\n")==400);
  assert(gate("POST /save HTTP/1.1\r\nContent-Length: -1\r\nContent-Type: application/json\r\n\r\n")==400);
  assert(gate("POST /save HTTP/1.1\r\nContent-Length: 2\r\nTransfer-Encoding: chunked\r\nContent-Type: application/json\r\n\r\n")==400);
  assert(gate("POST /save HTTP/1.1\r\nContent-Length: 2\r\nContent-Type: application/json\r\nContent-Type: multipart/form-data\r\n\r\n")==400);
  assert(gate("GET / HTTP/1.1\r\nContent-Length: 99\r\n\r\n")==413);
  assert(gate("GET / HTTP/1.1\r\nX-Padding: "+std::string(4096,'x')+"\r\n\r\n")==431);
  assert(argc==2||argc==3);auto jpeg=read(argv[1]);assert(!jpeg.empty());char directory[]="/tmp/init-portal-test-XXXXXX";root=mkdtemp(directory);
  ConferenceProfileStore store;assert(store.begin());ConferencePortal portal(store);WireClient web(portal);
  assert(!portal.start("too-short")&&WiFi.modeValue==WIFI_OFF);
  assert(portal.start("abcdefghijkl")&&portal.active()&&WiFi.modeValue==WIFI_AP&&!WiFi.autoReconnect&&!WiFi.persist);
  assert(portal.ssid()=="init-badge-123456789abc");auto nonce=getNonce(web);
  if(argc==3){std::ofstream page(argv[2]);page<<web.response.c_str();}
  auto body=form("Alex <&\" Example","","@example","");
  web.post("/save",body,"invalid");assert(web.responseCode==403&&store.profile().name.isEmpty());
  web.post("/save",form("Alex","https://evil.example/user","",""),nonce);assert(web.responseCode==400&&store.profile().name.isEmpty());
  web.post("/save",std::string("{\"name\":\"Alex\\u0000Other\",\"github\":\"\",\"x\":\"\",\"linkedin\":\"\",\"image\":\"keep\",\"imageToken\":\"\"}"),nonce);assert(web.responseCode==400);
  web.post("/image",jpeg,nonce,"image/jpeg");assert(web.responseCode==200&&store.profile().name.isEmpty()&&!store.avatarPixels());JsonDocument uploaded;assert(!deserializeJson(uploaded,web.response.c_str()));String token=uploaded["imageToken"].as<String>();assert(!token.isEmpty());
  clockEndpointTests(portal,store,web,nonce);
  renameFailed=true;web.post("/save",form("Alex","","@example","","staged",token),nonce);renameFailed=false;assert(web.responseCode==500&&store.profile().name.isEmpty()&&!store.avatarPixels()&&portal.active());
  web.post("/save",form("Alex <&\" Example","","@example","","staged",token),nonce);assert(web.responseCode==200&&store.profile().name=="Alex <&\" Example"&&store.avatarPixels());
  assert(portal.outcome()==ConferencePortalOutcome::Saved&&portal.active()&&WiFi.modeValue==WIFI_AP);
  testMillis+=2999;portal.tick();assert(portal.active());testMillis++;portal.tick();assert(!portal.active()&&WiFi.modeValue==WIFI_OFF&&portal.password().isEmpty());
  assert(portal.start());nonce=getNonce(web);assert(std::string(web.response.c_str()).find("Alex &lt;&amp;&quot; Example")!=std::string::npos);assert(std::string(web.response.c_str()).find("https://x.com/example")!=std::string::npos);
  web.post("/image",jpeg,nonce,"image/jpeg");assert(web.responseCode==200);web.post("/cancel",std::string("{}"),nonce);assert(web.responseCode==200&&portal.outcome()==ConferencePortalOutcome::Cancelled&&portal.active());testMillis+=3000;portal.tick();assert(!portal.active()&&store.profile().name=="Alex <&\" Example");
  assert(conferenceClockValid()&&conferenceClockEpoch()==1789651234&&conference_clock::offsetMinutes==345);
  assert(portal.start());nonce=getNonce(web);web.post("/save",form("Edited","example","","","remove"),nonce);assert(web.responseCode==200&&!store.avatarPixels()&&store.profile().urls[1].isEmpty());portal.stop();assert(portal.outcome()==ConferencePortalOutcome::Saved);
  {ConferenceProfileStore restored;assert(restored.begin()&&restored.profile().name=="Edited"&&restored.profile().urls[0]=="https://github.com/example"&&restored.profile().urls[1].isEmpty()&&!restored.avatarPixels());}
  assert(portal.start());nonce=getNonce(web);web.post("/image",std::vector<uint8_t>{},nonce,"image/jpeg",128*1024+1);assert(web.responseCode==413&&portal.active());
  portal.stop();assert(portal.outcome()==ConferencePortalOutcome::Cancelled&&WiFi.modeValue==WIFI_OFF);
  assert(portal.start());nonce=getNonce(web);testMillis+=599900;web.post("/cancel",std::string("{}"),nonce);testMillis+=500;portal.tick();assert(portal.active());testMillis+=2500;portal.tick();assert(!portal.active());
  testMillis=uint32_t(0)-3000;assert(portal.start());nonce=getNonce(web);web.post("/cancel",std::string("{}"),nonce);testMillis+=2999;portal.tick();assert(portal.active());testMillis++;portal.tick();assert(!portal.active());
  assert(portal.start());testMillis+=600000;portal.tick();assert(portal.outcome()==ConferencePortalOutcome::TimedOut&&!portal.active()&&WiFi.modeValue==WIFI_OFF);
  responsivenessTests(portal,store,web);
  NetworkServer::failBegin=true;unsigned stopsBefore=NetworkServer::stopCalls;
  assert(!portal.start()&&NetworkServer::stopCalls==stopsBefore+1&&!portal.active()&&portal.outcome()==ConferencePortalOutcome::Error&&WiFi.modeValue==WIFI_OFF&&portal.password().isEmpty());
  NetworkServer::failBegin=false;assert(portal.start());portal.stop();
  WiFi.failStart=true;assert(!portal.start()&&portal.outcome()==ConferencePortalOutcome::Error&&WiFi.modeValue==WIFI_OFF&&portal.password().isEmpty());
  std::filesystem::remove_all(root);std::cout<<"Conference portal: real socket transport, trickled authorized uploads, responsive ticks/cancel, absolute/idle/session deadlines, IO budget, response backpressure, authorization, escaped prefill, atomic save and3s ack grace passed\n";
}
