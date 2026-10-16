/* R8 local UDP protocol smoke test (no real Xbox/server contact). */
#define _POSIX_C_SOURCE 200809L
#include <time.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "SDL.h"
static struct timespec started;
uint32_t SDL_GetTicks(void){struct timespec x;clock_gettime(CLOCK_MONOTONIC,&x);return (uint32_t)((x.tv_sec-started.tv_sec)*1000+(x.tv_nsec-started.tv_nsec)/1000000);}
void SDL_Delay(uint32_t ms){struct timespec t={(time_t)(ms/1000), (long)(ms%1000)*1000000};nanosleep(&t,NULL);}
void SDL_Log(const char* fmt,...){(void)fmt;}
void SDL_LogError(int c,const char* fmt,...){(void)c;va_list v;va_start(v,fmt);vfprintf(stderr,fmt,v);va_end(v);fputc('\n',stderr);}
#define ZAMN_ANDROID_R8_NET_TEST 1
#include "../../android_port/app/src/main/cpp/zamn_android_net.c"
/* Host handshake should not need to advance emulated frames. */
bool zamn_runtime_reset_to_title(ZamnNativeRuntime* r,bool start){(void)r;(void)start;return true;}
int zamn_runtime_snapshot_size(ZamnNativeRuntime* r){(void)r;return 32;}
bool zamn_runtime_save_snapshot(ZamnNativeRuntime* r,uint8_t *buf,int cap,int *size){(void)r;if(cap<32)return false;memset(buf,0,32);*size=32;return true;}
bool zamn_runtime_load_snapshot(ZamnNativeRuntime*r,const uint8_t* b,int len){(void)r;(void)b;return len==32;}
void zamn_runtime_set_replay_suppressed(ZamnNativeRuntime* r,bool f){(void)r;(void)f;}
void zamn_runtime_set_pad(ZamnNativeRuntime* r,int player,uint16_t bits){(void)r;(void)player;(void)bits;}
uint64_t zamn_runtime_state_hash(const ZamnNativeRuntime* r){(void)r;return 0x2222222222222222ull;}
bool zamn_runtime_frame(ZamnNativeRuntime* r,uint8_t*b,int pitch,ZamnFrameResult*out){(void)r;(void)b;(void)pitch;(void)out;return true;}
static void packet_to_host(int fd,const struct sockaddr_in*host,uint8_t type,uint32_t session,const uint8_t*pay,int len){
  uint8_t pkt[128]={0};memcpy(pkt,"ZNP1",4);pkt[4]=4;pkt[5]=type;
  wr16(pkt+6,(uint16_t)(24+len));wr32(pkt+8,ZNP_BUILD);wr32(pkt+12,session);
  if(len)memcpy(pkt+24,pay,(size_t)len);
  sendto(fd,pkt,(size_t)(24+len),0,(const struct sockaddr*)host,sizeof(*host));
}
static int guest(int mobile,int wanted){
  int fd=socket(AF_INET,SOCK_DGRAM,0);if(fd<0)return 111;
  struct sockaddr_in host;endpoint(&host,"127.0.0.1",6464);
  uint32_t session=0,last=0;int ready=0;
  uint8_t intro[32]={0};wr64(intro,0x123456789abcdef0ull);intro[8]=4;
  memcpy(intro+9,"TEST",4);if(mobile)memcpy(intro+13,"ZA8M",4);
  for(uint32_t now=0;now<12000;now=SDL_GetTicks()){
    if(!ready && now-last>=90){packet_to_host(fd,&host,HELLO,0,intro,mobile?17:13);last=now;}
    struct timeval tv={0,10000};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
    uint8_t msg[256];struct sockaddr_in addr;socklen_t alen=sizeof(addr);
    int n=(int)recvfrom(fd,msg,sizeof(msg),0,(struct sockaddr*)&addr,&alen);
    if(n>=24 && !memcmp(msg,"ZNP1",4) && rd32(msg+8)==ZNP_BUILD){
      int type=msg[5],size=n-24;session=rd32(msg+12);
      if(type==OFFER && size>=18){ready=1;packet_to_host(fd,&host,READY,session,NULL,0);}
      if(type==PING && size>=8)packet_to_host(fd,&host,PONG,session,msg+24,8);
      if(type==START && size>=7){
        packet_to_host(fd,&host,START_ACK,session,NULL,0);
        packet_to_host(fd,&host,BOOT_READY,session,NULL,0);
      }
      if(type==BOOT_GO && size>=4){close(fd);return (msg[26]==wanted && msg[25]==(wanted!=0))?0:3;}
    }
  }
  close(fd);return 112;
}
int main(int argc,char**argv){
  // R13: AUTO starts at 1F input delay AND 1F rollback, even on fast LAN.
  memset(&g,0,sizeof(g));g.host_delay_choice=-1;g.host_rb_choice=-1;
  uint8_t auto_d=0,auto_w=0;
  host_pair(20,0,&auto_d,&auto_w);
  if(auto_d!=1||auto_w!=1){fprintf(stderr,"AUTO should enable 1F RB at 1F input delay\n");return 11;}
  g.host_delay_choice=1;g.host_rb_choice=1;
  host_pair(20,0,&auto_d,&auto_w);
  if(auto_d!=1||auto_w!=1){fprintf(stderr,"MANUAL RB=1 must work at manual 1F delay\n");return 15;}
  g.host_delay_choice=-1;g.host_rb_choice=-1;
  /* R13 lobby selector: AUTO -> OFF -> 1 -> 2/3 (mobile only). */
  g.have_peer=0;
  if(!host_apply_lobby_actions(16)||g.host_rb_choice!=0)return 17;
  if(!host_apply_lobby_actions(16)||g.host_rb_choice!=1)return 18;
  if(!host_apply_lobby_actions(16)||g.host_rb_choice!=2)return 19;
  host_apply_lobby_actions(32);
  if(g.host_rb_choice!=-1||g.host_delay_choice!=-1)return 20;
  g.have_peer=1;g.peer_mobile=0;
  host_apply_lobby_actions(16);host_apply_lobby_actions(16);
  if(g.host_rb_choice!=1)return 21;
  host_apply_lobby_actions(16);
  if(g.host_rb_choice!=-1)return 22;
  g.have_peer=0;
  host_pair(50,0,&auto_d,&auto_w);
  if(auto_d!=1||auto_w!=1){fprintf(stderr,"AUTO 2F coverage should select 1F delay+1F RB\n");return 12;}
  g.public_mode=1;g.peer_mobile=1;
  host_pair(65,0,&auto_d,&auto_w);
  if(auto_w!=2){fprintf(stderr,"AUTO mobile moderate RTT should select 2F RB\n");return 16;}
  host_pair(200,18,&auto_d,&auto_w);
  if(auto_w!=3){fprintf(stderr,"AUTO mobile high RTT should select 3F RB\n");return 13;}
  g.peer_mobile=0;host_pair(200,18,&auto_d,&auto_w);
  if(auto_w!=1){fprintf(stderr,"AUTO Xbox must remain within 1F RB\n");return 14;}
  memset(&g,0,sizeof(g));
  clock_gettime(CLOCK_MONOTONIC,&started);
  int mobile=argc>1 && !strcmp(argv[1],"mobile");
  ZanConfig config={0};strcpy(config.mode,"host_direct");strcpy(config.username,"ANDROID");
  int auto_run=argc>2 && !strcmp(argv[2],"auto");
  config.delay_choice=auto_run?-1:2;
  config.rollback_choice=auto_run?-1:3;
  int expected=auto_run?1:(mobile?3:1);
  pid_t child=fork();if(child<0)return 2;
  if(child==0){SDL_Delay(65);_exit(guest(mobile,expected));}
  bool ok=zan_connect(&config,0x123456789abcdef0ull,(ZamnNativeRuntime*)0x1234);
  int result=0;waitpid(child,&result,0);
  int passed=ok&&WIFEXITED(result)&&WEXITSTATUS(result)==0 && g.core.local_slot==0 &&
             g.rollback_window==expected;
  zan_close();if(!passed){fprintf(stderr,"FAIL mobile=%d host=%d child=%d slot=%u rb=%u\n",mobile,ok,result,g.core.local_slot,g.rollback_window);return 1;}
  printf("PASS simulated %s -> Android host: ZNP1v4, P1 slot, %dF RB (%s)\n",mobile?"Android":"Xbox",expected,auto_run?"AUTO":"MANUAL");
  return 0;
}
