#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

// Validate the complete, bounded HTTP header before allocating a body.
// No heap, Arduino, socket, or secret access.
namespace conference_http_gate {
static constexpr size_t HEADER_LIMIT=4096;
struct Request {
  bool post=false;
  char path[385]={};
  char nonce[33]={};
  size_t bodyLength=0;
};
static inline const char *lineEnd(const char *begin,const char *end){for(const char *p=begin;p+1<end;p++)if(p[0]=='\r'&&p[1]=='\n')return p;return nullptr;}
static inline bool equal(const char *begin,size_t length,const char *text){return length==strlen(text)&&memcmp(begin,text,length)==0;}
static inline bool headerName(const char *begin,size_t length,const char *text){return length==strlen(text)&&strncasecmp(begin,text,length)==0;}
static inline int inspect(const char *bytes,size_t length,Request *parsed=nullptr) {
  if(parsed)*parsed=Request{};
  if(length>HEADER_LIMIT)return 431;
  if(length<4||memcmp(bytes+length-4,"\r\n\r\n",4))return 400;
  const char *end=bytes+length,*first=lineEnd(bytes,end);if(!first||first-bytes>512)return 400;
  const char *space=static_cast<const char*>(memchr(bytes,' ',first-bytes));if(!space)return 400;
  bool get=equal(bytes,space-bytes,"GET"),post=equal(bytes,space-bytes,"POST");if(!get&&!post)return 405;
  const char *url=space+1,*last=static_cast<const char*>(memchr(url,' ',first-url));if(!last||last==url||last-url>384||url[0]!='/')return 400;
  if(!equal(last+1,first-last-1,"HTTP/1.1")&&!equal(last+1,first-last-1,"HTTP/1.0"))return 400;
  for(const char *p=url;p<last;p++)if(uint8_t(*p)<=32||uint8_t(*p)>=127)return 400;
  bool image=equal(url,last-url,"/image"),save=equal(url,last-url,"/save"),cancel=equal(url,last-url,"/cancel");
  if(post&&!image&&!save&&!cancel)return 404;
  Request result;result.post=post;memcpy(result.path,url,last-url);
  bool seenLength=false,seenType=false,seenNonce=false,seenHost=false;size_t bodyLength=0;bool correctType=false;
  const char *line=first+2;
  while(line<end){
    const char *finish=lineEnd(line,end);if(!finish)return 400;
    if(finish==line){if(finish+2!=end)return 400;break;}
    const char *colon=static_cast<const char*>(memchr(line,':',finish-line));if(!colon||colon==line)return 400;
    for(const char *p=line;p<colon;p++)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='-'))return 400;
    const char *value=colon+1,*tail=finish;while(value<tail&&(*value==' '||*value=='\t'))value++;while(tail>value&&(tail[-1]==' '||tail[-1]=='\t'))tail--;
    for(const char *p=value;p<tail;p++)if((uint8_t(*p)<32&&*p!='\t')||uint8_t(*p)==127)return 400;
    size_t nameLength=colon-line;
    if(headerName(line,nameLength,"Transfer-Encoding"))return 400;
    if(headerName(line,nameLength,"Expect"))return 417;
    if(headerName(line,nameLength,"Content-Length")){
      if(seenLength||value==tail)return 400;seenLength=true;
      for(const char *p=value;p<tail;p++){if(*p<'0'||*p>'9')return 400;bodyLength=bodyLength*10+(*p-'0');if(bodyLength>128*1024)return 413;}
    }
    if(headerName(line,nameLength,"Content-Type")){
      if(seenType)return 400;seenType=true;
      correctType=equal(value,tail-value,image?"image/jpeg":"application/json");
    }
    if(headerName(line,nameLength,"X-Conference-Nonce")){
      if(seenNonce)return 400;seenNonce=true;
      if(tail-value!=32)return 403;
      for(const char *p=value;p<tail;p++)if(!((*p>='0'&&*p<='9')||(*p>='a'&&*p<='f')))return 403;
      memcpy(result.nonce,value,32);
    }
    if(headerName(line,nameLength,"Host")){if(seenHost)return 400;seenHost=true;}
    line=finish+2;
  }
  if(get){if(bodyLength)return 413;}
  else {
    if(!seenLength||!bodyLength)return 411;
    if(bodyLength>(image?128*1024:2048))return 413;
    if(!seenType||!correctType)return 415;
  }
  result.bodyLength=bodyLength;if(parsed)*parsed=result;
  return 0;
}
} // namespace conference_http_gate
