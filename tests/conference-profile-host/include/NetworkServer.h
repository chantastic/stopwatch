#pragma once
#include <WiFi.h>
#include <sys/socket.h>
#include <unistd.h>
#include <memory>
#include <deque>
#include <cassert>
#include <fcntl.h>
// Only the listening/accepted-client adapter is simulated. Production code
// performs actual nonblocking recv/send calls against these socketpairs.
struct TestSocket {
  int descriptor=-1;bool local=true;
  ~TestSocket(){close();}
  void close(){if(descriptor>=0)::close(descriptor);descriptor=-1;}
};
class NetworkClient {
  std::shared_ptr<TestSocket> socket_;
public:
  NetworkClient()=default;
  explicit NetworkClient(std::shared_ptr<TestSocket> socket):socket_(socket){}
  int fd()const{return socket_?socket_->descriptor:-1;}
  void stop(){if(socket_)socket_->close();socket_.reset();}
  IPAddress localIP(){return socket_&&socket_->local?IPAddress(192,168,4,1):IPAddress(10,0,0,1);}
};
class NetworkServer {
  std::deque<NetworkClient> pending_;
  bool listening_=false;
public:
  inline static NetworkServer *last=nullptr;
  inline static bool failBegin=false;
  inline static unsigned stopCalls=0;
  explicit NetworkServer(uint16_t,uint8_t){last=this;}
  void begin(){listening_=!failBegin;}
  void stop(){stopCalls++;listening_=false;for(auto &client:pending_)client.stop();pending_.clear();}
  operator bool()const{return listening_;}
  NetworkClient accept(){if(!listening_||pending_.empty())return {};auto client=pending_.front();pending_.pop_front();return client;}
  std::shared_ptr<TestSocket> connect(int &peer,bool local=true){
    assert(listening_);int pair[2];assert(socketpair(AF_UNIX,SOCK_STREAM,0,pair)==0);int one=1;
    assert(setsockopt(pair[0],SOL_SOCKET,SO_NOSIGPIPE,&one,sizeof(one))==0);
    assert(setsockopt(pair[1],SOL_SOCKET,SO_NOSIGPIPE,&one,sizeof(one))==0);
    assert(fcntl(pair[1],F_SETFL,O_NONBLOCK)==0);
    auto socket=std::make_shared<TestSocket>();socket->descriptor=pair[0];socket->local=local;peer=pair[1];pending_.emplace_back(socket);return socket;
  }
};
