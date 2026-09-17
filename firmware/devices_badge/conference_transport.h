#pragma once
#include <WiFi.h>
#include <NetworkServer.h>
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <functional>
#include <utility>
#include "conference_http_gate.h"

// A single HTTP request per connection. Every tick performs at most one
// nonblocking socket operation, moving no more than IO_BUDGET bytes. In
// particular, no Arduino Stream readBytes/readString or NetworkClient::write
// can wait for a trickling sender or a phone that stopped reading.
class ConferenceTransport {
public:
  static constexpr size_t IO_BUDGET=2048;
  static constexpr uint32_t HEADER_TIMEOUT_MS=5000,REQUEST_TIMEOUT_MS=10000,
    IDLE_TIMEOUT_MS=1500,RESPONSE_TIMEOUT_MS=5000;
  // Counters and the last closed request's transport envelope only. Never
  // retain a path, header value, nonce, payload, address, or profile field.
  struct Diagnostics {
    uint32_t accepts=0,closes=0,lastAgeMs=0;
    const char *lastClose="none";
    int lastErrno=0;
    uint8_t lastMethod=0,lastPhase=0; // method0=unknown,1=GET,2=POST; phase1=headers,2=body,3=response
    uint16_t lastStatus=0;
    size_t lastHeaderBytes=0,lastBodyBytes=0,lastExpectedBody=0;
    bool active=false;
    uint8_t currentMethod=0,currentPhase=0;
    uint16_t currentStatus=0;
    uint32_t currentAgeMs=0,currentIdleMs=0;
    size_t currentHeaderBytes=0,currentBodyBytes=0,currentExpectedBody=0;
    uint32_t postAttempts=0,postCloses=0,lastPostAgeMs=0;
    const char *lastPostClose="none";
    int lastPostErrno=0;
    uint8_t lastPostPhase=0;
    uint16_t lastPostStatus=0;
    size_t lastPostHeaderBytes=0,lastPostBodyBytes=0,lastPostExpectedBody=0;
  };
  using Callback=std::function<void()>;
private:
  enum class Phase:uint8_t {Idle,Headers,Body,Response};
  NetworkServer server_{80,2};NetworkClient client_;
  Callback headersReady_,bodyReady_;
  Phase phase_=Phase::Idle;
  char header_[conference_http_gate::HEADER_LIMIT+1]={};size_t headerLength_=0;
  conference_http_gate::Request request_;
  uint8_t *body_=nullptr;size_t bodyLength_=0;
  String responseHeaders_,responseBody_;size_t headerSent_=0,bodySent_=0;
  uint32_t started_=0,progress_=0,responseStarted_=0;
  mutable Diagnostics diagnostics_;
  size_t receivedBody_=0;uint16_t responseStatus_=0;uint8_t method_=0;

