/* R9 local mobile pause, storage and editor bridge. Does not alter game state schema,
 * Xbox code, wire packets or online synchronization. Offline saves only. */
#include "zamn_android_pause.h"
#include <jni.h>
#include <SDL_system.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

#define SNAP_CAP (32*1024*1024)
#define SNAP_MAGIC "ZAMSAV9"
static char save_root[1024],status[120];
static uint64_t rom_hash;
static uint32_t ruleset;
static bool online_mode,visible;
static int selection;
static const char *items[]={"RESUME GAME","TOUCH CONTROL LAYOUT","REMAP GAMEPAD",
  "SAVE SLOT 1","LOAD SLOT 1","SAVE SLOT 2","LOAD SLOT 2","EXIT TO MAIN MENU"};

typedef struct {
  char magic[8];
  uint64_t rom;
  uint64_t runtime_hash;
  uint64_t data_hash;
  uint64_t frame;
  uint32_t mode;
  uint32_t bytes;
} SaveHeader;
static uint64_t hash_bytes(const uint8_t *p, size_t n){
  uint64_t h=UINT64_C(1469598103934665603);
  for(size_t i=0;i<n;i++){h^=p[i];h*=UINT64_C(1099511628211);}return h;
}
static void set_status(const char *msg){SDL_strlcpy(status,msg,sizeof(status));}
static void sanitize(const char *input,char *dest,size_t cap){
  size_t i=0;
  if(input)while(*input && i+1<cap){unsigned char c=(unsigned char)*input++;
    if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-')dest[i++]=(char)c;
  }
  if(!i){SDL_strlcpy(dest,"LOCAL",cap);return;}dest[i]=0;
}
void zap_init(const char *internal,uint64_t rom,const char *player,int mode,bool online){
  char who[24];sanitize(player,who,sizeof(who));
  rom_hash=rom;ruleset=mode>=0&&mode<=3?(uint32_t)mode:1u;online_mode=online;
  visible=false;selection=0;status[0]=0;
  save_root[0]=0;
  if(internal && *internal)SDL_snprintf(save_root,sizeof(save_root),
    "%s/zamn_saves/%016llX/%s/mode_%u",internal,(unsigned long long)rom,who,ruleset);
}
void zap_set_visible(bool on){
  // This is the OFFLINE-only editor. Never let an online callback present it:
  // transitioning into Android settings during a live match stalls netplay.
  visible=on && !online_mode;
  if(visible)selection=0;
}
bool zap_visible(void){return visible;}
const char *zap_status(void){return status;}
static bool savable(void){return !online_mode && !(ruleset&1u);}
static int allowed(int i){return !(i>=3&&i<=6) || savable();}
static int count(void){return 8;}
static int handle(int i){if(i<0||i>=count())return 0;if(!allowed(i))return 0;return i+1;}
int zap_hit(float x,float y){
  if(!visible)return 0;
  // Coordinates exactly match draw()'s normalized row rectangles.
  if(x<.10f||x>.90f||y<.19f||y>.94f)return 0;
  int i=(int)((y-.215f)/.087f);
  if(i<0||i>=count()||y>.215f+i*.087f+.073f)return 0;
  selection=i;return handle(i);
}
int zap_key(SDL_Keycode key){
  if(!visible)return 0;
  if(key==SDLK_ESCAPE||key==SDLK_BACKSPACE)return 1;
  if(key==SDLK_UP){selection=(selection+count()-1)%count();return 0;}
  if(key==SDLK_DOWN){selection=(selection+1)%count();return 0;}
  if(key==SDLK_RETURN||key==SDLK_SPACE)return handle(selection);
  return 0;
}
static const uint8_t G[36][7]={
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
static void label(SDL_Renderer*r,int x,int y,int pix,const char *s){
  for(int i=0;s[i]&&i<55;i++){
    int z=s[i]>='A'&&s[i]<='Z'?s[i]-'A':s[i]>='0'&&s[i]<='9'?26+s[i]-'0':-1;
    if(z<0)continue;
    for(int k=0;k<7;k++)for(int b=0;b<5;b++)if(G[z][k]&(1<<(4-b))){
      SDL_Rect rc={x+(i*6+b)*pix,y+k*pix,pix,pix};SDL_RenderFillRect(r,&rc);
    }
  }
}
void zap_draw(SDL_Renderer *r,int w,int h){
  if(!visible||!r||w<=0||h<=0)return;
  SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(r,0,10,6,216);SDL_Rect panel={0,0,w,h};SDL_RenderFillRect(r,&panel);
  int pix=(int)(fminf((float)w/170.f,(float)h/140.f));if(pix<2)pix=2;if(pix>5)pix=5;
  SDL_SetRenderDrawColor(r,122,255,138,255);label(r,(int)(w*.12f),(int)(h*.095f),pix,"PAUSE MENU");
  SDL_SetRenderDrawColor(r,254,218,99,255);
  label(r,(int)(w*.12f),(int)(h*.162f),pix,online_mode?"ONLINE GAME CONTINUES": "OFFLINE GAME PAUSED");
  for(int i=0;i<count();i++){
    int x=(int)(w*.10f),y=(int)(h*(.215f+i*.087f));
    SDL_Rect rc={x,y,(int)(w*.80f),(int)(h*.073f)};
    SDL_SetRenderDrawColor(r,20,75,43,230);SDL_RenderFillRect(r,&rc);
    SDL_SetRenderDrawColor(r,selection==i?225:100,selection==i?255:185,selection==i?220:110,255);
    SDL_RenderDrawRect(r,&rc);
    if(allowed(i)){SDL_SetRenderDrawColor(r,230,255,229,255);}
    else {SDL_SetRenderDrawColor(r,145,148,145,255);}
    label(r,x+14,y+5,pix,items[i]);
  }
  if(status[0]){
    SDL_SetRenderDrawColor(r,245,218,115,255);
    label(r,(int)(w*.12f),(int)(h*.93f),pix>2?2:pix,status);
  }
  SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
}
static bool mkdir_one(const char *path){return mkdir(path,0700)==0||errno==EEXIST;}
static bool mkdir_tree(const char *path){
  char buf[1024];if(strlen(path)>=sizeof(buf))return false;SDL_strlcpy(buf,path,sizeof(buf));
  for(char*p=buf+1;*p;p++)if(*p=='/'){*p=0;if(!mkdir_one(buf))return false;*p='/';}
  return mkdir_one(buf);
}
static int save_slot(ZamnNativeRuntime *rt,uint64_t frame,int slot){
  if(!savable()||!save_root[0]){set_status("SAVE NOT ALLOWED IN THIS MODE");return 0;}
  int cap=zamn_runtime_snapshot_size(rt),size=0;
  if(cap<=0||cap>SNAP_CAP){set_status("INVALID SNAPSHOT SIZE");return 0;}
  uint8_t *data=malloc((size_t)cap);if(!data){set_status("OUT OF MEMORY");return 0;}
  if(!zamn_runtime_save_snapshot(rt,data,cap,&size)||size<=0||size>cap){free(data);set_status("SAVE STATE FAILED");return 0;}
  SaveHeader h={0};memcpy(h.magic,SNAP_MAGIC,8);h.rom=rom_hash;h.mode=ruleset;
  h.bytes=(uint32_t)size;h.frame=frame;h.runtime_hash=zamn_runtime_state_hash(rt);h.data_hash=hash_bytes(data,(size_t)size);
  char path[1100],temp[1150];SDL_snprintf(path,sizeof(path),"%s/slot%d.zstate",save_root,slot);
  SDL_snprintf(temp,sizeof(temp),"%s.tmp",path);
  int success=0;
  if(mkdir_tree(save_root)){
    FILE*f=fopen(temp,"wb");if(f){
      if(fwrite(&h,1,sizeof(h),f)==sizeof(h) && fwrite(data,1,(size_t)size,f)==(size_t)size && fflush(f)==0){
        if(fsync(fileno(f))==0)success=1;
      }
      if(fclose(f)!=0)success=0;
      if(success && rename(temp,path)!=0)success=0;
      if(!success)remove(temp);
    }
  }
  free(data);set_status(success?"GAME SAVED":"SAVE FAILED CHECK STORAGE");return success;
}
static int load_slot(ZamnNativeRuntime *rt,uint64_t *frame,int slot){
  if(!savable()||!save_root[0]){set_status("LOAD NOT ALLOWED IN THIS MODE");return 0;}
  char path[1100];SDL_snprintf(path,sizeof(path),"%s/slot%d.zstate",save_root,slot);
  FILE *f=fopen(path,"rb");if(!f){set_status("NO SAVE IN SLOT");return 0;}
  SaveHeader h;int cap=zamn_runtime_snapshot_size(rt),old_size=0,ok=0;
  if(fread(&h,1,sizeof(h),f)!=sizeof(h)||memcmp(h.magic,SNAP_MAGIC,8)!=0||
     h.rom!=rom_hash||h.mode!=ruleset||cap<=0||cap>SNAP_CAP||h.bytes==0||h.bytes>(uint32_t)cap){
    fclose(f);set_status("SAVE WRONG PLAYER MODE OR ROM");return 0;
  }
  uint8_t *data=malloc((size_t)h.bytes),*backup=malloc((size_t)cap);
  if(!data||!backup){free(data);free(backup);fclose(f);set_status("OUT OF MEMORY");return 0;}
  if(fread(data,1,(size_t)h.bytes,f)==h.bytes && fgetc(f)==EOF &&
     h.data_hash==hash_bytes(data,h.bytes) &&
     zamn_runtime_save_snapshot(rt,backup,cap,&old_size) && old_size>0){
    bool applied=zamn_runtime_load_snapshot(rt,data,(int)h.bytes);
    if(applied && zamn_runtime_state_hash(rt)==h.runtime_hash){*frame=h.frame;ok=1;}
    else zamn_runtime_load_snapshot(rt,backup,old_size);
  }
  free(data);free(backup);fclose(f);
  set_status(ok?"GAME LOADED":"SAVE CORRUPT OR INCOMPATIBLE");return ok;
}
int zap_run_action(int action,ZamnNativeRuntime *rt,uint64_t *game_frame){
  if(!visible||online_mode)return 0;
  if(action==1){visible=false;status[0]=0;return 1;}
  if(action==2||action==3){return action;}
  if(action==8){visible=false;return 8;}
  if(action>=4&&action<=7){
    if(!savable()){set_status("SAVES DISABLED IN ONLINE OR NO SAVE");return 0;}
    const int slot=action>=6?2:1;
    if(action==4||action==6)save_slot(rt,*game_frame,slot);
    else load_slot(rt,game_frame,slot);
  }
  return 0;
}
void zap_open_android_settings(int mode){
  JNIEnv *env=(JNIEnv*)SDL_AndroidGetJNIEnv();jobject activity=SDL_AndroidGetActivity();
  if(!env||!activity){set_status("ANDROID SETTINGS UNAVAILABLE");return;}
  jclass cls=(*env)->GetObjectClass(env,activity);
  jmethodID method=cls?(*env)->GetMethodID(env,cls,"openPauseSettings","(I)V"):NULL;
  if(method)(*env)->CallVoidMethod(env,activity,method,(jint)mode);
  else set_status("SETTINGS LAUNCH FAILED");
  if((*env)->ExceptionCheck(env))(*env)->ExceptionClear(env);
  if(cls)(*env)->DeleteLocalRef(env,cls);(*env)->DeleteLocalRef(env,activity);
}
