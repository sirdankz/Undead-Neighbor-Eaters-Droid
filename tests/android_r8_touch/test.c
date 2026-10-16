#define _XOPEN_SOURCE 700
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <SDL.h>
#include "../../android_port/app/src/main/cpp/zamn_android_touch.h"
int SDL_GetCurrentDisplayMode(int i,SDL_DisplayMode *dm){(void)i;dm->w=1804;dm->h=832;return 0;}
int SDL_snprintf(char *text,size_t cap,const char *fmt,...){va_list ap;va_start(ap,fmt);int r=vsnprintf(text,cap,fmt,ap);va_end(ap);return r;}
void SDL_Log(const char *fmt,...){(void)fmt;}
static void need(int ok,const char*why){if(!ok){fprintf(stderr,"FAIL %s\n",why);exit(2);}}
static void cfg(const char*dir,int kind,int floating){
  char name[256];snprintf(name,sizeof(name),"%s/zamn_touch.cfg",dir);
  FILE*f=fopen(name,"w");need(f!=NULL,"create config");
  const float xy[11][2]={{.175,.625},{.175,.885},{.063,.755},{.288,.755},{.782,.624},{.733,.847},{.925,.784},{.455,.924},{.568,.924},{.100,.423},{.901,.423}};
  fprintf(f,"1 0 1.0\n");for(int i=0;i<11;i++)fprintf(f,"%.3f %.3f\n",xy[i][0],xy[i][1]);
  fprintf(f,"%d %d .25\n",kind,floating);fclose(f);
}
int main(void){
  char dir[]="/tmp/zamn-touch-XXXXXX";need(mkdtemp(dir)!=NULL,"temp directory");
  cfg(dir,0,0);zat_load(dir);
  zat_touch(1,.063f,.755f,true);need((zat_poll(1804,832,false)&(1u<<6))!=0,"cross left");zat_release_all();
  zat_touch(2,.175f,.625f,true);need((zat_poll(1804,832,false)&(1u<<4))!=0,"cross up");zat_release_all();
  zat_touch(3,.100f,.423f,true);need((zat_poll(1804,832,false)&(1u<<12))!=0,"weapon previous button");zat_release_all();
  zat_touch(4,.901f,.423f,true);need((zat_poll(1804,832,false)&(1u<<13))!=0,"weapon next button");zat_release_all();
  cfg(dir,2,1);zat_load(dir);
  zat_touch(5,.20f,.70f,true);
  zat_touch(5,.27f,.60f,true);
  unsigned bits=zat_poll(1804,832,false);
  need((bits&(1u<<4)) && (bits&(1u<<7)),"floating stick northeast");zat_release_all();
  zat_touch(6,.100f,.423f,true);
  bits=zat_poll(1804,832,false);
  need((bits&(1u<<12))!=0,"floating pad doesn't steal previous weapon");zat_release_all();
  printf("PASS R8: 8-way cross, floating stick diagonal, PREV and NEXT weapon targets\n");
  return 0;
}
