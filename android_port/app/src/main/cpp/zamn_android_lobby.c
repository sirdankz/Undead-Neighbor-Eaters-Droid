/* Mobile-friendly version of the Xbox R49.9.7 host/join lobby.
 * This is a display/input surface only. The unchanged ZNP1/4 protocol is
 * owned by zamn_android_net.c. No game frames advance here.
 */
#include "zamn_android_lobby.h"
#include <SDL.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
static SDL_Renderer *s_renderer;
static SDL_GameController *s_gamepad;
static SDL_JoystickID s_gamepad_id=-1;
static int s_width=480;
static Uint32 s_last_draw;
static int s_start_queued;
static int s_cancelled;
static const char *s_chars="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:./-<>% !?+";
static const unsigned char s_font[][7]={
  {14,17,17,31,17,17,17}, /* A */
  {30,17,17,30,17,17,30}, /* B */
  {14,17,16,16,16,17,14}, /* C */
  {30,17,17,17,17,17,30}, /* D */
  {31,16,16,30,16,16,31}, /* E */
  {31,16,16,30,16,16,16}, /* F */
  {14,17,16,23,17,17,15}, /* G */
  {17,17,17,31,17,17,17}, /* H */
  {31,4,4,4,4,4,31}, /* I */
  {7,2,2,2,18,18,12}, /* J */
  {17,18,20,24,20,18,17}, /* K */
  {16,16,16,16,16,16,31}, /* L */
  {17,27,21,21,17,17,17}, /* M */
  {17,25,21,19,17,17,17}, /* N */
  {14,17,17,17,17,17,14}, /* O */
  {30,17,17,30,16,16,16}, /* P */
  {14,17,17,17,21,18,13}, /* Q */
  {30,17,17,30,20,18,17}, /* R */
  {15,16,16,14,1,1,30}, /* S */
  {31,4,4,4,4,4,4}, /* T */
  {17,17,17,17,17,17,14}, /* U */
  {17,17,17,17,17,10,4}, /* V */
  {17,17,17,21,21,21,10}, /* W */
  {17,17,10,4,10,17,17}, /* X */
  {17,17,10,4,4,4,4}, /* Y */
  {31,1,2,4,8,16,31}, /* Z */
  {14,17,19,21,25,17,14}, /* 0 */
  {4,12,4,4,4,4,14}, /* 1 */
  {14,17,1,2,4,8,31}, /* 2 */
  {30,1,1,14,1,1,30}, /* 3 */
  {2,6,10,18,31,2,2}, /* 4 */
  {31,16,16,30,1,1,30}, /* 5 */
  {14,16,16,30,17,17,14}, /* 6 */
  {31,1,2,4,8,8,8}, /* 7 */
  {14,17,17,14,17,17,14}, /* 8 */
  {14,17,17,15,1,1,14}, /* 9 */
  {0,12,12,0,12,12,0}, /* : */
  {0,0,0,0,0,12,12}, /* . */
  {1,2,2,4,8,8,16}, /* / */
  {0,0,0,31,0,0,0}, /* - */
  {1,2,4,8,4,2,1}, /* < */
  {16,8,4,2,4,8,16}, /* > */
  {25,26,2,4,8,11,19}, /* % */
  {0,0,0,0,0,0,0}, /*   */
  {4,4,4,4,4,0,4}, /* ! */
  {14,17,1,2,4,0,4}, /* ? */
  {0,4,4,31,4,4,0}, /* + */
};
static void color(Uint8 r,Uint8 g,Uint8 b){SDL_SetRenderDrawColor(s_renderer,r,g,b,255);}
static void rect(int x,int y,int w,int h,Uint8 r,Uint8 g,Uint8 b){
    SDL_Rect q={x,y,w,h};color(r,g,b);SDL_RenderFillRect(s_renderer,&q);
}
static void glyph(int x,int y,char c,int scale,Uint8 r,Uint8 g,Uint8 b){
    c=(char)toupper((unsigned char)c);
    const char *at=strchr(s_chars,c);if(!at)return;
    const unsigned char *rows=s_font[at-s_chars];
    for(int yy=0;yy<7;yy++)for(int xx=0;xx<5;xx++)
      if(rows[yy]&(1<<(4-xx)))rect(x+xx*scale,y+yy*scale,scale,scale,r,g,b);
}
static void label(int x,int y,const char *str,int scale,Uint8 r,Uint8 g,Uint8 b,int max){
    if(!str)return;
    for(int i=0;str[i]&&i<max;i++)glyph(x+i*6*scale,y,str[i],scale,r,g,b);
}
static void framed(int x,int y,int w,int h,int highlight){
    rect(x,y,w,h,3,38,19);rect(x,y,w,1,highlight?255:66,highlight?255:125,highlight?255:75);
    rect(x,y+h-1,w,1,highlight?255:66,highlight?255:125,highlight?255:75);
    rect(x,y,1,h,highlight?255:66,highlight?255:125,highlight?255:75);
    rect(x+w-1,y,1,h,highlight?255:66,highlight?255:125,highlight?255:75);
}
static void open_gamepad(void){
    if(s_gamepad)return;
    for(int i=0;i<SDL_NumJoysticks();i++)if(SDL_IsGameController(i)){
        SDL_GameController *c=SDL_GameControllerOpen(i);
        if(c){s_gamepad=c;s_gamepad_id=SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(c));break;}
    }
}
void zalobby_attach(SDL_Renderer *renderer){
    s_renderer=renderer;s_last_draw=0;s_start_queued=0;s_cancelled=0;
    if(!renderer)return;
    open_gamepad();
    int w=0,h=0;SDL_GetRendererOutputSize(renderer,&w,&h);
    if(h<=0)h=224;
    s_width=(int)(224.f*(float)w/(float)h+.5f);
    if(s_width<330)s_width=330;if(s_width>620)s_width=620;
    SDL_RenderSetLogicalSize(renderer,s_width,224);
    SDL_Log("ZAMN R12 LOBBY compositor active width=%d",s_width);
}
void zalobby_detach(void){
    if(s_renderer){SDL_RenderSetLogicalSize(s_renderer,0,0);SDL_RenderSetViewport(s_renderer,NULL);}
    if(s_gamepad){SDL_GameControllerClose(s_gamepad);s_gamepad=NULL;s_gamepad_id=-1;}
    s_renderer=NULL;s_last_draw=0;s_start_queued=0;s_cancelled=0;
}
int zalobby_available(void){return s_renderer!=NULL;}
static void draw(const char *title,const char *room,const char *role,
                 const char *peer,const char *status,int delay,int rollback,
                 int rtt,int jitter,int can_start,int started,
                 int delay_choice,int rb_choice,int mobile_peer){
    const int w=s_width;
    color(0,0,0);SDL_RenderClear(s_renderer);
    framed(9,9,w-18,206,0);
    label(24,20,title,2,255,255,255,(w-48)/12);
    rect(22,46,w-44,1,55,110,62);
    label(24,54,room,1,255,211,90,(w-48)/6);
    label(24,70,role,1,180,235,193,(w-48)/6);
    label(24,88,peer,2,105,255,105,(w-48)/12);
    label(24,110,status,1,255,255,255,(w-48)/6);
    char net[120];
    const char *dm=delay_choice==-2?"HOST":delay_choice<0?"AUTO":"MANUAL";
    const char *rm=rb_choice==-2?"HOST":rb_choice<0?"AUTO":"MANUAL";
    snprintf(net,sizeof(net),"DELAY %s   ROLLBACK %s",dm,rm);
    label(24,124,net,1,255,211,90,(w-48)/6);
    if(delay>=0)snprintf(net,sizeof(net),"INPUT %dF   PREDICT %dF%s",delay,rollback,
          can_start?(mobile_peer?" / ANDROID":" / XBOX CAP 1F"):"");
    else snprintf(net,sizeof(net),"WAITING FOR NEGOTIATED VALUES");
    label(24,137,net,1,105,255,105,(w-48)/6);
    if(rtt>=0){snprintf(net,sizeof(net),"PING %dMS   JITTER %dMS",rtt,jitter);
        label(24,150,net,1,185,210,211,(w-48)/6);}
    if(can_start && !started){
        const int gap=5,avail=w-48-3*gap;
        const int bw=avail/4;
        const int xx[4]={24,24+bw+gap,24+2*(bw+gap),24+3*(bw+gap)};
        const char *text[4]={"DELAY -","DELAY +","RB MODE","ALL AUTO"};
        for(int i=0;i<4;i++){int width=i==3?w-24-xx[i]:bw;
            framed(xx[i],163,width,19,0);
            label(xx[i]+(width-(int)strlen(text[i])*6)/2,169,text[i],1,185,240,193,width/6);}
    }
    framed(24,188,116,25,0);
    label(37,196,"B BACK",1,255,255,255,13);
    if(can_start || started){
        framed(w-176,188,150,25,can_start && !started);
        label(w-162,195,started?"STARTING...":s_start_queued?"START QUEUED":"A START",1,
              started?185:105,started?210:255,started?211:105,23);
    }else {
        label(w-185,199,"WAITING FOR HOST",1,185,210,211,25);
    }
    SDL_RenderPresent(s_renderer);
}
int zalobby_tick(const char *title,const char *room,const char *role,
                 const char *peer,const char *status,int delay,int rollback,
                 int rtt,int jitter,int can_start,int started,
                 int delay_choice,int rb_choice,int mobile_peer){
    if(!s_renderer)return 0;
    SDL_Event e;
    int actions=0;
    while(SDL_PollEvent(&e)){
        if(e.type==SDL_QUIT){s_cancelled=1;continue;}
        if(e.type==SDL_CONTROLLERDEVICEADDED){open_gamepad();continue;}
        if(e.type==SDL_CONTROLLERDEVICEREMOVED && e.cdevice.which==s_gamepad_id){
            SDL_GameControllerClose(s_gamepad);s_gamepad=NULL;s_gamepad_id=-1;open_gamepad();continue;
        }
        if(e.type==SDL_KEYDOWN){
            SDL_Keycode key=e.key.keysym.sym;
            if(key==SDLK_ESCAPE||key==SDLK_BACKSPACE)s_cancelled=1;
            if(can_start && (key==SDLK_RETURN||key==SDLK_SPACE))s_start_queued=1;
            if(can_start && key==SDLK_LEFT)actions|=4;
            if(can_start && key==SDLK_RIGHT)actions|=8;
            if(can_start && (key==SDLK_r || key==SDLK_TAB))actions|=16;
            if(can_start && key==SDLK_HOME)actions|=32;
        }
        if(e.type==SDL_CONTROLLERBUTTONDOWN){
            if(e.cbutton.button==SDL_CONTROLLER_BUTTON_B || e.cbutton.button==SDL_CONTROLLER_BUTTON_BACK)s_cancelled=1;
            if(can_start&&(e.cbutton.button==SDL_CONTROLLER_BUTTON_A || e.cbutton.button==SDL_CONTROLLER_BUTTON_START))s_start_queued=1;
            if(can_start && e.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_LEFT)actions|=4;
            if(can_start && e.cbutton.button==SDL_CONTROLLER_BUTTON_DPAD_RIGHT)actions|=8;
            if(can_start && e.cbutton.button==SDL_CONTROLLER_BUTTON_RIGHTSTICK)actions|=16;
            if(can_start && e.cbutton.button==SDL_CONTROLLER_BUTTON_X)actions|=32;
        }
        if(e.type==SDL_FINGERDOWN || (e.type==SDL_MOUSEBUTTONDOWN && e.button.which!=SDL_TOUCH_MOUSEID)){
            float x,y;
            if(e.type==SDL_FINGERDOWN){x=e.tfinger.x*(float)s_width;y=e.tfinger.y*224.f;}
            else {int rw,rh;SDL_GetRendererOutputSize(s_renderer,&rw,&rh);x=(float)e.button.x*s_width/(rw?rw:1);y=(float)e.button.y*224/(rh?rh:1);}
            if(can_start && !started && y>=158.f && y<184.f){
                const float bw=(float)(s_width-48-3*5)/4.f;
                int index=(int)((x-24.f)/(bw+5.f));
                if(x>=24.f && x<(float)s_width-24.f && index>=0 && index<4 &&
                   (x-24.f)-(float)index*(bw+5.f)<(index==3?bw+6.f:bw))
                    actions|=(4<<index);
            }
            if(y>=185.f){if(x<151.f)s_cancelled=1;
                if(can_start && x>=(float)s_width-195.f)s_start_queued=1;}
        }
    }
    Uint32 now=SDL_GetTicks();
    if(!s_last_draw || now-s_last_draw>=45){
        draw(title,room,role,peer,status,delay,rollback,rtt,jitter,can_start,started,
             delay_choice,rb_choice,mobile_peer);
        s_last_draw=now;
    }
    return (s_cancelled?2:0)|(s_start_queued?1:0)|actions;
}
