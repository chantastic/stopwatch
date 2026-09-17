#pragma once
#include "conference_profile.h"
#include "avatar_decode.h"
#include <LittleFS.h>
#include <SHA2Builder.h>
#include <esp_partition.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <utility>

#ifndef CONFERENCE_STORE_ROOT
#define CONFERENCE_STORE_ROOT "/badgefs"
#endif

namespace conference_store_detail {
static constexpr size_t HEADER=64,METADATA_MAX=768,AVATAR_SIDE=160,AVATAR_BYTES=AVATAR_SIDE*AVATAR_SIDE*2;
static constexpr uint8_t MAGIC[8]={'I','N','I','T','C','F','0','1'};
static constexpr char RECORD[]="/conference-manual-v1.bin",TEMP[]="/conference-manual-v1.tmp";
static inline uint16_t u16(const uint8_t *p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
static inline uint32_t u32(const uint8_t *p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
static inline void put16(uint8_t *p,uint16_t v){p[0]=v;p[1]=v>>8;}
static inline void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=v>>(i*8);}
static inline String path(const char *name){return String(CONFERENCE_STORE_ROOT)+name;}
struct RecordFile {
  FILE *value;
  RecordFile(const String &name,const char *mode):value(fopen(name.c_str(),mode)){}
  ~RecordFile(){if(value)fclose(value);}
  bool close(){if(!value)return false;FILE *old=value;value=nullptr;return fclose(old)==0;}
};
struct Record {
  ConferenceProfile profile;uint16_t *pixels=nullptr;uint8_t digest[32]={};
  ~Record(){if(pixels)free(pixels);}
};
static inline size_t encode(const ConferenceProfile &profile,uint8_t *bytes) {
  const String *fields[]={&profile.name,&profile.urls[0],&profile.urls[1],&profile.urls[2]};size_t used=0;
  for(const String *field:fields){if(used+2+field->length()>METADATA_MAX)return 0;put16(bytes+used,field->length());used+=2;memcpy(bytes+used,field->c_str(),field->length());used+=field->length();}
  return used;
}
static inline bool decode(const uint8_t *bytes,size_t size,ConferenceProfile &profile) {
  String *fields[]={&profile.name,&profile.urls[0],&profile.urls[1],&profile.urls[2]};size_t used=0;
  for(String *field:fields){if(used+2>size)return false;size_t length=u16(bytes+used);used+=2;if(length>size-used||memchr(bytes+used,0,length))return false;*field=String(reinterpret_cast<const char*>(bytes+used),length);if(field->length()!=length)return false;used+=length;}
  return used==size&&conferenceProfileValid(profile);
}
static inline bool read(const char *name,Record &record) {
  RecordFile file(path(name),"rb");if(!file.value)return false;
  struct stat info;uint8_t header[HEADER],metadata[METADATA_MAX];
  if(fstat(fileno(file.value),&info)||!S_ISREG(info.st_mode)||info.st_size<int64_t(HEADER)||
    info.st_size>int64_t(HEADER+METADATA_MAX+AVATAR_BYTES)||fread(header,1,HEADER,file.value)!=HEADER)return false;
  size_t metadataBytes=u32(header+16),pixelBytes=u32(header+20);
  if(memcmp(header,MAGIC,8)||u16(header+8)!=1||u16(header+10)!=HEADER||u16(header+12)!=AVATAR_SIDE||u16(header+14)!=AVATAR_SIDE||
    !metadataBytes||metadataBytes>METADATA_MAX||(pixelBytes!=0&&pixelBytes!=AVATAR_BYTES)||u32(header+24)||u32(header+28)||
    uint64_t(info.st_size)!=HEADER+metadataBytes+pixelBytes)return false;
  if(fread(metadata,1,metadataBytes,file.value)!=metadataBytes||!decode(metadata,metadataBytes,record.profile))return false;
  SHA256Builder hash;hash.begin();hash.add(header,32);hash.add(metadata,metadataBytes);
  if(pixelBytes){record.pixels=static_cast<uint16_t*>(ps_malloc(pixelBytes));if(!record.pixels||fread(record.pixels,1,pixelBytes,file.value)!=pixelBytes)return false;hash.add(reinterpret_cast<uint8_t*>(record.pixels),pixelBytes);}
  if(ferror(file.value)||!file.close())return false;
  hash.calculate();hash.getBytes(record.digest);return memcmp(record.digest,header+32,32)==0;
}
} // namespace conference_store_detail

class ConferenceProfileStore {
  ConferenceProfile profile_;uint16_t *pixels_=nullptr;bool ready_=false;uint32_t revision_=0;String error_;
  bool fail(const char *message){error_=message;return false;}
public:
  ConferenceProfileStore()=default;
  ConferenceProfileStore(const ConferenceProfileStore&)=delete;
  ConferenceProfileStore &operator=(const ConferenceProfileStore&)=delete;
  ~ConferenceProfileStore(){if(pixels_)free(pixels_);}
  bool begin() {
    using namespace conference_store_detail;
    if(ready_)return true;
    const esp_partition_t *partition=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_FAT,"ffat");
    if(!partition||partition->address!=0x610000||partition->size!=0x9E0000)return fail("Unexpected storage partition");
    // Never format an existing or unrecognized filesystem, and never inspect
    // authenticated records or NVS. New blank units require explicit provisioning.
    if(!LittleFS.begin(false,CONFERENCE_STORE_ROOT,4,"ffat"))return fail("Storage unavailable; provisioning required");
    ready_=true;
    struct stat info;
    if(stat(path(RECORD).c_str(),&info)!=0){if(errno==ENOENT){error_="";return true;}ready_=false;return fail("Could not inspect saved badge");}
    Record record;
    if(!read(RECORD,record)){error_="Saved badge is damaged; configure to replace";return true;}
    profile_=std::move(record.profile);pixels_=record.pixels;record.pixels=nullptr;error_="";revision_++;return true;
  }
  bool ready()const{return ready_;}
  // Explicit factory/batch provisioning only. A caller must obtain the exact
  // destructive confirmation before invoking this method. If the existing
  // filesystem mounts, it is preserved. Otherwise this can ERASE ALL FILES in
  // the exact 0x610000 / 0x9E0000 ffat partition, including legacy cached photos.
  // It never touches NVS, application partitions, or security fuses. Normal
  // begin(), setup entry, image upload and save never invoke this method.
  bool initializeForConference(){
    if(ready_)return true;
    const esp_partition_t *partition=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_FAT,"ffat");
    if(!partition||partition->address!=0x610000||partition->size!=0x9E0000)return fail("Unexpected storage partition");
    if(begin())return true;
    if(!LittleFS.begin(true,CONFERENCE_STORE_ROOT,4,"ffat"))return fail("Explicit conference storage initialization failed");
    return begin();
  }
  const String &error()const{return error_;}
  const ConferenceProfile &profile()const{return profile_;}
  const uint16_t *avatarPixels()const{return pixels_;}
  int avatarWidth()const{return pixels_?160:0;}
  int avatarHeight()const{return pixels_?160:0;}
  uint32_t revision()const{return revision_;}
  // replaceImage=false retains the committed portrait. true with null/0 removes it.
  bool save(const ConferenceProfile &profile,const uint8_t *jpeg=nullptr,size_t jpegBytes=0,bool replaceImage=false) {
    using namespace conference_store_detail;
    if(!ready_)return fail("Storage unavailable");
    if(!conferenceProfileValid(profile))return fail("Check the name and social accounts");
    if((jpeg&&!jpegBytes)||(!jpeg&&jpegBytes)||(!replaceImage&&jpegBytes))return fail("Invalid image request");
    Record next;next.profile=profile;
    if(next.profile.name!=profile.name)return fail("Not enough memory");
    for(unsigned i=0;i<3;i++)if(next.profile.urls[i]!=profile.urls[i])return fail("Not enough memory");
    if((replaceImage&&jpegBytes)||(!replaceImage&&pixels_)){
      next.pixels=static_cast<uint16_t*>(ps_malloc(AVATAR_BYTES));if(!next.pixels)return fail("Not enough image memory");
      if(!replaceImage)memcpy(next.pixels,pixels_,AVATAR_BYTES);
      else {
        int width=0,height=0;uint8_t *rgb=badgeDecodeAvatarJpeg(jpeg,jpegBytes,width,height);
        if(!rgb)return fail("Use a JPEG up to 512 by 512 and 128 KiB");
        int side=width<height?width:height,left=(width-side)/2,top=(height-side)/2;
        for(unsigned y=0;y<AVATAR_SIDE;y++)for(unsigned x=0;x<AVATAR_SIDE;x++){
          const uint8_t *p=rgb+((top+y*side/AVATAR_SIDE)*width+left+x*side/AVATAR_SIDE)*3;
          next.pixels[y*AVATAR_SIDE+x]=uint16_t((p[0]&0xf8)<<8)|uint16_t((p[1]&0xfc)<<3)|(p[2]>>3);
        }
        badgeFreeAvatarPixels(rgb);
      }
    }
    uint8_t header[HEADER]={},metadata[METADATA_MAX];size_t length=encode(next.profile,metadata);if(!length)return fail("Badge too large");
    memcpy(header,MAGIC,8);put16(header+8,1);put16(header+10,HEADER);put16(header+12,AVATAR_SIDE);put16(header+14,AVATAR_SIDE);put32(header+16,length);put32(header+20,next.pixels?AVATAR_BYTES:0);
    SHA256Builder hash;hash.begin();hash.add(header,32);hash.add(metadata,length);if(next.pixels)hash.add(reinterpret_cast<uint8_t*>(next.pixels),AVATAR_BYTES);hash.calculate();hash.getBytes(header+32);
    RecordFile file(path(TEMP),"wb");if(!file.value)return fail("Could not open badge storage");
    bool written=fwrite(header,1,HEADER,file.value)==HEADER&&fwrite(metadata,1,length,file.value)==length;
    if(written&&next.pixels)written=fwrite(next.pixels,1,AVATAR_BYTES,file.value)==AVATAR_BYTES;
    if(written)written=!ferror(file.value)&&fflush(file.value)==0&&fsync(fileno(file.value))==0;
    bool closed=file.close();Record verified;
    if(!written||!closed||!read(TEMP,verified)||memcmp(verified.digest,header+32,32)){unlink(path(TEMP).c_str());return fail("Badge save failed; previous badge kept");}
    if(!LittleFS.rename(TEMP,RECORD)){unlink(path(TEMP).c_str());return fail("Badge commit failed; previous badge kept");}
    // No allocation after the atomic commit. RAM and next portal prefill now
    // describe the exact committed record, including explicit image removal.
    profile_=std::move(next.profile);if(pixels_)free(pixels_);pixels_=next.pixels;next.pixels=nullptr;revision_++;error_="";return true;
  }
  bool clear(){return save(ConferenceProfile{},nullptr,0,true);}
};
