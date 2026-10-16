#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include "SDL.h"
#include "native/runtime.h"
struct ZamnNativeRuntime { uint32_t tick; uint16_t pad[2];int suppress;};
uint32_t SDL_GetTicks(void){static uint32_t clock; return ++clock;}
void SDL_Delay(uint32_t ms){(void)ms;}
void SDL_Log(const char* fmt,...){(void)fmt;}
void SDL_LogError(int cat,const char* fmt,...){(void)cat;(void)fmt;}
int zamn_runtime_snapshot_size(ZamnNativeRuntime*r){(void)r;return 8;}
bool zamn_runtime_save_snapshot(ZamnNativeRuntime*r,uint8_t*dst,int cap,int*used){if(cap<8)return false;memcpy(dst,&r->tick,4);memcpy(dst+4,&r->pad,4);*used=8;return true;}
bool zamn_runtime_load_snapshot(ZamnNativeRuntime*r,const uint8_t*src,int n){if(n!=8)return false;memcpy(&r->tick,src,4);memcpy(r->pad,src+4,4);return true;}
void zamn_runtime_set_pad(ZamnNativeRuntime*r,int p,uint16_t v){r->pad[p]=v;}
void zamn_runtime_set_replay_suppressed(ZamnNativeRuntime*r,bool s){r->suppress=s;}
bool zamn_runtime_frame(ZamnNativeRuntime*r,uint8_t*frame,int pitch,ZamnFrameResult*result){(void)frame;(void)pitch;r->tick += r->pad[0]+r->pad[1];memset(result,0,sizeof(*result));return true;}
uint64_t zamn_runtime_state_hash(const ZamnNativeRuntime*r){return r->tick;}
bool zamn_runtime_reset_to_title(ZamnNativeRuntime*r,bool start){(void)start;r->tick=0;return true;}
#define ZAMN_ANDROID_R8_NET_TEST 1
#include "../../android_port/app/src/main/cpp/zamn_android_net.c"
int main(void){
  ZamnNativeRuntime runtime={0};s_runtime=&runtime;g.rollback=1;g.rollback_window=1;g.delay=0;g.active=1;
  znp_core_init(&g.core,1,0);assert(rb_init(&runtime));
  assert(rb_save(0));assert(znp_core_put_input(&g.core,1,0,1));
  assert(rb_predict(0));
  uint16_t pads[2]={0};assert(znp_core_frame_ready(&g.core,0,pads));assert(pads[0]==0&&pads[1]==1);
  zamn_runtime_set_pad(&runtime,0,pads[0]);zamn_runtime_set_pad(&runtime,1,pads[1]);ZamnFrameResult fr;assert(zamn_runtime_frame(&runtime,NULL,0,&fr));
  assert(runtime.tick==1);znp_core_commit_frame(&g.core,0);
  assert(rb_remote_input(0,2));assert(rb.request==0);
  assert(rb_apply(1));assert(runtime.tick==3);assert(rb.replays==1);assert(rb.corrections==1);
  assert(rb_is_authoritative(0));
  rb_record_hash(0,123);rb_publish_hashes(1);
  assert(g.core.local_hash[0].valid && g.core.local_hash[0].hash==123);
  assert(!g.core.fault);
  printf("PASS: R3 one-frame prediction, authoritative correction, snapshot replay, deferred hash\n");
  rb_reset();return 0;
}
