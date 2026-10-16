/* R5 mobile HUD: user-defined normalized positions in private app storage.
   No gameplay/snapshot/network protocol changes. */
#include "zamn_android_touch.h"
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define NB 12
#define LEGACY_NB 11
#define NF 16
static struct {float x,y;} keys[NB];
static const float defaults[NB][2]={
  {.175f,.625f},{.175f,.885f},{.063f,.755f},{.288f,.755f},
  {.782f,.624f},{.733f,.847f},{.925f,.784f},{.455f,.924f},{.568f,.924f},
  {.100f,.423f},{.901f,.423f},{.920f,.602f}
};
static const char* names[NB]={"U","D","L","R","CYCLE","CHANGE","SHOOT","BK","ST","PREV","NEXT","MAP"};
static const unsigned masks[NB]={1u<<4,1u<<5,1u<<6,1u<<7,1u<<0,1u<<8,1u<<1,1u<<2,1u<<3,1u<<12,(1u<<13)|(1u<<0),1u<<10};
static struct {SDL_FingerID id;float x,y,startx,starty;bool down,dpad;} fingers[NF];
static int dpad_style=1;
static bool floating=false;
static float deadzone=.12f;
static bool enabled=true,auto_hide=true;
static float scale=1.f;
static float button_sizes[NB]={1,1,1,1,1,1,1,1,1,1,1,1};
static float dpad_size=1.f;
static float clamp(float v,float lo,float hi){return v<lo?lo:v>hi?hi:v;}
static bool visible(bool controller){return enabled&&!(auto_hide&&controller);}
void zat_load(const char* dir){
  enabled=true;auto_hide=true;scale=1.f;dpad_style=1;floating=false;deadzone=.12f;dpad_size=1.f;
  for(int i=0;i<NB;i++)button_sizes[i]=1.f;
  for(int i=0;i<NB;i++){keys[i].x=defaults[i][0];keys[i].y=defaults[i][1];}
  if(!dir)return;
  char path[1024];
  if(SDL_snprintf(path,sizeof(path),"%s/zamn_touch.cfg",dir)>=(int)sizeof(path))return;
  FILE* fp=fopen(path,"r");if(!fp)return;
  int show=1,automatic=1;float s=1.f;
  if(fscanf(fp,"%d %d %f",&show,&automatic,&s)!=3){fclose(fp);return;}
  enabled=show!=0;auto_hide=automatic!=0;
  if(isfinite(s))scale=clamp(s,.70f,1.45f);
  for(int i=0;i<LEGACY_NB;i++){
    float x=0,y=0;
    if(fscanf(fp,"%f %f",&x,&y)!=2)break;
    if(isfinite(x)&&isfinite(y)){
      keys[i].x=clamp(x,.035f,.965f);keys[i].y=clamp(y,.16f,.965f);
    }
  }
  // Optional R8 fields follow all eleven legacy coordinates, so R5-R7 files remain valid.
  int kind=1,moving=0;float dz=.12f;
  if(fscanf(fp,"%d %d %f",&kind,&moving,&dz)==3){
    dpad_style=kind<0?0:kind>2?2:kind;
    floating=moving!=0;
    if(isfinite(dz))deadzone=clamp(dz,.12f,.55f);
  }
  // Optional R9 per-control radii; older R5-R8 layout files remain supported.
  char version[12]={0};
  if(fscanf(fp,"%11s",version)==1 && strcmp(version,"R9")==0){
    for(int i=0;i<LEGACY_NB;i++){
      float v=1.f;
      if(fscanf(fp,"%f",&v)!=1)break;
      if(isfinite(v))button_sizes[i]=clamp(v,.4f,1.7f);
    }
    float v=1.f;if(fscanf(fp,"%f",&v)==1 && isfinite(v))dpad_size=clamp(v,.45f,1.7f);
    // R10 extension: leaves the original 11 coordinates / R9 sizes intact.
    // Older R9 config files simply use the new MAP default.
    char extra[12]={0};float mx=0.f,my=0.f,ms=1.f;
    if(fscanf(fp,"%11s %f %f %f",extra,&mx,&my,&ms)==4 && strcmp(extra,"MAP")==0){
      if(isfinite(mx)&&isfinite(my)){
        keys[11].x=clamp(mx,.035f,.965f);
        keys[11].y=clamp(my,.16f,.965f);
      }
      if(isfinite(ms))button_sizes[11]=clamp(ms,.4f,1.7f);
    }
  }
  fclose(fp);
  SDL_Log("ZAMN R5 touch layout: enabled=%d auto_hide=%d scale=%.2f",enabled,auto_hide,scale);
}
void zat_release_all(void){memset(fingers,0,sizeof(fingers));}
void zat_touch(SDL_FingerID id,float x,float y,bool down){
  for(int i=0;i<NF;i++)if(fingers[i].down&&fingers[i].id==id){
    fingers[i].x=x;fingers[i].y=y;fingers[i].down=down;return;
  }
  if(!down)return;
  for(int i=0;i<NF;i++)if(!fingers[i].down){
    fingers[i].id=id;fingers[i].x=x;fingers[i].y=y;
    fingers[i].startx=x;fingers[i].starty=y;fingers[i].down=true;
    float cx=0,cy=0;for(int j=0;j<4;j++){cx+=keys[j].x;cy+=keys[j].y;}cx*=.25f;cy*=.25f;
    // Floating pad starts anywhere in the left-hand gameplay region.
    // Calculate the pickup radius in pixels: the four legacy D-pad positions
    // span much farther horizontally on widescreen phones than vertically.
    float w=1.f,h=1.f;
    // SDL stores normalized finger coordinates; use the current display size.
    SDL_DisplayMode dm;
    if(SDL_GetCurrentDisplayMode(0,&dm)==0){w=(float)dm.w;h=(float)dm.h;}
    if(w<10||h<10){w=1804;h=832;}
    float extent=0.f;
    for(int j=0;j<4;j++){
      float dx=(keys[j].x-cx)*w,dy=(keys[j].y-cy)*h;
      float d=sqrtf(dx*dx+dy*dy);if(d>extent)extent=d;
    }
    float dx=(x-cx)*w,dy=(y-cy)*h;
    float pickup=(.118f*fminf(w,h)*scale*dpad_size)*1.2f;
    bool action_hit=false;
    float button_radius=.050f*fminf(w,h)*scale;
    for(int j=4;j<NB;j++){
      float ddx=(x-keys[j].x)*w,ddy=(y-keys[j].y)*h;
      if(ddx*ddx+ddy*ddy<button_radius*button_radius*button_sizes[j]*button_sizes[j]*1.15f){action_hit=true;break;}
    }
    bool direction_hit=false;
    if(!action_hit)for(int j=0;j<4;j++){
      float xx=(x-keys[j].x)*w,yy=(y-keys[j].y)*h;
      float key_r=button_radius*button_sizes[j]*1.45f;
      if(xx*xx+yy*yy<=key_r*key_r){direction_hit=true;break;}
    }
    fingers[i].dpad=!action_hit && ((floating && x<.44f && y>.24f) ||
      (!floating && dx*dx+dy*dy<pickup*pickup) || direction_hit);
    return;
  }
}
static float radius(int w,int h){return .050f*(w<h?w:h)*scale;}
static float control_radius(int i,int w,int h){return radius(w,h)*button_sizes[i];}
static float pad_radius(int w,int h){return .118f*(w<h?w:h)*scale*dpad_size;}
uint16_t zat_poll(int w,int h,bool controller){
  if(!visible(controller)||w<1||h<1)return 0;
  unsigned bits=0;
  /* A single thumb owns the D-pad; all eight digital directions can be reached
     without lifting it. The optional stick quantizes direction in 45-degree
     sectors, with an adjustable radial deadzone. */
  float cx=0,cy=0;
  for(int j=0;j<4;j++){cx+=keys[j].x*w;cy+=keys[j].y*h;}
  cx*=.25f;cy*=.25f;
  float extent=0.f;for(int j=0;j<4;j++){
    float dx=keys[j].x*w-cx,dy=keys[j].y*h-cy;
    float d=sqrtf(dx*dx+dy*dy);if(d>extent)extent=d;
  }
  const float pad_r=pad_radius(w,h);
  for(int i=0;i<NF;i++)if(fingers[i].down){
    float px=fingers[i].x*w,py=fingers[i].y*h;
    if(fingers[i].dpad){
      float base_x=floating?fingers[i].startx*w:cx;
      float base_y=floating?fingers[i].starty*h:cy;
      float dx=px-base_x,dy=py-base_y;
      float length=sqrtf(dx*dx+dy*dy);
      if(length>pad_r*deadzone){
        if(dpad_style==0){
          // Cross: broad axis thresholds keep diagonals intuitive.
          float threshold=pad_r*deadzone*.70f;
          if(dx < -threshold)bits|=1u<<6;
          if(dx > threshold)bits|=1u<<7;
          if(dy < -threshold)bits|=1u<<4;
          if(dy > threshold)bits|=1u<<5;
        }else{
          // Eight-way disc / virtual stick: mutually exclusive 45-degree sectors.
          float a=atan2f(dy,dx);
          int sector=(int)floorf((a+3.14159265358979323846f/8.f) /
                                  (3.14159265358979323846f/4.f));
          sector=(sector%8+8)%8;
          if(sector==3||sector==4||sector==5)bits|=1u<<6;
          if(sector==7||sector==0||sector==1)bits|=1u<<7;
          if(sector==5||sector==6||sector==7)bits|=1u<<4;
          if(sector==1||sector==2||sector==3)bits|=1u<<5;
        }
      }
      continue;
    }
    int nearest=-1;float best=1.0e30f;
    for(int j=4;j<NB;j++){
      float xx=px-keys[j].x*w,yy=py-keys[j].y*h,d=xx*xx+yy*yy;
      if(d<=control_radius(j,w,h)*control_radius(j,w,h) && d<best){best=d;nearest=j;}
    }
    if(nearest>=0)bits|=masks[nearest];
  }
  return (uint16_t)bits;
}
static const unsigned char* letter(char c){
  static const unsigned char A[]={14,17,17,31,17,17,17},B[]={30,17,17,30,17,17,30},
    D[]={30,17,17,17,17,17,30},K[]={17,18,20,24,20,18,17},
    L[]={16,16,16,16,16,16,31},R[]={30,17,17,30,20,18,17},
    S[]={15,16,16,14,1,1,30},T[]={31,4,4,4,4,4,4},
    P[]={30,17,17,30,16,16,16},E[]={31,16,16,30,16,16,31},
    V[]={17,17,17,17,17,10,4},N[]={17,25,21,19,17,17,17},
    X[]={17,17,10,4,10,17,17},
    C[]={14,17,16,16,16,17,14},H[]={17,17,17,31,17,17,17},
    G[]={14,17,16,23,17,17,14},I[]={31,4,4,4,4,4,31},
    O[]={14,17,17,17,17,17,14},M[]={17,27,21,21,17,17,17},
    U[]={17,17,17,17,17,17,14},Y[]={17,17,10,4,4,4,4};
  switch(c){case 'A':return A;case 'B':return B;case 'D':return D;
    case 'K':return K;case 'L':return L;case 'R':return R;case 'S':return S;
    case 'T':return T;case 'U':return U;case 'Y':return Y;
    case 'P':return P;case 'E':return E;case 'V':return V;case 'N':return N;case 'X':return X;
    case 'C':return C;case 'H':return H;case 'G':return G;case 'I':return I;case 'O':return O;case 'M':return M;default:return NULL;}
}
static void hud_text(SDL_Renderer* rend,const char* str,int x,int y,int pixel){
  SDL_SetRenderDrawColor(rend,250,255,246,230);
  for(int i=0;str[i];i++){
    const unsigned char* rows=letter(str[i]);if(!rows)continue;
    for(int yy=0;yy<7;yy++)for(int xx=0;xx<5;xx++)if(rows[yy]&(1u<<(4-xx))){
      SDL_Rect dot={x+(i*6+xx)*pixel,y+yy*pixel,pixel,pixel};SDL_RenderFillRect(rend,&dot);
    }
  }
}
void zat_draw(SDL_Renderer* rend,int w,int h,bool controller){
  if(!rend||!visible(controller)||w<1||h<1)return;
  float r=radius(w,h);
  SDL_SetRenderDrawBlendMode(rend,SDL_BLENDMODE_BLEND);
  for(int i=0;i<NB;i++){
    if(i<4 && dpad_style!=0)continue;
    r=control_radius(i,w,h);
    int x=(int)(keys[i].x*w),y=(int)(keys[i].y*h),rad=(int)r;
    SDL_SetRenderDrawColor(rend,5,38,17,128);
    for(int row=-rad;row<=rad;row++){
      float f=sqrtf(fmaxf(0.f,r*r-(float)row*row));
      SDL_RenderDrawLine(rend,x-(int)f,y+row,x+(int)f,y+row);
    }
    SDL_SetRenderDrawColor(rend,104,255,104,208);
    for(int d=0;d<360;d+=8){
      float a=(float)d*.0174532925f,b=(float)(d+8)*.0174532925f;
      SDL_RenderDrawLine(rend,x+(int)(cosf(a)*r),y+(int)(sinf(a)*r),
                              x+(int)(cosf(b)*r),y+(int)(sinf(b)*r));
    }
    int size=(int)fmaxf(1.f,r/19.f),chars=(int)strlen(names[i]);
    int tx=x-(chars*6-1)*size/2,ty=y-7*size/2;
    hud_text(rend,names[i],tx,ty,size);
  }
  if(dpad_style!=0){
    float cx=0.f,cy=0.f;for(int i=0;i<4;i++){cx+=keys[i].x*w;cy+=keys[i].y*h;}cx*=.25f;cy*=.25f;
    float bx=cx,by=cy,thumbx=cx,thumby=cy;
    float extent=0.f;for(int j=0;j<4;j++){
      float dx=keys[j].x*w-cx,dy=keys[j].y*h-cy;
      float d=sqrtf(dx*dx+dy*dy);if(d>extent)extent=d;
    }
    float pad_r=pad_radius(w,h);
    r=radius(w,h)*dpad_size;
    for(int i=0;i<NF;i++)if(fingers[i].down && fingers[i].dpad){
      bx=floating?fingers[i].startx*w:cx;by=floating?fingers[i].starty*h:cy;
      float dx=fingers[i].x*w-bx,dy=fingers[i].y*h-by;
      float len=sqrtf(dx*dx+dy*dy);float t=len>pad_r?pad_r/len:1.f;
      thumbx=bx+dx*t;thumby=by+dy*t;break;
    }
    SDL_SetRenderDrawColor(rend,7,48,26,150);
    for(int dy=-(int)pad_r;dy<=(int)pad_r;dy++){
      float off=sqrtf(fmaxf(0.f,pad_r*pad_r-(float)dy*dy));
      SDL_RenderDrawLine(rend,(int)(bx-off),(int)(by+dy),(int)(bx+off),(int)(by+dy));
    }
    SDL_SetRenderDrawColor(rend,100,255,108,220);
    for(int d=0;d<360;d+=6){float a=d*.0174532925f,b=(d+6)*.0174532925f;
      SDL_RenderDrawLine(rend,(int)(bx+cosf(a)*pad_r),(int)(by+sinf(a)*pad_r),
                              (int)(bx+cosf(b)*pad_r),(int)(by+sinf(b)*pad_r));}
    SDL_Rect knob={(int)(thumbx-r*.44f),(int)(thumby-r*.44f),(int)(r*.88f),(int)(r*.88f)};
    SDL_SetRenderDrawColor(rend,103,240,123,165);SDL_RenderFillRect(rend,&knob);
  }
  SDL_SetRenderDrawBlendMode(rend,SDL_BLENDMODE_NONE);
}
