#define SDL_MAIN_HANDLED
#define _XOPEN_SOURCE 700
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include "../../android_port/app/src/main/cpp/zamn_android_touch.h"
int SDL_GetCurrentDisplayMode(int n,SDL_DisplayMode*d){(void)n;d->w=1804;d->h=832;return 0;}
int SDL_snprintf(char *s,size_t n,const char*f,...){va_list a;va_start(a,f);int v=vsnprintf(s,n,f,a);va_end(a);return v;}
void SDL_Log(const char*f,...){(void)f;}
static void need(int ok,const char*s){if(!ok){fprintf(stderr,"FAIL: %s\n",s);exit(2);}}
int main(void){
 char dir[]="/tmp/zamn-r9-touch-XXXXXX";need(mkdtemp(dir)!=NULL,"temp");
 char path[256];snprintf(path,sizeof(path),"%s/zamn_touch.cfg",dir);
 FILE*f=fopen(path,"w");need(f!=NULL,"config");
 fprintf(f,"1 0 1.0\n");
 float xy[11][2]={{.175,.680},{.175,.830},{.137,.755},{.213,.755},
  {.782,.624},{.733,.847},{.925,.784},{.455,.924},{.568,.924},{.100,.423},{.901,.423}};
 for(int i=0;i<11;i++)fprintf(f,"%f %f\n",xy[i][0],xy[i][1]);
 fprintf(f,"2 0 .25\nR9");
 for(int i=0;i<11;i++)fprintf(f," %f",i==4?.4f:1.0f);
 fprintf(f," .50\n");fclose(f);
 zat_load(dir);
 zat_touch(1,.782f,.624f,true);
 need((zat_poll(1804,832,0)&1)!=0,"center of smaller A registers");zat_release_all();
 zat_touch(2,.782f+24.f/1804.f,.624f,true);
 need((zat_poll(1804,832,0)&1)==0,"tap outside individually shrunken A not accepted");zat_release_all();
 zat_touch(3,.175f,.755f,true);
 zat_touch(3,.220f,.700f,true);
 need((zat_poll(1804,832,0)&(1u<<4))!=0,"smaller joystick reaches UP");zat_release_all();
 printf("PASS R9 touch: individually sized buttons and compact joystick active regions\n");
}
