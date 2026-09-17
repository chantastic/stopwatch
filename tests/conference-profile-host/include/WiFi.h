#pragma once
#include <Arduino.h>
struct IPAddress {
  uint32_t value;
  IPAddress(uint8_t a=0,uint8_t b=0,uint8_t c=0,uint8_t d=0):value((a<<24)|(b<<16)|(c<<8)|d){}
  bool operator==(const IPAddress &other)const{return value==other.value;}
};
static constexpr int WIFI_OFF=0,WIFI_AP=2;
struct FakeWiFi {
  int modeValue=WIFI_OFF;bool started=false,failStart=false,autoReconnect=true,persist=true;
  String ssid,password;
  void persistent(bool enabled){persist=enabled;}
  void setAutoReconnect(bool enabled){autoReconnect=enabled;}
  void disconnect(bool,bool){}
  bool mode(int value){modeValue=value;return true;}
  bool softAPConfig(IPAddress,IPAddress,IPAddress){return true;}
  bool softAP(const char *s,const char *p,int,int,int){if(failStart)return false;ssid=s;password=p;started=true;return true;}
  IPAddress softAPIP(){return IPAddress(192,168,4,1);}
  bool softAPdisconnect(bool){started=false;return true;}
};
inline FakeWiFi WiFi;
