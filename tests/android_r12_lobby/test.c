#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "zamn_android_lobby.h"
int SDL_NumJoysticks(void){return 0;}
SDL_bool SDL_IsGameController(int i){(void)i;return SDL_FALSE;}
SDL_GameController *SDL_GameControllerOpen(int i){(void)i;return NULL;}
SDL_Joystick *SDL_GameControllerGetJoystick(SDL_GameController *p){(void)p;return NULL;}
SDL_JoystickID SDL_JoystickInstanceID(SDL_Joystick *p){(void)p;return -1;}
void SDL_GameControllerClose(SDL_GameController *p){(void)p;}
static SDL_Renderer *const ren=(SDL_Renderer*)(uintptr_t)0x100;
static SDL_Event queued;
static int queued_present=0,draws=0;
int SDL_PollEvent(SDL_Event *e){if(!queued_present)return 0;*e=queued;queued_present=0;return 1;}
Uint32 SDL_GetTicks(void){static Uint32 t=1;return t+=50;}
int SDL_GetRendererOutputSize(SDL_Renderer*r,int*w,int*h){(void)r;*w=1920;*h=900;return 0;}
int SDL_RenderSetLogicalSize(SDL_Renderer*r,int w,int h){(void)r;(void)w;(void)h;return 0;}
int SDL_RenderSetViewport(SDL_Renderer*r,const SDL_Rect*rect){(void)r;(void)rect;return 0;}
int SDL_SetRenderDrawColor(SDL_Renderer*r,Uint8 a,Uint8 b,Uint8 c,Uint8 d){(void)r;(void)a;(void)b;(void)c;(void)d;return 0;}
int SDL_RenderFillRect(SDL_Renderer*r,const SDL_Rect*x){(void)r;(void)x;return 0;}
int SDL_RenderClear(SDL_Renderer*r){(void)r;return 0;}
void SDL_RenderPresent(SDL_Renderer*r){(void)r;draws++;}
void SDL_Log(const char *fmt,...){(void)fmt;}
static void finger(float x,float y){memset(&queued,0,sizeof(queued));queued.type=SDL_FINGERDOWN;queued.tfinger.x=x/478.f;queued.tfinger.y=y/224.f;queued_present=1;}
static int poll_ui(int host){return zalobby_tick("HOST DIRECT","LOCAL IP 192.168.1.2","P1 HOST","P2 READY", "PLAYER 2 READY / PRESS A",2,1,32,4,host,0,-1,-1,0);}
int main(void){
  zalobby_attach(ren);assert(zalobby_available());assert(poll_ui(1)==0);assert(draws==1);
  finger(430,192);assert(poll_ui(1)&1);zalobby_detach();
  zalobby_attach(ren);finger(430,192);assert((poll_ui(0)&1)==0);zalobby_detach();
  zalobby_attach(ren);finger(45,170);assert((poll_ui(1)&4)!=0);zalobby_detach();
  zalobby_attach(ren);finger(150,170);assert((poll_ui(1)&8)!=0);zalobby_detach();
  zalobby_attach(ren);finger(242,170);assert((poll_ui(1)&16)!=0);zalobby_detach();
  zalobby_attach(ren);finger(383,170);assert((poll_ui(1)&32)!=0);zalobby_detach();
  zalobby_attach(ren);finger(60,200);assert((poll_ui(1)&2)!=0);zalobby_detach();
  printf("R12 lobby touch host start, guest wait, delay -/+, rollback selector, reset AUTO, back: PASS\n");
  return 0;
}
