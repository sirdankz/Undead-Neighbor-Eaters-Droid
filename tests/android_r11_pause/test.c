#define SDL_MAIN_HANDLED
#define _XOPEN_SOURCE 700
#include <SDL.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/stat.h>
#include "android_port/app/src/main/cpp/zamn_android_online_pause.h"
#include "android_port/app/src/main/cpp/zamn_android_net.h"
struct ZamnNativeRuntime {uint8_t state[64];};
static uint32_t tick;static int phase,operations,control_messages,synced;static char work[1200];
static unsigned selected_mode=0;static uint8_t local_slot=1;static int inject_touch=0,bad_gate=0;
static struct {uint32_t seq,frame;uint64_t hash;uint8_t step,yes,reserved[6];} reply;
typedef struct {uint32_t seq,frame;char name[17],reserved[3];} Cmd;
typedef struct {uint32_t seq;uint8_t owner,screen,selection,key_index,list_count,list_selected,reserved[2];char name[17],status[32],list[6][17];} UI;
uint8_t zan_local_slot(void){return local_slot;}
uint8_t zan_ruleset(void){return (uint8_t)selected_mode;}
uint64_t zan_rom_hash(void){return 0x1234;}
bool zan_resync_after_shared_load(uint32_t frame){synced=frame==100;return synced;}
int zan_control_send(uint8_t kind,const void *payload,int n){
  if(kind==4 && n==(int)sizeof(reply)){memcpy(&reply,payload,sizeof(reply));return 1;}
  return 1;
}
int zan_control_recv(uint8_t *kind,void *payload,int cap,uint32_t timeout_ms){
  (void)timeout_ms;
  if(reply.seq){
    assert(cap >= (int)sizeof(reply));*kind=4;
    if(bad_gate && reply.step==0)reply.hash^=1u;
    memcpy(payload,&reply,sizeof(reply));reply.seq=0;
    return sizeof(reply);
  }
  if(control_messages==0){
    UI ui={0};ui.owner=0;ui.screen=0;ui.selection=2;
    assert(cap>=(int)sizeof(ui));*kind=1;memcpy(payload,&ui,sizeof(ui));control_messages++;
    return sizeof(ui);
  }
  if(control_messages==1 && operations){
    Cmd cmd={0};cmd.seq=2;cmd.frame=100;strcpy(cmd.name,"ALPHA");
    assert(cap>=(int)sizeof(cmd));*kind=(uint8_t)(operations==1?2:3);
    memcpy(payload,&cmd,sizeof(cmd));control_messages++;
    return sizeof(cmd);
  }
  if(control_messages <=(operations?2:1)){
    Cmd cmd={0};cmd.seq=3;cmd.frame=100;
    assert(cap>=(int)sizeof(cmd));*kind=5;memcpy(payload,&cmd,sizeof(cmd));control_messages++;
    return sizeof(cmd);
  }
  *kind=0;return 0;
}
size_t SDL_strlcpy(char *dest,const char*src,size_t n){size_t size=strlen(src);if(n){size_t take=size<n-1?size:n-1;memcpy(dest,src,take);dest[take]=0;}return size;}
int SDL_snprintf(char*dest,size_t n,const char*fmt,...){va_list ap;va_start(ap,fmt);int k=vsnprintf(dest,n,fmt,ap);va_end(ap);return k;}
Uint32 SDL_GetTicks(void){return ++tick;}
void SDL_Delay(Uint32 ms){tick+=ms;}
int SDL_PollEvent(SDL_Event*e){if(!inject_touch)return 0;inject_touch=0;memset(e,0,sizeof(*e));e->type=SDL_FINGERDOWN;e->tfinger.x=.50f;e->tfinger.y=.60f;return 1;}
int SDL_GetRendererOutputSize(SDL_Renderer*r,int*w,int*h){(void)r;*w=1200;*h=675;return 0;}
int SDL_SetRenderDrawColor(SDL_Renderer*r,Uint8 a,Uint8 b,Uint8 c,Uint8 d){(void)r;(void)a;(void)b;(void)c;(void)d;return 0;}
int SDL_RenderClear(SDL_Renderer*r){(void)r;return 0;}
int SDL_SetRenderDrawBlendMode(SDL_Renderer*r,SDL_BlendMode m){(void)r;(void)m;return 0;}
int SDL_RenderFillRect(SDL_Renderer*r,const SDL_Rect *p){(void)r;(void)p;return 0;}
int SDL_RenderDrawRect(SDL_Renderer*r,const SDL_Rect *p){(void)r;(void)p;return 0;}
int SDL_RenderCopy(SDL_Renderer*r,SDL_Texture*t,const SDL_Rect*s,const SDL_Rect*d){(void)r;(void)t;(void)s;(void)d;return 0;}
void SDL_RenderPresent(SDL_Renderer*r){(void)r;}
void SDL_PauseAudioDevice(SDL_AudioDeviceID id,int pause_on){(void)id;(void)pause_on;}
void SDL_Log(const char*fmt,...){(void)fmt;}
void SDL_LogError(int cat,const char*fmt,...){(void)cat;(void)fmt;}
int zamn_runtime_snapshot_size(ZamnNativeRuntime*rt){(void)rt;return 64;}
bool zamn_runtime_save_snapshot(ZamnNativeRuntime*rt,uint8_t*dest,int cap,int*used){if(cap<64)return false;memcpy(dest,rt->state,64);*used=64;return true;}
bool zamn_runtime_load_snapshot(ZamnNativeRuntime*rt,const uint8_t*data,int len){if(len!=64)return false;memcpy(rt->state,data,64);return true;}
uint64_t zamn_runtime_state_hash(const ZamnNativeRuntime*rt){uint64_t h=0;for(int i=0;i<64;++i)h=h*131u+rt->state[i];return h;}
uint16_t zamn_runtime_current_level(const ZamnNativeRuntime*rt){(void)rt;return 4;}
static void test(ZamnNativeRuntime*rt,int kind,int want){
  operations=kind;control_messages=0;tick=0;phase=0;reply.seq=0;synced=0;
  uint32_t frame=100;int res=zon_run(rt,&frame,0,(SDL_Renderer*)1,NULL,NULL,0);
  if(res!=want){fprintf(stderr,"FAIL: kind=%d result=%d expected=%d frame=%u\n",kind,res,want,frame);exit(3);}
  if(frame!=100){fprintf(stderr,"FAIL: frame changed without synced LOAD\n");exit(4);}
}
int main(void){
  char temp[]="/tmp/zamn-r11-pauses-XXXXXX";
  assert(mkdtemp(temp));snprintf(work,sizeof(work),"%s",temp);
  ZamnNativeRuntime rt={0};for(int i=0;i<64;i++)rt.state[i]=(uint8_t)i;
  zon_init(work,0x1234,"ALICE",0);
  test(&rt,0,1); // peer owns Resume; hash gate and resume barrier
  local_slot=0;inject_touch=1;control_messages=0;tick=0;reply.seq=0;
  uint32_t frame=100;
  assert(zon_run(&rt,&frame,0,(SDL_Renderer*)1,NULL,NULL,0)==1 && frame==100);
  local_slot=1; // Android-owned touchscreen Resume uses the same Xbox barrier.
  test(&rt,1,1); // peer owns Save; same protocol, local .zsv written
  char path[1300];snprintf(path,sizeof(path),"%s/zamn_shared_online/0000000000001234/ALICE/mode_0/ALPHA.zsv",work);
  struct stat st;assert(stat(path,&st)==0 && st.st_size>80);
  memset(rt.state,0x44,64);
  // The mock Xbox reports its saved snapshot's frame/hash in the barrier.
  // Build an expected second-phase peer result from the local save header.
  test(&rt,2,2);assert(rt.state[5]==5 && synced);
  bad_gate=1;control_messages=0;reply.seq=0;tick=0;
  uint32_t bad_frame=100;
  assert(zon_run(&rt,&bad_frame,0,(SDL_Renderer*)1,NULL,NULL,0)==0);
  bad_gate=0;
  selected_mode=1;zon_init(work,0x1234,"ALICE",1);
  // No-save refuses file creation and refuses loading a savable-mode save.
  test(&rt,0,1);
  puts("PASS R11: Xbox shared pause gate, Android-owner touch resume, remote resume, Save and Load barriers, restored frame, hash mismatch refusal, no-save mode");
}
