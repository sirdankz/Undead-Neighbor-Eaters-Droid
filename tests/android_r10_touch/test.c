#define _XOPEN_SOURCE 700
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include "../../android_port/app/src/main/cpp/zamn_android_touch.h"
int SDL_GetCurrentDisplayMode(int n,SDL_DisplayMode*d){(void)n;d->w=1804;d->h=832;return 0;}
int SDL_snprintf(char *s,size_t n,const char*f,...){va_list a;va_start(a,f);int v=vsnprintf(s,n,f,a);va_end(a);return v;}
void SDL_Log(const char*f,...){(void)f;}
static void check(int value,const char*why){if(!value){fprintf(stderr,"FAIL R10: %s\n",why);exit(2);}}
static const float coords[11][2]={{.175,.680},{.175,.830},{.137,.755},{.213,.755},
 {.782,.624},{.733,.847},{.925,.784},{.455,.924},{.568,.924},{.100,.423},{.901,.423}};
static void write_cfg(const char *dir,int has_map,int legacy){
 char p[256];snprintf(p,sizeof p,"%s/zamn_touch.cfg",dir);FILE *f=fopen(p,"w");check(f!=NULL,"create config");
 fprintf(f,"1 0 1.0\n");
 for(int i=0;i<11;i++)fprintf(f,"%.3f %.3f\n",coords[i][0],coords[i][1]);
 fprintf(f,"1 0 .12\nR9");for(int i=0;i<11;i++)fprintf(f," 1.000");
 fprintf(f," 1.000\n");
 if(has_map)fprintf(f,"MAP %.3f %.3f %.3f\n",legacy?.920:.865,legacy?.602:.581,legacy?1.:.55);
 fclose(f);
}
static unsigned press(float x,float y){zat_release_all();zat_touch(1,x,y,true);unsigned xbits=zat_poll(1804,832,false);zat_release_all();return xbits;}
int main(void){
 char dir[]="/tmp/zamn-r10-XXXXXX";check(mkdtemp(dir)!=NULL,"temp dir");
 zat_load(NULL);
 check((press(.175f,.755f)&(1u<<4))==0,"default disc center yields neutral (12% deadzone)");
 zat_touch(2,.175f,.755f,true);zat_touch(2,.220f,.700f,true);
 check((zat_poll(1804,832,false)&((1u<<4)|(1u<<7)))==((1u<<4)|(1u<<7)),"default 8-way disc diagonal NE");zat_release_all();
 write_cfg(dir,0,0);zat_load(dir);
 check((press(.920f,.602f)&(1u<<10))!=0,"legacy R9 config uses MAP default and bit10");
 check((press(.782f,.624f)&(1u<<0))!=0,"formerly SHOOT still emits its original cycle bit");
 check((press(.733f,.847f)&(1u<<8))!=0,"formerly ACTION still emits its change item bit");
 check((press(.925f,.784f)&(1u<<1))!=0,"formerly ITEM still emits shoot bit");
 check((press(.100f,.423f)&(1u<<12))!=0,"PREV still works");
 check((press(.901f,.423f)&(1u<<13))!=0,"NEXT still works");
 write_cfg(dir,1,0);zat_load(dir);
 check((press(.865f,.581f)&(1u<<10))!=0,"R10 moved MAP button works");
 check((press(.920f,.602f)&(1u<<10))==0,"MAP hitbox respects resized location");
 printf("PASS R10: default disc, map + legacy configs, labels bit mappings, PREV/NEXT\n");
 return 0;
}
