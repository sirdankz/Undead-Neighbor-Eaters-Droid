#define SDL_MAIN_HANDLED
#define _XOPEN_SOURCE 700
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include "../../android_port/app/src/main/cpp/zamn_android_pause.h"
struct ZamnNativeRuntime{uint8_t state[64];};
size_t SDL_strlcpy(char *dest,const char *src,size_t n){size_t len=strlen(src);if(n){size_t v=len<n-1?len:n-1;memcpy(dest,src,v);dest[v]=0;}return len;}
int SDL_snprintf(char*dest,size_t size,const char*fmt,...){va_list a;va_start(a,fmt);int v=vsnprintf(dest,size,fmt,a);va_end(a);return v;}
int zamn_runtime_snapshot_size(ZamnNativeRuntime*r){(void)r;return 64;}
bool zamn_runtime_save_snapshot(ZamnNativeRuntime*r,uint8_t *d,int n,int *out){if(n<64)return false;memcpy(d,r->state,64);*out=64;return true;}
bool zamn_runtime_load_snapshot(ZamnNativeRuntime*r,const uint8_t*d,int n){if(n!=64)return false;memcpy(r->state,d,64);return true;}
uint64_t zamn_runtime_state_hash(const ZamnNativeRuntime*r){uint64_t h=0;for(int i=0;i<64;i++)h=h*131+r->state[i];return h;}
static void need(bool ok,const char*m){if(!ok){fprintf(stderr,"FAIL: %s (%s)\n",m,zap_status());exit(2);}}
int main(void){
  char tmp[]="/tmp/zamn-r9-test-XXXXXX";need(mkdtemp(tmp)!=NULL,"temp folder");
  ZamnNativeRuntime r={0};for(int i=0;i<64;i++)r.state[i]=(uint8_t)i;
  uint64_t frame=560;
  zap_init(tmp,0x1234,"ALICE",0,false);zap_set_visible(true);
  need(zap_run_action(4,&r,&frame)==0,"save action has no navigation outcome");
  memset(r.state,0xaa,64);frame=900;
  need(zap_run_action(5,&r,&frame)==0,"load action has no navigation outcome");
  need(frame==560 && r.state[5]==5,"restore snapshot / frame");
  zap_init(tmp,0x1234,"BOB",0,false);zap_set_visible(true);
  memset(r.state,99,64);frame=903;
  zap_run_action(5,&r,&frame);
  need(frame==903&&r.state[5]==99,"player isolation");
  zap_init(tmp,0x1234,"ALICE",1,false);zap_set_visible(true);
  zap_run_action(5,&r,&frame);
  need(frame==903&&r.state[5]==99,"no-save mode blocked");
  zap_init(tmp,0x1234,"ALICE",0,true);zap_set_visible(true);
  zap_run_action(4,&r,&frame);zap_run_action(5,&r,&frame);
  need(frame==903&&r.state[5]==99,"online save load blocked");
  zap_init(tmp,0x5678,"ALICE",0,false);zap_set_visible(true);
  zap_run_action(5,&r,&frame);
  need(frame==903&&r.state[5]==99,"ROM separation");
  printf("PASS R9: atomic snapshots; player/ROM/mode isolation; online and no-save blocked\n");
}
