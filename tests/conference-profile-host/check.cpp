// Real production validation, decoder, and atomic storage against a temporary
// filesystem. Synthetic photos only; never accesses board ports or user files.
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <Arduino.h>
static std::string root;
static bool mountFailed=false,renameFailed=false,allocationFailed=false,writeFailed=false,syncFailed=false,closeFailed=false;
static unsigned renames=0,formatCalls=0;
struct FakeLittleFS {
  bool begin(bool format,const char *base,uint8_t maxFiles,const char *label){
    assert(root==base&&maxFiles==4&&std::string(label)=="ffat");
    if(format&&mountFailed){formatCalls++;std::filesystem::remove_all(root);std::filesystem::create_directory(root);mountFailed=false;}
    return !mountFailed;
  }
  bool rename(const String &from,const String &to){if(renameFailed)return false;renames++;return std::rename((root+from.c_str()).c_str(),(root+to.c_str()).c_str())==0;}
} LittleFS;
static constexpr int ESP_PARTITION_TYPE_DATA=1,ESP_PARTITION_SUBTYPE_DATA_FAT=0x81;
struct esp_partition_t {uint32_t address=0x610000,size=0x9E0000;};
static esp_partition_t partition;
const esp_partition_t *esp_partition_find_first(int type,int subtype,const char *label){assert(type==1&&subtype==0x81&&std::string(label)=="ffat");return &partition;}
void *ps_malloc(size_t n){return allocationFailed?nullptr:malloc(n);}
size_t checkedWrite(const void *data,size_t size,size_t count,FILE *file){if(writeFailed)return std::fwrite(data,size,count?count-1:0,file);return std::fwrite(data,size,count,file);}
int checkedSync(int fd){return syncFailed?-1:fsync(fd);}
int checkedClose(FILE *file){int result=std::fclose(file);return closeFailed?EOF:result;}
#define CONFERENCE_STORE_ROOT root.c_str()
#define fwrite checkedWrite
#define fsync checkedSync
#define fclose checkedClose
#include "../../firmware/devices_badge/conference_profile_store.h"
#undef fwrite
#undef fsync
#undef fclose
namespace detail=conference_store_detail;
std::vector<uint8_t> read(const std::string &path){std::ifstream stream(path,std::ios::binary);return {std::istreambuf_iterator<char>(stream),{}};}
void write(const std::string &path,const std::vector<uint8_t> &bytes){std::ofstream stream(path,std::ios::binary);stream.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());}
void nameTests(){
  String result;
  assert(conferenceName("  Alex Example  ",result)&&result=="Alex Example");
  assert(conferenceName("Jos\xc3\xa9",result)&&result=="Jos\xc3\xa9");
  assert(conferenceName(String(std::string(60,'a')),result));
  assert(!conferenceName(String(std::string(61,'a')),result));
  for(const char *invalid:{"a\nb","a\rb","\xc0\x80","\xed\xa0\x80","\xf4\x90\x80\x80","\xc2\x80","\xe2\x80\xae","\xe2\x81\xa6"})assert(!conferenceName(invalid,result));
  assert(!conferenceName(String("a\0b",3),result));
  assert(conferenceName("",result)&&result.isEmpty());
}
void urlTests(){
  String out;
  struct Case {ConferenceNetwork network;const char *input;const char *output;};
  for(auto test:{Case{ConferenceNetwork::GitHub," @example-dev ","https://github.com/example-dev"},
    Case{ConferenceNetwork::GitHub,"https://www.github.com/Example/","https://github.com/Example"},
    Case{ConferenceNetwork::X,"https://twitter.com/example_1/","https://x.com/example_1"},
    Case{ConferenceNetwork::LinkedIn,"https://linkedin.com/in/example-dev","https://www.linkedin.com/in/example-dev/"}}){assert(conferenceSocialUrl(test.network,test.input,out)&&out==test.output);}
  for(unsigned n=0;n<3;n++){
    auto network=ConferenceNetwork(n);assert(conferenceSocialUrl(network,"",out)&&out.isEmpty());
    for(const char *bad:{"https://evil.example/user","http://github.com/example","https://github.com.evil/example","https://user@github.com/example","example?x=1","example#x","example%2fother","example/other","a\\b","a b","a\nb","\xc3\xa9","javascript:alert(1)","@@example","//github.com/example"})assert(!conferenceSocialUrl(network,bad,out));
  }
  for(const char *bad:{"-a","a-","a--b","a_b"})assert(!conferenceSocialUrl(ConferenceNetwork::GitHub,bad,out));
  assert(!conferenceSocialUrl(ConferenceNetwork::X,"abcdefghijklmnop",out));
  assert(!conferenceSocialUrl(ConferenceNetwork::LinkedIn,"https://linkedin.com/company/example/",out));
}
int main(int argc,char **argv){
  assert(argc==3);nameTests();urlTests();
  auto jpeg=read(argv[1]),oversizedDimensions=read(argv[2]);assert(!jpeg.empty()&&!oversizedDimensions.empty());
  char folder[]="/tmp/init-conference-store-XXXXXX";root=mkdtemp(folder);
  const std::string record=root+detail::RECORD,legacy=root+"/profile-0.bin";
  const std::vector<uint8_t> legacyBytes={'l','e','g','a','c','y'};write(legacy,legacyBytes);
  partition.address++;{ConferenceProfileStore wrong;assert(!wrong.begin()&&!wrong.ready()&&!wrong.initializeForConference()&&formatCalls==0);}partition.address--;
  mountFailed=true;{ConferenceProfileStore failure;assert(!failure.begin()&&!failure.ready());}mountFailed=false;
  ConferenceProfileStore store;assert(store.begin()&&store.ready()&&store.profile().name.isEmpty()&&!store.avatarPixels());
  assert(store.initializeForConference()&&formatCalls==0&&read(legacy)==legacyBytes);
  ConferenceProfile profile;profile.name="Synthetic Test";profile.urls[1]="https://x.com/example";
  assert(store.save(profile,jpeg.data(),jpeg.size(),true));assert(store.avatarWidth()==160&&store.avatarHeight()==160);
  assert(store.profile().urls[0].isEmpty()&&store.profile().urls[1]=="https://x.com/example"&&store.profile().urls[2].isEmpty());
  assert((store.avatarPixels()[0]&0xf800)==0xf800);
  auto initial=read(record);auto revision=store.revision();
  ConferenceProfile changed=profile;changed.name="Different Name";changed.urls[1]="";changed.urls[2]="https://www.linkedin.com/in/example/";
  assert(!store.save(changed,oversizedDimensions.data(),oversizedDimensions.size(),true));assert(read(record)==initial&&store.profile().name==profile.name&&store.revision()==revision);
  std::vector<uint8_t> overBytes(128*1024+1);overBytes[0]=255;overBytes[1]=216;
  assert(!store.save(changed,overBytes.data(),overBytes.size(),true));assert(read(record)==initial);
  for(bool *failure:{&renameFailed,&allocationFailed,&writeFailed,&syncFailed,&closeFailed}){
    *failure=true;assert(!store.save(changed));*failure=false;
    assert(read(record)==initial&&store.profile().name==profile.name&&store.revision()==revision);
    ConferenceProfileStore reloaded;assert(reloaded.begin()&&reloaded.profile().name==profile.name&&reloaded.avatarPixels());
  }
  assert(store.save(changed));assert(store.avatarPixels()&&store.profile().urls[1].isEmpty());
  {ConferenceProfileStore reloaded;assert(reloaded.begin()&&reloaded.profile().name==changed.name&&reloaded.profile().urls[1].isEmpty()&&reloaded.profile().urls[2]==changed.urls[2]&&reloaded.avatarPixels());}
  auto intact=read(record);
  for(size_t offset:{size_t(0),size_t(8),size_t(12),size_t(16),size_t(20),size_t(24),size_t(32),size_t(68),intact.size()-1}){
    auto corrupt=intact;corrupt[offset]^=0x80;write(record,corrupt);
    ConferenceProfileStore reloaded;assert(reloaded.begin()&&reloaded.profile().name.isEmpty()&&!reloaded.avatarPixels()&&!reloaded.error().isEmpty());
  }
  write(record,intact);assert(store.save(changed,nullptr,0,true));assert(!store.avatarPixels());
  {ConferenceProfileStore reloaded;assert(reloaded.begin()&&reloaded.profile().name==changed.name&&!reloaded.avatarPixels());}
  assert(store.clear());assert(store.profile().name.isEmpty()&&!store.avatarPixels());
  for(const auto &url:store.profile().urls)assert(url.isEmpty());
  {ConferenceProfileStore reloaded;assert(reloaded.begin()&&reloaded.profile().name.isEmpty()&&!reloaded.avatarPixels());}
  assert(read(legacy)==legacyBytes);
  mountFailed=true;
  {ConferenceProfileStore blank;assert(!blank.begin()&&formatCalls==0&&read(legacy)==legacyBytes);assert(blank.initializeForConference()&&blank.ready()&&formatCalls==1);assert(!std::filesystem::exists(legacy));}
  std::filesystem::remove_all(root);
  std::cout<<"Conference profile: URL/name validation, actual JPEG limits, isolated atomic storage, injected failures, corruption, edit/reload, remove/clear passed\n";
  return 0;
}