  static bool expired(uint32_t now,uint32_t start,uint32_t limit){return uint32_t(now-start)>=limit;}
  void releaseBody(){if(body_)free(body_);body_=nullptr;bodyLength_=0;}
  void closeClient(const char *reason,int error=0){
    if(client_.fd()>=0||phase_!=Phase::Idle){
      diagnostics_.closes++;diagnostics_.lastClose=reason;diagnostics_.lastErrno=error;
      diagnostics_.lastAgeMs=millis()-started_;diagnostics_.lastMethod=method_;
      diagnostics_.lastPhase=uint8_t(phase_);diagnostics_.lastStatus=responseStatus_;
      diagnostics_.lastHeaderBytes=headerLength_;diagnostics_.lastBodyBytes=receivedBody_;
      diagnostics_.lastExpectedBody=request_.bodyLength;
      if(method_==2){
        diagnostics_.postCloses++;diagnostics_.lastPostClose=reason;diagnostics_.lastPostErrno=error;
        diagnostics_.lastPostAgeMs=diagnostics_.lastAgeMs;diagnostics_.lastPostPhase=uint8_t(phase_);
        diagnostics_.lastPostStatus=responseStatus_;diagnostics_.lastPostHeaderBytes=headerLength_;
        diagnostics_.lastPostBodyBytes=receivedBody_;diagnostics_.lastPostExpectedBody=request_.bodyLength;
      }
    }
    client_.stop();client_=NetworkClient();releaseBody();phase_=Phase::Idle;
    headerLength_=headerSent_=bodySent_=0;request_=conference_http_gate::Request{};
    responseHeaders_="";responseBody_="";
  }
  void sendResponse(){
    bool headers=headerSent_<responseHeaders_.length();
    size_t remaining=headers?responseHeaders_.length()-headerSent_:responseBody_.length()-bodySent_;
    if(!remaining){closeClient("response_complete");return;}
    size_t count=remaining<IO_BUDGET?remaining:IO_BUDGET;
    const char *bytes=headers?responseHeaders_.c_str()+headerSent_:responseBody_.c_str()+bodySent_;
    int sent=::send(client_.fd(),bytes,count,MSG_DONTWAIT);
    if(sent<0){int error=errno;if(error!=EAGAIN&&error!=EWOULDBLOCK&&error!=EINTR)closeClient("send_error",error);return;}
    if(!sent){closeClient("send_zero");return;}
    if(headers)headerSent_+=size_t(sent);else bodySent_+=size_t(sent);progress_=millis();
    if(headerSent_==responseHeaders_.length()&&bodySent_==responseBody_.length())closeClient("response_complete");
  }
public:
  ConferenceTransport(Callback headersReady,Callback bodyReady):headersReady_(std::move(headersReady)),bodyReady_(std::move(bodyReady)){}
  ~ConferenceTransport(){stop();}
  ConferenceTransport(const ConferenceTransport&)=delete;
  ConferenceTransport&operator=(const ConferenceTransport&)=delete;
  bool begin(){server_.begin();if(server_)return true;server_.stop();return false;}
  void stop(){closeClient("stopped");server_.stop();}
  const Diagnostics &diagnostics()const{
    diagnostics_.active=phase_!=Phase::Idle;
    diagnostics_.currentMethod=diagnostics_.active?method_:0;
    diagnostics_.currentPhase=uint8_t(phase_);diagnostics_.currentStatus=diagnostics_.active?responseStatus_:0;
    diagnostics_.currentAgeMs=diagnostics_.active?millis()-started_:0;
    diagnostics_.currentIdleMs=diagnostics_.active?millis()-progress_:0;
    diagnostics_.currentHeaderBytes=headerLength_;diagnostics_.currentBodyBytes=diagnostics_.active?receivedBody_:0;
    diagnostics_.currentExpectedBody=request_.bodyLength;return diagnostics_;
  }
  bool idle()const{return phase_==Phase::Idle;}
  const conference_http_gate::Request &request()const{return request_;}
  const uint8_t *body()const{return body_;}
  size_t bodyLength()const{return bodyLength_;}
  uint8_t *takeBody(){uint8_t *result=body_;body_=nullptr;bodyLength_=0;return result;}
  bool readBody(){
    if(phase_!=Phase::Headers||!request_.post||!request_.bodyLength)return false;
    body_=static_cast<uint8_t*>(ps_malloc(request_.bodyLength+1));
    if(!body_)return false;bodyLength_=0;phase_=Phase::Body;return true;
  }
  void reply(int code,const char *type,const String &body,bool redirect=false){
    releaseBody();responseBody_=body;headerSent_=bodySent_=0;responseStatus_=code;
    char first[180];snprintf(first,sizeof(first),"HTTP/1.1 %d Response\r\nContent-Type: %s\r\nContent-Length: %u\r\n",code,type,unsigned(responseBody_.length()));
    responseHeaders_=String(first)+"Connection: close\r\nCache-Control: no-store\r\nReferrer-Policy: no-referrer\r\nX-Content-Type-Options: nosniff\r\nContent-Security-Policy: default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; img-src blob:; form-action 'self'; frame-ancestors 'none'\r\n";
    if(redirect)responseHeaders_+="Location: http://192.168.4.1/\r\n";
    responseHeaders_+="\r\n";phase_=Phase::Response;responseStarted_=progress_=millis();
  }
  void tick(){
    if(phase_==Phase::Idle){
      client_=server_.accept();if(client_.fd()<0)return;
      diagnostics_.accepts++;started_=progress_=millis();receivedBody_=0;responseStatus_=0;method_=0;
      if(!(client_.localIP()==WiFi.softAPIP())){closeClient("nonlocal");return;}
      int flags=fcntl(client_.fd(),F_GETFL,0);
      if(flags<0||fcntl(client_.fd(),F_SETFL,flags|O_NONBLOCK)<0){closeClient("nonblocking_failed",errno);return;}
      phase_=Phase::Headers;headerLength_=0;
    }
    uint32_t now=millis();
    if(phase_==Phase::Response){
      if(expired(now,responseStarted_,RESPONSE_TIMEOUT_MS)){closeClient("response_timeout");return;}
      if(expired(now,progress_,IDLE_TIMEOUT_MS)){closeClient("response_idle_timeout");return;}
      sendResponse();return;
    }
    if(expired(now,started_,REQUEST_TIMEOUT_MS)){closeClient("request_timeout");return;}
    if(phase_==Phase::Headers&&expired(now,started_,HEADER_TIMEOUT_MS)){closeClient("header_timeout");return;}
    if(expired(now,progress_,IDLE_TIMEOUT_MS)){closeClient("request_idle_timeout");return;}
    uint8_t incoming[IO_BUDGET];int count=::recv(client_.fd(),incoming,sizeof(incoming),MSG_DONTWAIT);
    if(count<0){int error=errno;if(error!=EAGAIN&&error!=EWOULDBLOCK&&error!=EINTR)closeClient("recv_error",error);return;}
    if(!count){closeClient("peer_eof");return;}progress_=now;
    size_t offset=0;
    while(offset<size_t(count)){
      if(phase_==Phase::Headers){
        if(headerLength_==conference_http_gate::HEADER_LIMIT){reply(431,"application/json","{\"message\":\"Request headers too large.\"}");return;}
        header_[headerLength_++]=incoming[offset++];
        // Recognize only the method prefix for diagnostic classification. The
        // complete header gate still owns all request validation/authorization.
        if(!method_&&headerLength_>=4&&!memcmp(header_,"GET ",4))method_=1;
        if(!method_&&headerLength_>=5&&!memcmp(header_,"POST ",5)){method_=2;diagnostics_.postAttempts++;}
        if(headerLength_>=4&&!memcmp(header_+headerLength_-4,"\r\n\r\n",4)){
          header_[headerLength_]=0;int rejection=conference_http_gate::inspect(header_,headerLength_,&request_);
          if(rejection){reply(rejection,"application/json","{\"message\":\"Request rejected. Reload setup.\"}");return;}
          method_=request_.post?2:1;
          headersReady_();
          // The callback must either authorize a bounded body or queue a reply.
          if(phase_==Phase::Headers){closeClient("handler_no_action");return;}
        }
      }else if(phase_==Phase::Body){
        size_t remaining=request_.bodyLength-bodyLength_;size_t available=size_t(count)-offset;
        size_t copied=remaining<available?remaining:available;
        memcpy(body_+bodyLength_,incoming+offset,copied);bodyLength_+=copied;receivedBody_=bodyLength_;offset+=copied;
        if(bodyLength_==request_.bodyLength){body_[bodyLength_]=0;bodyReady_();return;}
      }else return; // No pipelining; bytes after this request are discarded.
    }
  }
};
