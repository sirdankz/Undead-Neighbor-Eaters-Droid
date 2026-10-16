/* Xbox R49.7-compatible deterministic START+BACK pause/save control channel.
 * No additional wire messages; uses ZNP_CONTROL and the Xbox's exact structs.
 * This menu is *not* the Android-only offline HUD configuration overlay.
 */
#include "zamn_android_online_pause.h"
#include "zamn_android_net.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <SDL.h>

enum { CTRL_MENU=1,CTRL_SAVE=2,CTRL_LOAD=3,CTRL_BARRIER=4,CTRL_RESUME=5 };
enum { MENU_MAIN=0,MENU_KEYBOARD=1,MENU_LOAD=2 };
#define SV_MAGIC 0x3856535au
#define SV_SCHEMA 3u
#define SV_BUILD 0x5A4D4908u
#define SV_MAX 16
#define SV_PAGE 6
#define SNAP_MAX (32*1024*1024)

typedef struct {
  uint32_t magic,schema,build,snapshot_size,frame;
  uint8_t ruleset,reserved_mode[3];
  uint16_t level,reserved_level;
  uint32_t level_frames;
  uint64_t rom_hash,state_hash,blob_hash;
  char name[SV_MAX+1],reserved2[7];
} OnlineSaveHeader;
typedef struct {
  uint32_t seq;
  uint8_t owner,screen,selection,key_index,list_count,list_selected,reserved[2];
  char name[SV_MAX+1],status[32],list[SV_PAGE][SV_MAX+1];
} MenuWire;
typedef struct {uint32_t seq,frame;char name[SV_MAX+1],reserved[3];} CommandWire;
typedef struct {uint32_t seq,frame;uint64_t state_hash;uint8_t phase,ok,reserved[6];} BarrierWire;
typedef struct {OnlineSaveHeader h;uint8_t *data;int bytes;} Staged;
_Static_assert(sizeof(MenuWire)==164,"Xbox R49.7 menu payload mismatch");
_Static_assert(sizeof(CommandWire)==28,"Xbox R49.7 command mismatch");
_Static_assert(sizeof(BarrierWire)==24,"Xbox R49.7 barrier mismatch");
_Static_assert(sizeof(OnlineSaveHeader)==80,"Xbox R49.7 save format mismatch");
static char root[1080];static uint64_t rom;static uint8_t mode;
static uint64_t fnv(const uint8_t*p,int n){uint64_t h=UINT64_C(14695981039346656037);for(int i=0;i<n;++i){h^=p[i];h*=UINT64_C(1099511628211);}return h;}
static void clean_name(const char*src,char*dst,size_t n){
  size_t i=0;if(!n)return;
  while(src && *src && i+1<n && i<SV_MAX){char ch=*src++;
    if(ch>='a'&&ch<='z')ch=(char)(ch-'a'+'A');
    if(ch==' ')ch='_';
    if((ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='-'||ch=='_'||ch=='.')dst[i++]=ch;
  }dst[i]=0;
}
void zon_init(const char*dir,uint64_t hash,const char*player,int ruleset){
  rom=hash;mode=(uint8_t)(ruleset&3);root[0]=0;
  char who[SV_MAX+1];clean_name(player,who,sizeof(who));if(!who[0])SDL_strlcpy(who,"LOCAL",sizeof(who));
  if(dir)SDL_snprintf(root,sizeof(root),"%s/zamn_shared_online/%016llX/%s/mode_%u",
                      dir,(unsigned long long)hash,who,(unsigned)mode);
}
static bool ensure_dirs(void){
  if(!root[0]||strlen(root)>1000)return false;
  char s[sizeof(root)];SDL_strlcpy(s,root,sizeof(s));
  for(char*p=s+1;*p;p++)if(*p=='/'){*p=0;if(mkdir(s,0700)!=0&&errno!=EEXIST)return false;*p='/';}
  return mkdir(s,0700)==0||errno==EEXIST;
}
static bool save_path(const char *name,char *out,size_t cap){
  char clean[SV_MAX+1];clean_name(name,clean,sizeof(clean));
  if(!clean[0]||!root[0])return false;
  int n=SDL_snprintf(out,cap,"%s/%s.zsv",root,clean);return n>0 && (size_t)n<cap;
}
static void staged_free(Staged *s){free(s->data);memset(s,0,sizeof(*s));}
static bool snapshot_backup(ZamnNativeRuntime *rt,Staged*s,uint32_t frame){
  staged_free(s);int cap=zamn_runtime_snapshot_size(rt),used=0;
  if(cap<=0||cap>SNAP_MAX)return false;
  s->data=(uint8_t*)malloc((size_t)cap);if(!s->data)return false;
  if(!zamn_runtime_save_snapshot(rt,s->data,cap,&used)||used<=0||used>cap){staged_free(s);return false;}
  s->bytes=used;s->h.frame=frame;s->h.state_hash=zamn_runtime_state_hash(rt);
  return true;
}
static bool snapshot_apply(ZamnNativeRuntime *rt,const Staged*s){
  return s&&s->data&&s->bytes>0 &&
       zamn_runtime_load_snapshot(rt,s->data,s->bytes)&&
       zamn_runtime_state_hash(rt)==s->h.state_hash;
}
static bool write_save(ZamnNativeRuntime *rt,uint32_t frame,const char*name,uint64_t*hash){
  char path[1200],temp[1208],san[SV_MAX+1];
  if((mode&1)||!save_path(name,path,sizeof(path))||!ensure_dirs())return false;
  int cap=zamn_runtime_snapshot_size(rt),used=0;
  if(cap<=0||cap>SNAP_MAX)return false;
  uint8_t *data=(uint8_t*)malloc((size_t)cap);if(!data)return false;
  bool ok=zamn_runtime_save_snapshot(rt,data,cap,&used)&&used>0&&used<=cap;
  if(ok){
    OnlineSaveHeader h;memset(&h,0,sizeof(h));clean_name(name,san,sizeof(san));
    h.magic=SV_MAGIC;h.schema=SV_SCHEMA;h.build=SV_BUILD;h.snapshot_size=(uint32_t)used;
    h.frame=frame;h.ruleset=mode;h.level=zamn_runtime_current_level(rt);
    h.rom_hash=rom;h.state_hash=zamn_runtime_state_hash(rt);h.blob_hash=fnv(data,used);
    SDL_strlcpy(h.name,san,sizeof(h.name));SDL_snprintf(temp,sizeof(temp),"%s.tmp",path);
    FILE*f=fopen(temp,"wb");ok=f && fwrite(&h,1,sizeof(h),f)==sizeof(h)&&
                      fwrite(data,1,(size_t)used,f)==(size_t)used&&fflush(f)==0;
    if(f){if(ok && fsync(fileno(f))!=0)ok=false;if(fclose(f)!=0)ok=false;}
    if(ok && rename(temp,path)!=0)ok=false;
    if(!ok)remove(temp);
    if(hash)*hash=h.state_hash;
  }
  free(data);return ok;
}
static bool read_save(ZamnNativeRuntime *rt,const char*name,Staged*s){
  staged_free(s);char path[1200];if(!save_path(name,path,sizeof(path)))return false;
  FILE*f=fopen(path,"rb");if(!f)return false;
  OnlineSaveHeader h;int cap=zamn_runtime_snapshot_size(rt);
  bool ok=fread(&h,1,sizeof(h),f)==sizeof(h)&&h.magic==SV_MAGIC&&
          (h.schema==SV_SCHEMA||h.schema==2)&&h.build==SV_BUILD&&h.rom_hash==rom&&
          (h.ruleset&3u)==mode&&!(mode&1u)&&h.snapshot_size>0&&cap>0&&
          cap<=SNAP_MAX&&h.snapshot_size<=(uint32_t)cap;
  if(ok){s->data=(uint8_t*)malloc((size_t)h.snapshot_size);ok=s->data &&
       fread(s->data,1,h.snapshot_size,f)==h.snapshot_size &&
       fgetc(f)==EOF && fnv(s->data,(int)h.snapshot_size)==h.blob_hash;
    if(ok){s->h=h;s->bytes=(int)h.snapshot_size;}
  }
  fclose(f);if(!ok)staged_free(s);return ok;
}
static int list_saves(char list[][SV_MAX+1],int cap){
  DIR*d=opendir(root);if(!d)return 0;int n=0;struct dirent*e;
  while(n<cap && (e=readdir(d))){size_t len=strlen(e->d_name);
    if(len<5 || strcmp(e->d_name+len-4,".zsv"))continue;
    char name[SV_MAX+1]={0};size_t count=len-4;if(count>SV_MAX)count=SV_MAX;
    memcpy(name,e->d_name,count);
    // Validate all header fields, not just the filename.
    char path[1200];OnlineSaveHeader h;FILE*f=NULL;
    if(!save_path(name,path,sizeof(path)) || !(f=fopen(path,"rb")))continue;
    bool valid=fread(&h,1,sizeof(h),f)==sizeof(h);fclose(f);
    if(!valid || h.magic!=SV_MAGIC || (h.schema!=3&&h.schema!=2) ||
       h.build!=SV_BUILD || h.rom_hash!=rom || (h.ruleset&3u)!=mode)continue;
    SDL_strlcpy(list[n++],name,SV_MAX+1);
  }closedir(d);return n;
}
static bool barrier(uint32_t seq,int phase,bool ok,uint32_t frame,uint64_t hash,
                    bool *peer_ok,uint32_t*peer_frame,uint64_t*peer_hash){
  BarrierWire mine={0};mine.seq=seq;mine.frame=frame;mine.state_hash=hash;
  mine.phase=(uint8_t)phase;mine.ok=ok?1:0;
  uint32_t start=SDL_GetTicks(),last=0;
  while(SDL_GetTicks()-start<8000){
    uint32_t now=SDL_GetTicks();
    if(!last || now-last>=24){zan_control_send(CTRL_BARRIER,&mine,sizeof(mine));last=now;}
    uint8_t kind=0;BarrierWire peer={0};int n=zan_control_recv(&kind,&peer,sizeof(peer),12);
    if(n<0)return false;
    if(kind==CTRL_BARRIER && n==(int)sizeof(peer)&&peer.seq==seq&&peer.phase==phase){
      if(peer_ok)*peer_ok=peer.ok!=0;if(peer_frame)*peer_frame=peer.frame;
      if(peer_hash)*peer_hash=peer.state_hash;
      zan_control_send(CTRL_BARRIER,&mine,sizeof(mine));return true;
    }
  }return false;
}
static void send_command(uint8_t kind,const CommandWire *cmd){
  for(int j=0;j<8;j++){zan_control_send(kind,cmd,sizeof(*cmd));SDL_Delay(5);}
}

// Same pixel font palette as the Xbox's small overlay; rows are intentionally
// scaled for Android thumb targets, without touching the game's 4:3 renderer.
static const uint8_t FONT[36][7]={
{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
{30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
{14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{31,4,4,4,4,4,31},
{7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
{17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
{30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
{17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
{17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
{14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
{30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
{14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
{14,17,17,15,1,1,14}};
static void draw_text(SDL_Renderer*r,int x,int y,int pix,const char*s){
  for(int i=0;s&&s[i]&&i<62;i++){
    unsigned char c=(unsigned char)s[i];int z=c>='A'&&c<='Z'?c-'A':c>='0'&&c<='9'?26+c-'0':-1;
    if(z<0)continue;
    for(int yy=0;yy<7;yy++)for(int xx=0;xx<5;xx++)if(FONT[z][yy]&(1<<(4-xx))){
      SDL_Rect d={x+(i*6+xx)*pix,y+yy*pix,pix,pix};SDL_RenderFillRect(r,&d);
    }
  }
}
static void screen_draw(SDL_Renderer *r,SDL_Texture*background,const SDL_Rect*src,
                        const MenuWire*m,bool owner,bool savable){
  int w=0,h=0;SDL_GetRendererOutputSize(r,&w,&h);
  SDL_SetRenderDrawColor(r,0,0,0,255);SDL_RenderClear(r);
  if(background && src){int dw=w,dh=(int)(w*.75f);if(dh>h){dh=h;dw=(int)(h*4.f/3.f);}
    SDL_Rect rect={(w-dw)/2,(h-dh)/2,dw,dh};SDL_RenderCopy(r,background,src,&rect);}
  SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(r,1,12,7,226);SDL_Rect panel={(int)(w*.12f),(int)(h*.06f),(int)(w*.76f),(int)(h*.90f)};
  SDL_RenderFillRect(r,&panel);
  SDL_SetRenderDrawColor(r,91,218,107,255);SDL_RenderDrawRect(r,&panel);
  int pix=h/250;if(pix<2)pix=2;if(pix>4)pix=4;
  int x=(int)(w*.15f);SDL_SetRenderDrawColor(r,250,250,250,255);
  draw_text(r,x,(int)(h*.11f),pix,"ONLINE PAUSE");
  SDL_SetRenderDrawColor(r,103,248,125,255);
  draw_text(r,(int)(w*.59f),(int)(h*.11f),pix,owner?"LOCAL CONTROL":"REMOTE CONTROL");
  if(m->screen==MENU_MAIN){
    for(int i=0;i<(savable?3:1);i++){
      int y=(int)(h*(.27f+i*.145f));SDL_Rect rect={(int)(w*.18f),y,(int)(w*.64f),(int)(h*.11f)};
      SDL_SetRenderDrawColor(r,13,67,36,228);SDL_RenderFillRect(r,&rect);
      SDL_SetRenderDrawColor(r,i==m->selection?220:80,i==m->selection?250:170,110,255);
      SDL_RenderDrawRect(r,&rect);
      SDL_SetRenderDrawColor(r,245,255,241,255);
      draw_text(r,rect.x+20,y+12,pix,savable?(i==0?"SAVE GAME":i==1?"LOAD GAME":"RESUME GAME"):"RESUME GAME");
    }
  }else if(m->screen==MENU_KEYBOARD){
    SDL_SetRenderDrawColor(r,250,215,103,255);draw_text(r,x,(int)(h*.21f),pix,"NAME");
    draw_text(r,x+(int)(w*.13f),(int)(h*.21f),pix,m->name);
    const char*keys="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_. ";
    for(int i=0;i<40;i++){
      int col=i%8,row=i/8,cx=(int)(w*(.22f+col*.075f)),cy=(int)(h*(.31f+row*.093f));
      SDL_SetRenderDrawColor(r,m->key_index==i?245:80,m->key_index==i?255:188,130,255);
      char label[2]={keys[i],0};if(label[0]==' ')label[0]='_';draw_text(r,cx,cy,pix,label);
    }
    SDL_SetRenderDrawColor(r,230,255,231,255);
    draw_text(r,x,(int)(h*.83f),pix,"START DONE  B BACK");
  }else if(m->screen==MENU_LOAD){
    SDL_SetRenderDrawColor(r,250,220,103,255);draw_text(r,x,(int)(h*.21f),pix,"SELECT SAVE");
    if(!m->list_count)draw_text(r,x,(int)(h*.37f),pix,"NO SAVES FOUND");
    for(int i=0;i<m->list_count&&i<SV_PAGE;i++){
      int y=(int)(h*(.31f+i*.078f));SDL_SetRenderDrawColor(r,i==m->list_selected?240:102,i==m->list_selected?252:217,120,255);
      draw_text(r,x+12,y,pix,m->list[i]);
    }
  }
  SDL_SetRenderDrawColor(r,250,215,105,255);
  draw_text(r,x,(int)(h*.87f),pix,m->status);
  SDL_SetRenderDrawColor(r,196,221,204,255);
  draw_text(r,x,(int)(h*.93f),pix,"A SELECT  B BACK");
  SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);SDL_RenderPresent(r);
}
static int touch_hit(const MenuWire*m,float x,float y){
  if(x<.15f||x>.84f)return 0;
  if(m->screen==MENU_MAIN){for(int i=0;i<3;i++)if(y>=.27f+i*.145f&&y<.27f+i*.145f+.115f)return 100+i;}
  if(m->screen==MENU_KEYBOARD){
    if(y>=.31f&&y<.77f){int row=(int)((y-.31f)/.093f),col=(int)((x-.22f)/.075f);
      if(row>=0&&row<5&&col>=0&&col<8)return 200+row*8+col;}
    if(y>.79f)return 301;
  }
  if(m->screen==MENU_LOAD){for(int i=0;i<SV_PAGE;i++)if(y>=.31f+i*.078f&&y<.31f+i*.078f+.067f)return 400+i;}
  if(y>.89f)return 302;
  return 0;
}
static bool do_resume(uint32_t *frame,ZamnNativeRuntime*rt,uint32_t seq){
  CommandWire cmd={0};cmd.seq=seq;cmd.frame=*frame;
  send_command(CTRL_RESUME,&cmd);
  bool pok=false;uint32_t pf=0;uint64_t ph=0,hash=zamn_runtime_state_hash(rt);
  return barrier(seq,9,true,*frame,hash,&pok,&pf,&ph)&&pok&&pf==*frame&&ph==hash;
}
// return 1 success; 0 failed but safely synchronized (return to menu); -1 transport fatal.
static int save_command(ZamnNativeRuntime*rt,uint32_t frame,const CommandWire*cmd,bool send){
  if(send)send_command(CTRL_SAVE,cmd);
  uint64_t hash=0;bool local=write_save(rt,frame,cmd->name,&hash),peer=false;
  uint32_t pf=0;uint64_t ph=0;
  bool linked=barrier(cmd->seq,1,local,frame,hash,&peer,&pf,&ph);
  if(!(linked&&local&&peer&&pf==frame&&ph==hash)){
    if(local){char path[1200];if(save_path(cmd->name,path,sizeof(path)))remove(path);}
    return linked?0:-1;
  }return 1;
}
static int load_command(ZamnNativeRuntime*rt,uint32_t*frame,const CommandWire*cmd,bool send){
  if(send)send_command(CTRL_LOAD,cmd);
  Staged incoming={0},backup={0};bool local=read_save(rt,cmd->name,&incoming),peer=false;
  uint32_t pf=0;uint64_t ph=0;
  bool linked=barrier(cmd->seq,2,local,local?incoming.h.frame:0,
                      local?incoming.h.state_hash:0,&peer,&pf,&ph);
  bool same=linked&&local&&peer&&pf==incoming.h.frame&&ph==incoming.h.state_hash;
  bool attempted=false,applied=false;
  if(same){attempted=snapshot_backup(rt,&backup,*frame);
    applied=attempted&&snapshot_apply(rt,&incoming);}
  bool peer_applied=false;uint32_t paf=0;uint64_t pah=0;
  bool committed=linked&&barrier(cmd->seq,3,applied,
      applied?incoming.h.frame:*frame,applied?zamn_runtime_state_hash(rt):0,
      &peer_applied,&paf,&pah);
  int out=0;
  if(committed&&applied&&peer_applied&&paf==incoming.h.frame&&pah==incoming.h.state_hash){
    *frame=incoming.h.frame;out=1;
  }else if(attempted&&!snapshot_apply(rt,&backup))out=-1;
  else if(!linked||!committed)out=-1;
  staged_free(&incoming);staged_free(&backup);return out;
}
int zon_run(ZamnNativeRuntime*rt,uint32_t*frame,unsigned owner,
            SDL_Renderer*r,SDL_Texture*bg,const SDL_Rect*src,SDL_AudioDeviceID audio){
  if(!rt||!frame||!r)return 0;
  MenuWire menu={0};menu.owner=(uint8_t)owner;
  uint32_t seq=1;bool savable=(zan_ruleset()&1u)==0;
  bool local=zan_local_slot()==owner,online_fatal=false;
  uint32_t gate=0x80000000u^*frame,pf=0;uint64_t ph=0,hash=zamn_runtime_state_hash(rt);
  bool pok=false;
  SDL_Log("ZAMN R11 shared pause enter frame=%u localOwner=%d hash=%08X%08X",*frame,local,
          (unsigned)(hash>>32),(unsigned)hash);
  if(!barrier(gate,0,true,*frame,hash,&pok,&pf,&ph)||!pok||pf!=*frame||ph!=hash){
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Shared pause gate disagreed; refusing unsynced menu");return 0;
  }
  if(audio)SDL_PauseAudioDevice(audio,1);
  char saves[32][SV_MAX+1]={0};int save_count=0,save_sel=0;
  bool running=true,loaded=false;
  while(running){
    if(local){
      SDL_Event e;
      while(SDL_PollEvent(&e)){
        if(e.type==SDL_QUIT){online_fatal=true;running=false;break;}
        int action=0;
        if(e.type==SDL_FINGERDOWN){int hit=touch_hit(&menu,e.tfinger.x,e.tfinger.y);
          if(hit>=100&&hit<103){menu.selection=(uint8_t)(hit-100);action=1;}
          if(hit>=200&&hit<240){menu.key_index=(uint8_t)(hit-200);action=1;}
          if(hit>=400&&hit<406 && hit-400<menu.list_count){menu.list_selected=(uint8_t)(hit-400);action=1;}
          if(hit==301)action=3;if(hit==302)action=2;
        }
        if(e.type==SDL_KEYDOWN){SDL_Keycode k=e.key.keysym.sym;
          if(k==SDLK_ESCAPE||k==SDLK_BACKSPACE)action=2;
          if(k==SDLK_RETURN||k==SDLK_SPACE)action=1;
          if(k==SDLK_UP)action=4;if(k==SDLK_DOWN)action=5;
          if(k==SDLK_LEFT)action=6;if(k==SDLK_RIGHT)action=7;
          if(k==SDLK_DELETE)action=8;
          if(k==SDLK_TAB)action=3;
        }
        if(e.type==SDL_CONTROLLERBUTTONDOWN){
          switch(e.cbutton.button){
          case SDL_CONTROLLER_BUTTON_A:action=1;break;
          case SDL_CONTROLLER_BUTTON_B:action=2;break;
          case SDL_CONTROLLER_BUTTON_START:action=3;break;
          case SDL_CONTROLLER_BUTTON_X:action=8;break;
          case SDL_CONTROLLER_BUTTON_DPAD_UP:action=4;break;
          case SDL_CONTROLLER_BUTTON_DPAD_DOWN:action=5;break;
          case SDL_CONTROLLER_BUTTON_DPAD_LEFT:action=6;break;
          case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:action=7;break;
          }
        }
        if(menu.screen==MENU_MAIN){
          if(savable && action==4&&menu.selection>0)--menu.selection;
          if(savable && action==5&&menu.selection<2)++menu.selection;
          if(action==2 || (action==1 && (!savable||menu.selection==2))){
            if(!do_resume(frame,rt,++seq))online_fatal=true;
            running=false;break;
          }
          if(action==1 && savable && menu.selection==0){menu.screen=MENU_KEYBOARD;menu.key_index=0;menu.name[0]=0;}
          if(action==1 && savable && menu.selection==1){save_count=list_saves(saves,32);save_sel=0;menu.screen=MENU_LOAD;}
        }else if(menu.screen==MENU_KEYBOARD){
          int k=menu.key_index,row=k/8,col=k%8;
          if(action==4)row=(row+4)%5;if(action==5)row=(row+1)%5;
          if(action==6)col=(col+7)%8;if(action==7)col=(col+1)%8;
          menu.key_index=(uint8_t)(row*8+col);
          if(action==2){menu.screen=MENU_MAIN;menu.name[0]=0;}
          if(action==8){size_t n=strlen(menu.name);if(n)menu.name[n-1]=0;}
          if(action==1){const char *keys="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_. ";
            size_t n=strlen(menu.name);if(n<SV_MAX && (keys[menu.key_index]!=' '||n)){
              menu.name[n]=keys[menu.key_index];menu.name[n+1]=0;}
          }
          if(action==3 && menu.name[0]){
            CommandWire cmd={0};cmd.seq=++seq;cmd.frame=*frame;
            clean_name(menu.name,cmd.name,sizeof(cmd.name));
            int ok=save_command(rt,*frame,&cmd,true);
            SDL_strlcpy(menu.status,ok==1?"SAVE COMPLETE":"SAVE FAILED OR PEER MISMATCH",sizeof(menu.status));
            menu.screen=MENU_MAIN;menu.selection=0;menu.name[0]=0;
            if(ok<0){online_fatal=true;running=false;break;}
          }
        }else if(menu.screen==MENU_LOAD){
          if(action==2){menu.screen=MENU_MAIN;}
          if(save_count){
            if(action==4)save_sel=(save_sel+save_count-1)%save_count;
            if(action==5)save_sel=(save_sel+1)%save_count;
            if(action==1){CommandWire cmd={0};cmd.seq=++seq;cmd.frame=*frame;
              SDL_strlcpy(cmd.name,saves[save_sel],sizeof(cmd.name));
              int ok=load_command(rt,frame,&cmd,true);
              if(ok==1){loaded=true;running=false;break;}
              SDL_strlcpy(menu.status,"LOAD FAILED OR NOT ON BOTH",sizeof(menu.status));
              menu.screen=MENU_MAIN;
              if(ok<0){online_fatal=true;running=false;break;}
            }
          }
        }
      }
      menu.seq=seq;menu.list_count=0;menu.list_selected=0;
      memset(menu.list,0,sizeof(menu.list));
      if(menu.screen==MENU_LOAD&&save_count){
        int first=save_sel-3;if(first<0)first=0;
        if(first>save_count-SV_PAGE)first=save_count-SV_PAGE;
        if(first<0)first=0;
        int show=save_count-first;if(show>SV_PAGE)show=SV_PAGE;
        menu.list_count=(uint8_t)show;menu.list_selected=(uint8_t)(save_sel-first);
        for(int i=0;i<show;i++)SDL_strlcpy(menu.list[i],saves[first+i],SV_MAX+1);
      }
      if(running)zan_control_send(CTRL_MENU,&menu,sizeof(menu));
    }else{
      uint8_t kind=0;uint8_t buf[sizeof(MenuWire)]={0};
      int n=zan_control_recv(&kind,buf,sizeof(buf),18);
      if(n<0){online_fatal=true;running=false;}
      if(kind==CTRL_MENU && n==(int)sizeof(MenuWire))memcpy(&menu,buf,sizeof(menu));
      else if(kind==CTRL_SAVE && n==(int)sizeof(CommandWire)){
        CommandWire cmd;memcpy(&cmd,buf,sizeof(cmd));
        if(save_command(rt,*frame,&cmd,false)<0){online_fatal=true;running=false;}
      }else if(kind==CTRL_LOAD && n==(int)sizeof(CommandWire)){
        CommandWire cmd;memcpy(&cmd,buf,sizeof(cmd));
        int ok=load_command(rt,frame,&cmd,false);
        if(ok==1){loaded=true;running=false;}
        else if(ok<0){online_fatal=true;running=false;}
      }else if(kind==CTRL_RESUME && n==(int)sizeof(CommandWire)){
        CommandWire cmd;memcpy(&cmd,buf,sizeof(cmd));
        bool their_ok=false;uint32_t their_frame=0;uint64_t their_hash=0;
        uint64_t current=zamn_runtime_state_hash(rt);
        if(!barrier(cmd.seq,9,true,*frame,current,&their_ok,&their_frame,&their_hash)||
          !their_ok||their_frame!=*frame||their_hash!=current)online_fatal=true;
        running=false;
      }
      // Pump Android's system quit/back events without launching the offline
      // preferences screen. The remote owner alone controls this shared menu.
      SDL_Event e;while(SDL_PollEvent(&e))if(e.type==SDL_QUIT){online_fatal=true;running=false;}
    }
    if(running){screen_draw(r,bg,src,&menu,local,savable);SDL_Delay(16);}
  }
  if(audio)SDL_PauseAudioDevice(audio,0);
  if(online_fatal)return 0;
  if(loaded && !zan_resync_after_shared_load(*frame))return 0;
  SDL_Log("ZAMN R11 shared pause %s frame=%u",loaded?"load":"resume",*frame);
  return loaded?2:1;
}
