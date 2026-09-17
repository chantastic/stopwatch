#pragma once
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
class String {
  std::string value;
public:
  String()=default;
  String(const char *s):value(s?s:""){}
  String(const char *s,unsigned int n):value(s,n){}
  String(const std::string &s):value(s){}
  size_t length()const{return value.size();}
  bool isEmpty()const{return value.empty();}
  const char *c_str()const{return value.c_str();}
  char operator[](size_t i)const{return value[i];}
  bool startsWith(const String &s)const{return value.rfind(s.value,0)==0;}
  bool endsWith(const String &s)const{return value.size()>=s.value.size()&&value.compare(value.size()-s.value.size(),s.value.size(),s.value)==0;}
  String substring(size_t from)const{return value.substr(from);}
  String substring(size_t from,size_t to)const{return value.substr(from,to-from);}
  bool concat(const char *s){value+=s?s:"";return true;}
  bool concat(const char *s,size_t n){value.append(s,n);return true;}
  String& operator+=(const String &s){value+=s.value;return *this;}
  void replace(const char *from,const char *to){size_t pos=0;while((pos=value.find(from,pos))!=std::string::npos){value.replace(pos,strlen(from),to);pos+=strlen(to);}}
  friend String operator+(const String &a,const String &b){return a.value+b.value;}
  friend bool operator==(const String &a,const String &b){return a.value==b.value;}
  friend bool operator!=(const String &a,const String &b){return !(a==b);}
};
