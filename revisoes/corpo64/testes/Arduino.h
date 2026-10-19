#pragma once
#include <cstdint>
#include <cstddef>
#include <deque>
#include <string>
#include <cstdio>
using std::size_t;
#define CONFIG_IDF_TARGET_ESP32 1
#define SERIAL_8N1 0
extern int64_t fakeClock;
struct Port {
  std::deque<uint8_t> q; std::string log; bool enabled=true;
  void begin(int,int=0,int=0,int=0){enabled=true;}
  void end(){enabled=false;q.clear();}
  void setRxBufferSize(int){} void setRxFIFOFull(int){}
  int available(){return enabled?int(q.size()):0;}
  int read(){if(q.empty()||!enabled)return -1;auto c=q.front();q.pop_front();return c;}
  template<class... A> void printf(const char *fmt,A... a){char b[4096];std::snprintf(b,sizeof(b),fmt,a...);log+=b;}
  void println(const char *s=""){log+=s;log+='\n';}
  void print(const char *s){log+=s;} void print(char c){log+=c;}
};
extern Port Serial,Serial2;
inline void delay(int n){fakeClock+=1000*n;}
inline void setCpuFrequencyMhz(int){}
