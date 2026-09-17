#pragma once
#include <Arduino.h>

// Manual conference data is intentionally independent of authenticated caches.
enum class ConferenceNetwork : uint8_t { GitHub=0, X=1, LinkedIn=2 };
struct ConferenceProfile { String name; String urls[3]; };

static inline bool conferenceAlnum(char c) {
  return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9');
}
static inline String conferenceTrim(const String &s) {
  size_t begin=0,end=s.length();
  while(begin<end && s[begin]==' ')++begin;
  while(end>begin && s[end-1]==' ')--end;
  return s.substring(begin,end);
}
static inline bool conferenceName(const String &input,String &name) {
  if(input.length()>160)return false;
  String s=conferenceTrim(input);if(s.length()>120)return false;
  unsigned count=0;
  for(size_t i=0;i<s.length();) {
    uint8_t c=s[i++];uint32_t cp;unsigned extra;
    if(c<128){cp=c;extra=0;}
    else if(c>=0xc2&&c<=0xdf){cp=c&31;extra=1;}
    else if(c>=0xe0&&c<=0xef){cp=c&15;extra=2;}
    else if(c>=0xf0&&c<=0xf4){cp=c&7;extra=3;}
    else return false;
    if(i+extra>s.length())return false;
    for(unsigned n=0;n<extra;n++){uint8_t next=s[i++];if((next&0xc0)!=0x80)return false;cp=(cp<<6)|(next&63);}
    if((extra==1&&cp<128)||(extra==2&&cp<2048)||(extra==3&&cp<65536)||
       cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff)||cp<32||(cp>=127&&cp<=159)||
       cp==0x2028||cp==0x2029||(cp>=0x202a&&cp<=0x202e)||(cp>=0x2066&&cp<=0x2069))return false;
    if(++count>60)return false;
  }
  name=s;return true;
}
static inline bool conferenceSocialUrl(ConferenceNetwork network,const String &input,String &url) {
  url="";if(uint8_t(network)>2||input.length()>180)return false;
  String value=conferenceTrim(input);if(value.isEmpty())return true;
  String handle=value;
  const char *prefixes[4]={};unsigned prefixCount=0;
  if(network==ConferenceNetwork::GitHub){prefixes[0]="https://github.com/";prefixes[1]="https://www.github.com/";prefixCount=2;}
  if(network==ConferenceNetwork::X){prefixes[0]="https://x.com/";prefixes[1]="https://twitter.com/";prefixes[2]="https://www.x.com/";prefixes[3]="https://www.twitter.com/";prefixCount=4;}
  if(network==ConferenceNetwork::LinkedIn){prefixes[0]="https://www.linkedin.com/in/";prefixes[1]="https://linkedin.com/in/";prefixCount=2;}
  bool fullUrl=false;
  for(unsigned i=0;i<prefixCount;i++)if(value.startsWith(prefixes[i])){handle=value.substring(strlen(prefixes[i]));fullUrl=true;break;}
  if(fullUrl&&handle.endsWith("/"))handle=handle.substring(0,handle.length()-1);
  if(!fullUrl&&handle.startsWith("@"))handle=handle.substring(1);
  const size_t maximum=network==ConferenceNetwork::GitHub?39:network==ConferenceNetwork::X?15:100;
  if(handle.isEmpty()||handle.length()>maximum)return false;
  for(size_t i=0;i<handle.length();i++) {
    char c=handle[i];
    if(!conferenceAlnum(c)&&!(c=='_'&&network!=ConferenceNetwork::GitHub)&&!(c=='-'&&network!=ConferenceNetwork::X))return false;
    if(network==ConferenceNetwork::GitHub&&c=='-'&&(i==0||i+1==handle.length()||handle[i-1]=='-'))return false;
  }
  if(network==ConferenceNetwork::GitHub)url="https://github.com/"+handle;
  if(network==ConferenceNetwork::X)url="https://x.com/"+handle;
  if(network==ConferenceNetwork::LinkedIn)url="https://www.linkedin.com/in/"+handle+"/";
  return true;
}
static inline bool conferenceProfileValid(const ConferenceProfile &profile) {
  String result;
  if(!conferenceName(profile.name,result)||result!=profile.name)return false;
  for(unsigned i=0;i<3;i++)if(!conferenceSocialUrl(ConferenceNetwork(i),profile.urls[i],result)||result!=profile.urls[i])return false;
  return true;
}
