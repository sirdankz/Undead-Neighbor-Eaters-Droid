/*
 * ZAMN R2 — experimental Android guest to unmodified R49.9.7 OG Xbox.
 * Implements the published Xbox wire protocol (ZNP1 v4, build 0x5A4D490A)
 * and ZDIR1 / ZNAT1 / ZRLY1 rendezvous messages. No server or Xbox changes.
 * R3 adds 1-frame rollback snapshots/prediction compatible with Xbox R49.9.7.
 * All network errors are bounded and never silently fall back to solo play.
 */
#include "zamn_android_net.h"
#if defined(ZAMN_ANDROID_R8_NET_TEST)
/* Legacy isolated UDP protocol smoke test has no graphical SDL renderer. */
static inline void zalobby_attach(SDL_Renderer *r){(void)r;}
static inline void zalobby_detach(void){}
static inline int zalobby_available(void){return 0;}
static inline int zalobby_tick(const char *a,const char *b,const char *c,const char *d,
                              const char *e,int f,int g,int h,int i,int j,int k,
                              int delay_choice,int rb_choice,int mobile_peer){
    (void)a;(void)b;(void)c;(void)d;(void)e;
    (void)f;(void)g;(void)h;(void)i;(void)j;(void)k;
    (void)delay_choice;(void)rb_choice;(void)mobile_peer;return 0;
}
#else
#include "zamn_android_lobby.h"
#endif
#include <SDL.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "netplay/netplay_core.h"

#define ZNP_PORT 6464
#define ZDIR_PORT 6467
#define ZDIR_IP "172.233.145.244" /* Same existing public directory as Xbox */
#define ZNP_BUILD 0x5A4D490Au
#define ZNP_PROTOCOL 4
#define ZNP_HEADER 24
#define ZNP_MAX_PACKET 256
#define ZNP_SEND_HISTORY 32
#define ZNP_TIMEOUT_MS 6000u

enum { HELLO=1, OFFER=2, READY=3, START=4, START_ACK=5, BOOT_READY=6, BOOT_GO=7,
       INPUT=8, REJECT=9, GOODBYE=10, PUNCH=11, PING=12, PONG=13, CONTROL=14 };

static struct {
  int fd, active, public_mode, have_intro, have_self, direct_ok, local_direct, peer_direct;
  int is_host, peer_mobile, have_peer, host_delay_choice, host_rb_choice;
  uint32_t last_heartbeat;char registration_token[32];
  int direct_count;
  struct sockaddr_in peer, directory;
  char cookie[20], peer_cookie[20], room[16], username[16], peer_name[16], local_ip[32];
  uint32_t session, seq, last_rx, last_tx, last_keep, last_punch, last_direct_rx, last_recv_hash;
  uint8_t ruleset, delay, rollback_window, rollback;
  uint64_t rom_hash;
  ZnpCore core;
  char error[180];
} g = {.fd=-1};

static void error_msg(const char*);
/* Android-only rollback state. Xbox uses a 1-frame prediction window and
   a ring of window+2 snapshots. None of this alters the wire protocol. */
#define RB_EMPTY UINT32_MAX
#define RB_SLOTS 5
#define RB_MASK_GAME 0x1fffu
static ZamnNativeRuntime *s_runtime;
typedef struct {uint32_t frame;int size;uint8_t valid;uint8_t*data;} RbState;
typedef struct {uint32_t frame;uint8_t valid;} RbPred;
typedef struct {uint32_t frame;uint64_t hash;uint8_t valid;} RbHash;
static struct {
  RbState states[RB_SLOTS];RbPred pred[ZNP_HISTORY];RbHash hash[ZNP_HISTORY];
  int capacity,slot_count;
  uint32_t oldest,request,latest_actual,predictions,corrections,replays;
  uint16_t last_pad;
} rb;
static void rb_reset(void){
  for(int i=0;i<RB_SLOTS;++i){free(rb.states[i].data);rb.states[i].data=NULL;}
  memset(&rb,0,sizeof(rb));rb.oldest=rb.request=rb.latest_actual=RB_EMPTY;
}
static int rb_init(ZamnNativeRuntime*rt){
  rb_reset();if(!g.rollback)return 1;
  rb.capacity=zamn_runtime_snapshot_size(rt);
  rb.slot_count=g.rollback_window+2;
  if(rb.slot_count<3)rb.slot_count=3;
  if(rb.slot_count>RB_SLOTS)rb.slot_count=RB_SLOTS;
  if(rb.capacity<=0){error_msg("Rollback snapshot capacity unavailable");return 0;}
  for(int i=0;i<rb.slot_count;i++){
    rb.states[i].data=(uint8_t*)malloc((size_t)rb.capacity);
    if(!rb.states[i].data){error_msg("Insufficient RAM for rollback snapshots");return 0;}
  }
  rb.latest_actual=g.delay?(uint32_t)g.delay-1u:RB_EMPTY;
  SDL_Log("ZAMN R3 rollback initialized window=%u snapshotCapacity=%d",g.rollback_window,rb.capacity);
  return 1;
}
static void rb_recount_oldest(void){
  rb.oldest=RB_EMPTY;
  for(int i=0;i<ZNP_HISTORY;i++)if(rb.pred[i].valid &&
     (rb.oldest==RB_EMPTY||rb.pred[i].frame<rb.oldest))rb.oldest=rb.pred[i].frame;
}
static int rb_can_advance(uint32_t frame){
  if(!g.rollback)return 1;
  if(rb.oldest!=RB_EMPTY&&frame>=rb.oldest&&frame-rb.oldest>=g.rollback_window)return 0;
  if(rb.latest_actual==RB_EMPTY)return frame<g.rollback_window;
  if(rb.latest_actual>=frame)return 1;
  return frame-rb.latest_actual<=g.rollback_window;
}
static int rb_save(uint32_t frame){
  RbState*s=&rb.states[frame%rb.slot_count];int size=0;
  if(!s->data||!zamn_runtime_save_snapshot(s_runtime,s->data,rb.capacity,&size)){
    error_msg("Rollback snapshot save failed");return 0;
  }
  s->frame=frame;s->size=size;s->valid=1;return 1;
}
static int rb_remote_input(uint32_t frame,uint16_t pad){
  RbPred*p=&rb.pred[frame&(ZNP_HISTORY-1)];
  if(p->valid&&p->frame==frame){
    bool changed=false;
    if(!znp_core_correct_input(&g.core,g.core.local_slot^1,frame,pad,&changed))return 0;
    p->valid=0;rb_recount_oldest();
    if(changed){
      if(g.core.frame>frame&&g.core.frame-frame>g.rollback_window){
        error_msg("Late rollback correction exceeded negotiated rollback window");g.core.fault=1;return 0;
      }
      if(rb.request==RB_EMPTY||frame<rb.request)rb.request=frame;
      ++rb.corrections;
    }
  }else if(!znp_core_put_input(&g.core,g.core.local_slot^1,frame,pad))return 0;
  if(rb.latest_actual==RB_EMPTY||frame>=rb.latest_actual){rb.latest_actual=frame;rb.last_pad=pad;}
  return 1;
}
static int rb_predict(uint32_t frame){
  if(!rb_can_advance(frame))return 0;
  RbPred*p=&rb.pred[frame&(ZNP_HISTORY-1)];
  if(p->valid && p->frame==frame)return 1;
  if(p->valid){error_msg("Rollback prediction history exhausted");g.core.fault=1;return 0;}
  if(!znp_core_put_input(&g.core,g.core.local_slot^1,frame,rb.last_pad))return 0;
  p->frame=frame;p->valid=1;rb_recount_oldest();++rb.predictions;
  return 1;
}
static int rb_is_authoritative(uint32_t frame){
  RbPred*p=&rb.pred[frame&(ZNP_HISTORY-1)];
  return (rb.oldest==RB_EMPTY||rb.oldest>frame) &&
         (rb.latest_actual!=RB_EMPTY&&rb.latest_actual>=frame) &&
         !(p->valid&&p->frame==frame) &&
         (rb.request==RB_EMPTY||rb.request>frame);
}
static void rb_record_hash(uint32_t frame,uint64_t hash){
  RbHash*h=&rb.hash[frame&(ZNP_HISTORY-1)];h->frame=frame;h->hash=hash;h->valid=1;
}
static void rb_publish_hashes(uint32_t now){
  if(!g.rollback)return;
  for(int i=0;i<ZNP_HISTORY;i++){
    RbHash*h=&rb.hash[i];
    if(h->valid&&now>=h->frame&&now-h->frame>=g.rollback_window&&rb_is_authoritative(h->frame)){
      znp_core_put_local_hash(&g.core,h->frame,h->hash);h->valid=0;
    }
  }
}
static int rb_apply(uint32_t current){
  if(!g.rollback ||rb.request==RB_EMPTY||rb.request>=current)return 1;
  const uint32_t from=rb.request;rb.request=RB_EMPTY;
  if(current-from>g.rollback_window){error_msg("Rollback depth exceeded negotiated window");return 0;}
  RbState*save=&rb.states[from%rb.slot_count];
  if(!save->valid||save->frame!=from||!zamn_runtime_load_snapshot(s_runtime,save->data,save->size)){
    error_msg("Rollback restore failed: missing frame snapshot");return 0;
  }
  int success=1;
  zamn_runtime_set_replay_suppressed(s_runtime,true);
  for(uint32_t f=from;f<current;++f){
    if(f!=from && !rb_is_authoritative(f) && !rb_save(f)){success=0;break;}
    uint16_t pads[2];if(!znp_core_frame_ready(&g.core,f,pads)){success=0;break;}
    zamn_runtime_set_pad(s_runtime,0,pads[0]&RB_MASK_GAME);
    zamn_runtime_set_pad(s_runtime,1,pads[1]&RB_MASK_GAME);
    ZamnFrameResult result;
    if(!zamn_runtime_frame(s_runtime,NULL,0,&result)){success=0;break;}
    if((f+1u)%ZNP_HASH_PERIOD==0)rb_record_hash(f,zamn_runtime_state_hash(s_runtime));
  }
  zamn_runtime_set_replay_suppressed(s_runtime,false);
  if(!success){error_msg("Rollback frame replay failed");return 0;}
  ++rb.replays;
  SDL_Log("ZAMN R3 rollback corrected frame=%u at=%u predictions=%u corrections=%u replays=%u",
          from,current,rb.predictions,rb.corrections,rb.replays);
  return 1;
}

static uint16_t rd16(const uint8_t*p){return (uint16_t)p[0]|(uint16_t)p[1]<<8;}
static uint32_t rd32(const uint8_t*p){return (uint32_t)rd16(p)|((uint32_t)rd16(p+2)<<16);}
static uint64_t rd64(const uint8_t*p){return (uint64_t)rd32(p)|((uint64_t)rd32(p+4)<<32);}
static void wr16(uint8_t*p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void wr32(uint8_t*p,uint32_t v){wr16(p,(uint16_t)v);wr16(p+2,(uint16_t)(v>>16));}
static void wr64(uint8_t*p,uint64_t v){wr32(p,(uint32_t)v);wr32(p+4,(uint32_t)(v>>32));}
static uint32_t ms(void){return SDL_GetTicks();}
static void error_msg(const char*s){snprintf(g.error,sizeof(g.error),"%s",s);SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"ZAMN CROSSPLAY: %s",s);}
const char *zan_error(void){return g.error[0]?g.error:"Network connection failed";}
void zan_attach_lobby(SDL_Renderer *renderer){zalobby_attach(renderer);}
void zan_detach_lobby(void){zalobby_detach();}
bool zan_was_cancelled(void){return strcmp(g.error,"Cancelled lobby / returned to menu")==0;}
/* This is deliberately a UI-only layer: never emit protocol packets from it. */
static int lobby_update(const char *state,int delay,int window,int rtt,int jitter,int started){
  char room[120],role[100],peer[100];
  const char *title=g.is_host?(g.public_mode?"HOST PUBLIC ROOM":"HOST DIRECT"):
                            (g.public_mode?"JOIN PUBLIC ROOM":"JOIN DIRECT");
  if(g.public_mode){snprintf(room,sizeof(room),"PUBLIC ROOM %s",g.room[0]?g.room:"CONNECTING");}
  else if(g.is_host){snprintf(room,sizeof(room),"LOCAL IP %s  UDP 6464",g.local_ip);}
  else {char ip[INET_ADDRSTRLEN]={0};inet_ntop(AF_INET,&g.peer.sin_addr,ip,sizeof(ip));
        snprintf(room,sizeof(room),"HOST IP %s  UDP 6464",ip);}
  snprintf(role,sizeof(role),"P%d %s / %s",g.is_host?1:2,g.username,
           g.is_host?"HOST":"GUEST");
  if(g.have_peer||g.session){snprintf(peer,sizeof(peer),"P%d %s",g.is_host?2:1,
     g.peer_name[0]?g.peer_name:"CONNECTED");}
  else {snprintf(peer,sizeof(peer),"P%d WAITING",g.is_host?2:1);}
  return zalobby_tick(title,room,role,peer,state,delay,window,rtt,jitter,g.is_host,started,
                      g.is_host?g.host_delay_choice:-2,
                      g.is_host?g.host_rb_choice:-2,g.peer_mobile);
}
static int lobby_cancelled(int flags){
  if(flags&2){error_msg("Cancelled lobby / returned to menu");return 1;}
  return 0;
}

static int same(const struct sockaddr_in*a,const struct sockaddr_in*b){return a->sin_addr.s_addr==b->sin_addr.s_addr && a->sin_port==b->sin_port;}
static int endpoint(struct sockaddr_in*a,const char*ip,unsigned port){memset(a,0,sizeof(*a));a->sin_family=AF_INET;a->sin_port=htons((uint16_t)port);return inet_pton(AF_INET,ip,&a->sin_addr)==1;}
static int send_addr(const void*ptr,int n,const struct sockaddr_in*addr){return g.fd>=0 && sendto(g.fd,ptr,(size_t)n,0,(const struct sockaddr*)addr,sizeof(*addr))==n;}
static int dir_text(const char*s){return send_addr(s,(int)strlen(s),&g.directory);}

bool zan_read_config(ZanConfig*cfg,const char*dir){
  if(!cfg)return false;memset(cfg,0,sizeof(*cfg));strcpy(cfg->mode,"offline");
  cfg->delay_choice=-1;cfg->rollback_choice=-1;
  if(!dir)return false;
  char path[1024];if(snprintf(path,sizeof(path),"%s/zamn_online.conf",dir)>=(int)sizeof(path))return false;
  FILE*f=fopen(path,"rb");if(!f)return false;
  char line[200];while(fgets(line,sizeof(line),f)){
    char *sep=strchr(line,'=');if(!sep)continue;*sep++=0;sep[strcspn(sep,"\r\n")]=0;
#define LOAD(k,member) if(!strcmp(line,k)){snprintf(cfg->member,sizeof(cfg->member),"%s",sep);continue;}
    LOAD("mode",mode) LOAD("host",host) LOAD("room",room) LOAD("username",username) LOAD("token",token)
    if(!strcmp(line,"delay")){cfg->delay_choice=atoi(sep);continue;}
    if(!strcmp(line,"rollback")){cfg->rollback_choice=atoi(sep);continue;}
#undef LOAD
  }fclose(f);
  return !strcmp(cfg->mode,"direct")||!strcmp(cfg->mode,"public")||
         !strcmp(cfg->mode,"host_direct")||!strcmp(cfg->mode,"host_public");
}

static void discover_local_ip(void){
  snprintf(g.local_ip,sizeof(g.local_ip),"0.0.0.0");
  int fd=socket(AF_INET,SOCK_DGRAM,0);if(fd<0)return;
  struct sockaddr_in dst,from;socklen_t len=sizeof(from);
  if(endpoint(&dst,ZDIR_IP,ZDIR_PORT) && connect(fd,(const struct sockaddr*)&dst,sizeof(dst))==0 &&
     getsockname(fd,(struct sockaddr*)&from,&len)==0){
    inet_ntop(AF_INET,&from.sin_addr,g.local_ip,sizeof(g.local_ip));
  }
  close(fd);
}
static void nat_note_direct(void){
  g.last_direct_rx=ms();if(g.direct_count<3)++g.direct_count;
  if(g.direct_count>=3)g.local_direct=1;
  if(g.local_direct&&g.peer_direct)g.direct_ok=1;
}
static void nat_text(const char*s,const struct sockaddr_in*from){
  if(!g.public_mode)return;
  if(same(from,&g.directory)&&!strncmp(s,"ZNAT1|INTRO|",12)){
    char pc[20]={0},ip[32]={0},role[8]={0},lanip[32]={0};unsigned port=0,lp=0;
    int count=sscanf(s,"ZNAT1|INTRO|%19[^|]|%31[^|]|%u|%7[^|]|LAN|%31[^|]|%u",pc,ip,&port,role,lanip,&lp);
    if(count<4)count=sscanf(s,"ZNAT1|INTRO|%19[^|]|%31[^|]|%u|%7s",pc,ip,&port,role);
    if(count>=4 && port>0 && port<65536){
      struct sockaddr_in a;
      /* The Xbox can supply a LAN candidate. Choose it only when reachable on
         the same /24, otherwise retain the public endpoint for NAT/relay. */
      const char*chosen=ip;unsigned chosen_port=port;
      if(count==6 && lp>0 && lp<65536 && inet_pton(AF_INET,lanip,&(struct in_addr){0})==1){
        struct sockaddr_in tmp;endpoint(&tmp,lanip,lp);
        struct in_addr self;memset(&self,0,sizeof(self));inet_pton(AF_INET,g.local_ip,&self);
        if(self.s_addr && (ntohl(tmp.sin_addr.s_addr)&0xffffff00u)==(ntohl(self.s_addr)&0xffffff00u))
          {chosen=lanip;chosen_port=lp;}
      }
      if(endpoint(&a,chosen,chosen_port)){
        int changed=!g.have_intro||!same(&a,&g.peer);
        g.peer=a;g.have_intro=1;snprintf(g.peer_cookie,sizeof(g.peer_cookie),"%s",pc);
        if(changed){g.direct_ok=g.local_direct=g.peer_direct=g.direct_count=0;}
        SDL_Log("ZAMN NAT intro peer %s:%u",chosen,chosen_port);
      }
    }
  }else if(same(from,&g.directory)&&!strncmp(s,"ZNAT1|SELF|",11)){
    char c[20]={0};if(sscanf(s,"ZNAT1|SELF|%19[^|]",c)==1){snprintf(g.cookie,sizeof(g.cookie),"%s",c);g.have_self=1;}
  }else if(same(from,&g.directory)&&!strcmp(s,"ZNAT1|RESET")){
    g.have_intro=0;g.have_self=0;g.direct_ok=0;
  }else if(g.have_intro&&same(from,&g.peer)){
    char cookie[20]={0};
    if(sscanf(s,"ZNAT1|PUNCH|%19s",cookie)==1 && !strcmp(cookie,g.peer_cookie))nat_note_direct();
    if(sscanf(s,"ZNAT1|DIRECTOK|%19s",cookie)==1 && !strcmp(cookie,g.peer_cookie)){
      nat_note_direct();g.peer_direct=1;if(g.local_direct)g.direct_ok=1;
    }
  }
}
static void tick_nat(void){
  if(!g.public_mode)return;
  uint32_t now=ms();char msg[96];
  if(g.is_host&&g.public_mode&&g.have_self&&g.room[0]&&g.registration_token[0]&&now-g.last_heartbeat>=2500){
    snprintf(msg,sizeof(msg),"ZDIR1|HEARTBEAT|%s|%s|%s",g.room,g.registration_token,g.cookie);
    dir_text(msg);g.last_heartbeat=now;
  }
  if(g.have_self && now-g.last_keep>=700){snprintf(msg,sizeof(msg),"ZNAT1|KEEP|%s",g.cookie);dir_text(msg);g.last_keep=now;}
  if(g.have_self&&g.have_intro&&now-g.last_punch>=120){
    snprintf(msg,sizeof(msg),"ZNAT1|PUNCH|%s",g.cookie);send_addr(msg,(int)strlen(msg),&g.peer);
    if(g.local_direct){snprintf(msg,sizeof(msg),"ZNAT1|DIRECTOK|%s",g.cookie);send_addr(msg,(int)strlen(msg),&g.peer);}
    g.last_punch=now;
  }
  if(g.direct_ok && now-g.last_direct_rx>1800){g.direct_ok=g.local_direct=g.peer_direct=g.direct_count=0;}
}
static int make_packet(uint8_t*out,int cap,uint8_t type,uint32_t session,const uint8_t*payload,int count){
  if(count<0 || count>cap-ZNP_HEADER)return 0;
  memcpy(out,"ZNP1",4);out[4]=ZNP_PROTOCOL;out[5]=type;wr16(out+6,(uint16_t)(count+ZNP_HEADER));
  wr32(out+8,ZNP_BUILD);wr32(out+12,session);wr32(out+16,g.seq++);wr32(out+20,g.core.frame);
  if(count)memcpy(out+ZNP_HEADER,payload,(size_t)count);
  return count+ZNP_HEADER;
}
static int send_packet(uint8_t type,uint32_t session,const uint8_t*payload,int n,const struct sockaddr_in*to){
  uint8_t pkt[ZNP_MAX_PACKET],relay[ZNP_MAX_PACKET+32];int bytes=make_packet(pkt,sizeof(pkt),type,session,payload,n);
  if(!bytes)return 0;int sent=send_addr(pkt,bytes,to);
  if(g.public_mode && g.have_self && !g.direct_ok && strlen(g.cookie)==16){
    memcpy(relay,"ZRLY1|",6);memcpy(relay+6,g.cookie,16);relay[22]='|';memcpy(relay+23,pkt,(size_t)bytes);
    if(send_addr(relay,23+bytes,&g.directory))sent=1;
  }
  if(sent)g.last_tx=ms();return sent;
}
static int packet_peer(uint8_t t,const uint8_t*p,int n){return send_packet(t,g.session,p,n,&g.peer);}
/* returns 1 for ZNP packet, 2 for directory text, 0 for none */
typedef struct {uint8_t type;uint32_t session;uint32_t ack;const uint8_t*payload;int len;struct sockaddr_in from;} Packet;
static int read_packet(uint8_t*data,int cap,Packet*p,char*ascii,int ascii_cap){
  for(int k=0;k<16;++k){struct sockaddr_in from;socklen_t flen=sizeof(from);
    int n=(int)recvfrom(g.fd,data,(size_t)cap,0,(struct sockaddr*)&from,&flen);
    if(n<0){if(errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR)return 0;error_msg("UDP socket receive failed");return -1;}
    int via_relay=0;
    if(g.public_mode && same(&from,&g.directory) && n>23 && !memcmp(data,"ZRLY1|",6) && data[22]=='|'){
      if(!g.have_intro || strlen(g.peer_cookie)!=16 || memcmp(data+6,g.peer_cookie,16))continue;
      memmove(data,data+23,(size_t)(n-23));n-=23;from=g.peer;via_relay=1;
    }else if(n && data[0]=='Z' && (g.public_mode || n>5)){
      if(n<cap)data[n]=0;else continue;
      if(g.public_mode)nat_text((const char*)data,&from);
      if(ascii && ascii_cap>0 && g.public_mode && same(&from,&g.directory)&&n>=6&&!memcmp(data,"ZDIR1|",6)){
        snprintf(ascii,(size_t)ascii_cap,"%s",(const char*)data);return 2;
      }
      /* Other server ASCII is not gameplay. */
      if(memcmp(data,"ZNP1",4))continue;
    }
    if(n<ZNP_HEADER || memcmp(data,"ZNP1",4) || data[4]!=ZNP_PROTOCOL || rd16(data+6)!=n || rd32(data+8)!=ZNP_BUILD)continue;
    if(!same(&from,&g.peer)){
      if(!(g.is_host && !g.have_peer && !g.public_mode && data[5]==HELLO))continue;
    }
    p->from=from;p->type=data[5];p->session=rd32(data+12);p->ack=rd32(data+20);p->payload=data+ZNP_HEADER;p->len=n-ZNP_HEADER;
    if(!via_relay && g.public_mode && g.have_intro && same(&from,&g.peer))nat_note_direct();
    g.last_rx=ms();return 1;
  }
  return 0;
}
static int open_socket(void){
  g.fd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(g.fd<0){error_msg("Could not create UDP socket");return 0;}
  struct sockaddr_in local;memset(&local,0,sizeof(local));local.sin_family=AF_INET;local.sin_port=htons(ZNP_PORT);
  local.sin_addr.s_addr=htonl(INADDR_ANY);
  if(bind(g.fd,(const struct sockaddr*)&local,sizeof(local))<0){error_msg("Could not bind UDP port 6464 (in use?)");return 0;}
  int flags=fcntl(g.fd,F_GETFL,0);if(flags<0 || fcntl(g.fd,F_SETFL,flags|O_NONBLOCK)<0){error_msg("Could not configure UDP socket");return 0;}
  int buffer=64*1024;setsockopt(g.fd,SOL_SOCKET,SO_RCVBUF,&buffer,sizeof(buffer));setsockopt(g.fd,SOL_SOCKET,SO_SNDBUF,&buffer,sizeof(buffer));
  endpoint(&g.directory,ZDIR_IP,ZDIR_PORT);discover_local_ip();return 1;
}
static int join_directory(const ZanConfig*c){
  uint32_t begin=ms(),last=0;int joined=0;uint8_t data[512];Packet pkt;char ascii[512],msg[256];
  if(!c->room[0] || !c->token[0]){error_msg("Public rooms require an authenticated profile");return 0;}
  /* Existing Xbox server accepts local-ip/port plus auth token; 0.0.0.0 is
     valid as a LAN hint when the mobile stack cannot identify its Wi-Fi IP. */
  snprintf(msg,sizeof(msg),"ZDIR1|JOIN|%s|%08X|%08X%08X|%s|%u|%s",c->room,
           ZNP_BUILD,(unsigned)(g.rom_hash>>32),(unsigned)g.rom_hash,g.local_ip,ZNP_PORT,c->token);
  while(ms()-begin<10000){
    if(lobby_cancelled(lobby_update(joined?"ROOM FOUND / CONNECTING":"CONTACTING PUBLIC DIRECTORY",
                         -1,0,-1,0,0)))return 0;
    uint32_t now=ms();if(!joined && now-last>=300){dir_text(msg);last=now;}
    tick_nat();int t=read_packet(data,sizeof(data)-1,&pkt,ascii,sizeof(ascii));if(t<0)return 0;
    if(t==2){char room[16]={0},cookie[20]={0};
      if(sscanf(ascii,"ZDIR1|JOINOK|%15[^|]|%19s",room,cookie)==2){
        snprintf(g.room,sizeof(g.room),"%s",room);snprintf(g.cookie,sizeof(g.cookie),"%s",cookie);g.have_self=1;joined=1;
        SDL_Log("ZAMN ZDIR JOINOK room %s",room);
      }else if(!strncmp(ascii,"ZDIR1|ERR|",10)){error_msg(ascii+10);return 0;}
    }
    if(joined && g.have_intro)return 1;
    SDL_Delay(4);
  }
  error_msg("Public room JOIN/INTRO timed out");return 0;
}
static int handshake(ZamnNativeRuntime*rt){
  uint32_t begin=ms(),last_hello=0,last_ready=0;int offered=0,started=0,prepared=0;uint8_t data[ZNP_MAX_PACKET+1],hello[32];Packet p;char ascii[512];
  wr64(hello,g.rom_hash);int name_len=(int)strlen(g.username);if(name_len>12)name_len=12;
  hello[8]=(uint8_t)name_len;memcpy(hello+9,g.username,(size_t)name_len);
  memcpy(hello+9+name_len,"ZA8M",4);
  while(ms()-begin<150000){
    const char *state=started?"HOST STARTED / LOADING GAME":offered?
      "READY / WAITING FOR HOST A":"CONNECTING / WAITING FOR OFFER";
    if(lobby_cancelled(lobby_update(state,offered?g.delay:-1,
          offered?g.rollback_window:0,-1,0,started)))return 0;
    uint32_t now=ms();tick_nat();
    if(!offered && now-last_hello>=250){send_packet(HELLO,0,hello,13+name_len,&g.peer);last_hello=now;}
    if(offered && !started && now-last_ready>=100){packet_peer(READY,NULL,0);last_ready=now;}
    int n=read_packet(data,sizeof(data)-1,&p,ascii,sizeof(ascii));if(n<0)return 0;
    if(n==1){
      if(p.type==REJECT){error_msg("Xbox rejected connection: ROM/build mismatch");return 0;}
      if(p.type==OFFER && p.len>=18 && rd64(p.payload)==g.rom_hash){
        if(!offered){g.session=p.session;g.delay=p.payload[8];g.ruleset=p.payload[16]&3u;
          g.rollback=(p.payload[10]!=0 && p.payload[11]!=0);g.rollback_window=g.rollback?p.payload[11]:0;
          unsigned namebytes=p.payload[17];
          if(namebytes<=12 && p.len>=(int)(18+namebytes)){
            memcpy(g.peer_name,p.payload+18,namebytes);g.peer_name[namebytes]=0;
          }
          g.peer_mobile=(namebytes<=12 && p.len>=(int)(22+namebytes) &&
                         !memcmp(p.payload+18+namebytes,"ZA8M",4));
          if(g.rollback_window>(g.peer_mobile?3:1)){
            error_msg("Host offered rollback depth unsupported by peer capability");return 0;}
          offered=1;SDL_Log("ZAMN OFFER session=%08X delay=%u rules=%u",g.session,g.delay,g.ruleset);
        }packet_peer(READY,NULL,0);last_ready=ms();
      }else if(offered && p.session==g.session && p.type==PING && p.len>=8){packet_peer(PONG,p.payload,8);
      }else if(offered && p.session==g.session && p.type==START && p.len>=7){
        if(p.payload[1]!=2 || p.payload[2]!=0 || p.payload[3]!=1 || (p.payload[6]&3u)!=g.ruleset){error_msg("Xbox START slot/ruleset mismatch");return 0;}
        g.rollback=(p.payload[4]!=0&&p.payload[5]!=0);g.rollback_window=g.rollback?p.payload[5]:0;
        if(g.rollback_window>(g.peer_mobile?3:1)){error_msg("Host requested an unsupported rollback window");return 0;}
        g.delay=p.payload[0];if(g.delay>12){error_msg("Xbox requested invalid delay");return 0;}
        if(!started){
          started=1;SDL_Log("ZAMN START received. Preparing canonical title... ");
          prepared=zamn_runtime_reset_to_title(rt,true)?1:-1;
          if(prepared<0){error_msg("Android canonical title reset failed");return 0;}
          SDL_Log("ZAMN canonical start hash=%08X%08X",(unsigned)(zamn_runtime_state_hash(rt)>>32),(unsigned)zamn_runtime_state_hash(rt));
        }
        packet_peer(START_ACK,NULL,0);packet_peer(BOOT_READY,NULL,0);last_ready=ms();
      }else if(started&&prepared==1&&p.session==g.session && p.type==BOOT_GO){
        if(p.len<4 || (p.payload[3]&3u)!=g.ruleset || (p.payload[1]!=0)!=g.rollback ||
           p.payload[2]!=g.rollback_window){error_msg("Rollback/ruleset mismatch in BOOT_GO");return 0;}
        g.delay=p.payload[0];znp_core_init(&g.core,1,g.delay);g.last_recv_hash=0xffffffffu;
        if(!rb_init(rt))return 0;
        g.active=1;
        SDL_Log("ZAMN Android/Xbox crossplay connected: guest=P2 delay=%u rollback=%u window=%u",g.delay,g.rollback,g.rollback_window);return 1;
      }
    }
    if(started && prepared==1 && ms()-last_ready>=100){packet_peer(START_ACK,NULL,0);packet_peer(BOOT_READY,NULL,0);last_ready=ms();}
    SDL_Delay(2);
  }
  error_msg("Xbox did not finish the multiplayer handshake (timeout)");return 0;
}
/* R8 host. Mirrors Xbox ZNP1 HELLO/OFFER/READY/START/BOOT_GO and ZDIR1
   registration; unmodified Xbox guests see only a 1-frame window. */
static int register_directory(const ZanConfig*c){
  if(!c->token[0]){error_msg("Sign in under PROFILE before hosting public rooms");return 0;}
  uint32_t started=ms(),last=0;char msg[256],reply[512];uint8_t buf[512];Packet pkt;
  snprintf(msg,sizeof(msg),"ZDIR1|REGISTER|%08X|%08X%08X|%u|%s|%u|%s",
           ZNP_BUILD,(unsigned)(g.rom_hash>>32),(unsigned)g.rom_hash,0u,g.local_ip,ZNP_PORT,c->token);
  while(ms()-started<7000u){
    if(lobby_cancelled(lobby_update("REGISTERING PUBLIC ROOM",-1,0,-1,0,0)))return 0;
    uint32_t now=ms();if(now-last>=300){dir_text(msg);last=now;}
    int n=read_packet(buf,sizeof(buf)-1,&pkt,reply,sizeof(reply));if(n<0)return 0;
    if(n==2){char rid[16]={0},token[32]={0},cookie[20]={0};
      if(sscanf(reply,"ZDIR1|REGOK|%15[^|]|%31[^|]|%19s",rid,token,cookie)==3){
        snprintf(g.room,sizeof(g.room),"%s",rid);
        snprintf(g.registration_token,sizeof(g.registration_token),"%s",token);
        snprintf(g.cookie,sizeof(g.cookie),"%s",cookie);g.have_self=1;
        SDL_Log("ZAMN R8 public room registered id=%s",g.room);return 1;
      }
      if(!strncmp(reply,"ZDIR1|ERR|",10)){error_msg(reply+10);return 0;}
    }
    SDL_Delay(4);
  }
  error_msg("Public host registration timed out");return 0;
}
static uint8_t host_coverage(uint32_t rtt,uint32_t jitter,int lan){
  unsigned frames;
  if(lan)frames=rtt<=35?1:rtt<=60?2:rtt<=90?3:4;
  else if(rtt<=85)frames=rtt<=25?1:rtt<=55?2:3;
  else {frames=(rtt*3u+50u)/100u;if(frames<4)frames=4;}
  if(jitter>=20 && frames<12)++frames;
  if(jitter>=50 && frames<12)++frames;
  return (uint8_t)(frames>12?12:frames);
}
static void host_pair(uint32_t rtt,uint32_t jitter,uint8_t *delay,uint8_t *window){
  int lan=!g.public_mode || g.direct_ok;
  int coverage=g.host_delay_choice>=0?g.host_delay_choice:host_coverage(rtt,jitter,lan);
  int rb=g.host_rb_choice;
  if(rb<0){
    /* R13 AUTO always enables at least one predictive frame, even if the
       measured base input delay is only 1F. This trades an extra rollback
       snapshot for snappier controls but never reduces base delay below 1.
       Only Android-to-Android sessions may auto-select 2/3 frames. */
    rb=g.peer_mobile?(coverage<=2?1:coverage==3?2:3):1;
  }
  if(!g.peer_mobile && rb>1)rb=1;
  if(rb>3)rb=3;
  if(rb<0)rb=0;
  // MANUAL delay means a fixed base input-delay value. AUTO considers the
  // chosen rollback window as additional coverage; never double-subtract 1.
  int base=g.host_delay_choice>=0?coverage:coverage-rb;
  if(base<1)base=1;
  if(base>12)base=12;
  *delay=(uint8_t)base;*window=(uint8_t)rb;
}
/* R13 lobby selections are separate from the on-wire negotiation, so AUTO
   and MANUAL are always visible without changing Xbox/server protocol. */
static int host_apply_lobby_actions(int flags){
  int changed=0;
  if(flags&4){g.host_delay_choice=(g.host_delay_choice<0)?12:
                                (g.host_delay_choice==0?-1:g.host_delay_choice-1);changed=1;}
  if(flags&8){g.host_delay_choice=(g.host_delay_choice>=12)?-1:
                                g.host_delay_choice+1;changed=1;}
  if(flags&16){
    const int max_window=g.have_peer && !g.peer_mobile?1:3;
    if(g.host_rb_choice>=max_window)g.host_rb_choice=-1;
    else ++g.host_rb_choice;
    changed=1;
  }
  if(flags&32){g.host_delay_choice=-1;g.host_rb_choice=-1;changed=1;}
  return changed;
}
static int handshake_host(ZamnNativeRuntime*rt){
  uint8_t raw[512],payload[96];Packet p;char ascii[512];
  uint32_t begin=ms(),last_offer=0,last_ping=0,last_start=0,pingid=0;
  uint32_t samples=0,rtt=0,jitter=0;
  int ready=0,ack=0,boot=0,started=0,start_queued=0;
  SDL_Log("ZAMN R8 HOST waiting on UDP 6464, local=%s, publicRoom=%s",g.local_ip,g.room);
  while(ms()-begin<180000u){
    uint8_t view_delay=1,view_window=0;
    if(g.have_peer)host_pair(rtt,jitter,&view_delay,&view_window);
    const char *state=started?"PREPARING GAME / WAITING FOR PEER":
       !g.have_peer?"PLAYER 2 WAITING":!ready?"PLAYER 2 CONNECTING":
       samples<6?"AUTO CALIBRATING / PLEASE WAIT":
       start_queued?"START QUEUED / PREPARING":"PLAYER 2 READY / PRESS A";
    int flags=lobby_update(state,g.have_peer?view_delay:-1,
                           g.have_peer?view_window:0,g.have_peer?(int)rtt:-1,
                           (int)jitter,started);
    if(lobby_cancelled(flags))return 0;
    if(flags&1)start_queued=1;
    /* R13 lobby-only selection controls; subsequent OFFER/START carry the
       standard Xbox protocol 4 values. No new server fields or packets. */
    if(!started && host_apply_lobby_actions(flags))last_offer=0;
    uint32_t now=ms();tick_nat();
    int n=read_packet(raw,sizeof(raw)-1,&p,ascii,sizeof(ascii));if(n<0)return 0;
    if(n==1 && p.type==HELLO && p.len>=9 && rd64(p.payload)==g.rom_hash){
      if(!g.have_peer){
        unsigned chars=p.payload[8];if(chars>12||p.len<(int)(9+chars))continue;
        if(g.public_mode && !g.have_intro)continue;
        g.peer=p.from;g.have_peer=1;
        memcpy(g.peer_name,p.payload+9,chars);g.peer_name[chars]=0;
        g.peer_mobile=p.len>=(int)(13+chars)&&!memcmp(p.payload+9+chars,"ZA8M",4);
        g.session=now ^ (uint32_t)g.rom_hash ^ 0x5a414d4eu;
        if(!g.session)g.session=0x5a4d0101u;
        SDL_Log("ZAMN R8 HOST peer accepted mobile=%d session=%08X",g.peer_mobile,g.session);
      }
    }
    if(n==1 && g.have_peer && p.session==g.session){
      if(p.type==READY)ready=1;
      else if(p.type==START_ACK)ack=1;
      else if(p.type==BOOT_READY)boot=1;
      else if(p.type==PONG && p.len>=8){
        uint32_t sample=now-rd32(p.payload);
        if(sample<2000){uint32_t old=rtt;
          if(!samples)rtt=sample;
          else rtt=(rtt*7u+sample+4u)/8u;
          unsigned diff=sample>old?sample-old:old-sample;
          jitter=(jitter*3u+diff+2u)/4u;
          if(samples<100000)samples++;
        }
      }
    }
    if(g.have_peer){
      uint8_t d=1,w=0;host_pair(rtt,jitter,&d,&w);
      if(!started && now-last_offer>=100){
        wr64(payload,g.rom_hash);payload[8]=d;payload[9]=g.host_delay_choice<0?1:0;
        payload[10]=w!=0;payload[11]=w;wr16(payload+12,(uint16_t)rtt);
        wr16(payload+14,(uint16_t)jitter);payload[16]=g.ruleset;
        int plen=(int)strlen(g.username);if(plen>12)plen=12;
        payload[17]=(uint8_t)plen;memcpy(payload+18,g.username,(size_t)plen);
        memcpy(payload+18+plen,"ZA8M",4);
        packet_peer(OFFER,payload,22+plen);last_offer=now;
      }
      if(!started && now-last_ping>=50){wr32(payload,now);wr32(payload+4,pingid++);
        packet_peer(PING,payload,8);last_ping=now;}
      if(ready && !started && samples>=6 && (start_queued || !zalobby_available())){
        g.delay=d;g.rollback_window=w;g.rollback=w!=0;
        started=zamn_runtime_reset_to_title(rt,true)?1:-1;
        if(started<0){error_msg("Android host canonical reset failed");return 0;}
        SDL_Log("ZAMN R8 negotiated host: delay=%u rb=%u rtt=%u jitter=%u mobile=%d",
                g.delay,g.rollback_window,rtt,jitter,g.peer_mobile);
      }
      if(started && !(ack&&boot) && now-last_start>=120){
        payload[0]=g.delay;payload[1]=2;payload[2]=0;payload[3]=1;
        payload[4]=g.rollback;payload[5]=g.rollback_window;payload[6]=g.ruleset;
        packet_peer(START,payload,7);last_start=now;
      }
      if(started && ack && boot){
        payload[0]=g.delay;payload[1]=g.rollback;payload[2]=g.rollback_window;payload[3]=g.ruleset;
        for(int i=0;i<4;i++)packet_peer(BOOT_GO,payload,4);
        znp_core_init(&g.core,0,g.delay);g.last_recv_hash=UINT32_MAX;
        if(!rb_init(rt))return 0;
        g.active=1;
        SDL_Log("ZAMN R8 HOST connected: P1 delay=%u rb=%u mobile=%d",
                g.delay,g.rollback_window,g.peer_mobile);
        return 1;
      }
    }
    SDL_Delay(3);
  }
  error_msg("Host timed out waiting for player; check same Wi-Fi/UDP 6464 or public directory");return 0;
}

bool zan_connect(const ZanConfig*cfg,uint64_t hash,ZamnNativeRuntime*rt){
  if(!cfg || !rt)return false;
  rb_reset();memset(&g,0,sizeof(g));g.fd=-1;g.rom_hash=hash;g.seq=1;g.last_rx=ms();s_runtime=rt;
  g.is_host=!strcmp(cfg->mode,"host_direct")||!strcmp(cfg->mode,"host_public");
  g.public_mode=!strcmp(cfg->mode,"public")||!strcmp(cfg->mode,"host_public");
  g.host_delay_choice=cfg->delay_choice<0?-1:cfg->delay_choice>12?12:cfg->delay_choice;
  g.host_rb_choice=cfg->rollback_choice<0?-1:cfg->rollback_choice>3?3:cfg->rollback_choice;
  snprintf(g.username,sizeof(g.username),"%s",cfg->username[0]?cfg->username:"ANDROID");
  if(!open_socket())goto bad;
  if(g.is_host){
    if(g.public_mode&&!register_directory(cfg))goto bad;
    if(handshake_host(rt))return true;
  }else{
    if(g.public_mode){if(!join_directory(cfg))goto bad;}
    else if(!endpoint(&g.peer,cfg->host,ZNP_PORT)){error_msg("Invalid Xbox IPv4 address");goto bad;}
    if(handshake(rt))return true;
  }
bad:zan_close();return false;
}
static void pump_inputs(void){
  uint8_t data[ZNP_MAX_PACKET+1];Packet p;char ascii[512];
  tick_nat();for(int i=0;i<40;++i){int n=read_packet(data,sizeof(data)-1,&p,ascii,sizeof(ascii));if(n<=0)break;
    if(n==2)continue;if(p.session!=g.session)continue;
    if(p.type==GOODBYE){error_msg("Xbox player disconnected");g.core.fault=1;g.core.fault_frame=g.core.frame;break;}
    if(p.type==BOOT_GO)continue;
    if(p.type!=INPUT || p.len<24)continue;
    uint8_t slot=p.payload[0],count=p.payload[1];if(slot==(uint8_t)g.core.local_slot || slot>1 || count==0 || count>32 || p.len!=24+count*2)continue;
    uint32_t first=rd32(p.payload+4),hframe=rd32(p.payload+12);
    for(unsigned k=0;k<count;++k){if(g.rollback){if(!rb_remote_input(first+k,rd16(p.payload+24+k*2)))break;}
      else if(!znp_core_put_input(&g.core,g.core.local_slot^1,first+k,rd16(p.payload+24+k*2)))break;}
    if(hframe!=0xffffffffu){
      uint64_t hash=rd64(p.payload+16);
      if(hframe!=g.last_recv_hash){SDL_Log("ZAMN peer checkpoint frame=%u hash=%08X%08X",hframe,(unsigned)(hash>>32),(unsigned)hash);g.last_recv_hash=hframe;}
      znp_core_put_peer_hash(&g.core,hframe,hash);
    }
  }
}
static int send_inputs(uint32_t newest){
  uint8_t payload[24+32*2];uint32_t first=newest>=31?newest-31:0;
  while(first<newest && !znp_core_get_input(&g.core,g.core.local_slot,first,NULL))++first;
  uint8_t count=(uint8_t)(newest-first+1);
  payload[0]=g.core.local_slot;payload[1]=count;wr16(payload+2,0);
  wr32(payload+4,first);wr32(payload+8,g.core.frame);
  uint32_t hf=0xffffffffu;uint64_t hv=0;
  if(znp_core_latest_local_hash(&g.core,&hf,&hv) && hf%ZNP_HASH_PERIOD!=ZNP_HASH_PERIOD-1){hf=0xffffffffu;hv=0;}
  wr32(payload+12,hf);wr64(payload+16,hv);
  for(unsigned i=0;i<count;++i){uint16_t pad;if(!znp_core_get_input(&g.core,g.core.local_slot,first+i,&pad))return 0;wr16(payload+24+i*2,pad);}
  return packet_peer(INPUT,payload,24+count*2);
}
bool zan_prepare_frame(uint32_t frame){
  if(!g.active||g.core.fault||g.core.frame!=frame)return false;
  if(!g.rollback)return true;
  const uint32_t start=ms();uint32_t resend=start;
  for(;;){
    pump_inputs();
    if(!rb_apply(frame)){g.core.fault=1;return false;}
    if(g.core.fault){error_msg("Rollback state/hash fault");return false;}
    if(rb_can_advance(frame))return true;
    if(ms()-start>=ZNP_TIMEOUT_MS){error_msg("Timed out waiting to enter rollback prediction window");return false;}
    if(ms()-resend>=8){send_inputs(g.core.latest_local);resend=ms();}
    SDL_Delay(1);
  }
}
bool zan_frame_inputs(uint32_t frame,uint16_t local,uint16_t*p1,uint16_t*p2){
  if(!g.active || g.core.fault || frame!=g.core.frame)return false;
  const uint32_t target=frame+g.delay;
  if(!znp_core_put_input(&g.core,g.core.local_slot,target,local))return false;
  pump_inputs();send_inputs(target);
  uint32_t begin=ms(),last=begin;uint16_t pads[2];
  while(ms()-begin<ZNP_TIMEOUT_MS){
    pump_inputs();if(g.core.fault){error_msg("Input or state hash desync");return false;}
    if(g.rollback){
      if(!rb_apply(frame)){g.core.fault=1;return false;}
      if(!znp_core_get_input(&g.core,g.core.local_slot^1,frame,NULL)){
        if(!rb_can_advance(frame)){/* must wait for authoritative input */}
        else if(!rb_save(frame)||!rb_predict(frame)){g.core.fault=1;return false;}
      }
    }
    if(znp_core_frame_ready(&g.core,frame,pads)){*p1=pads[0];*p2=pads[1];return true;}
    uint32_t now=ms();if(now-last>=8){send_inputs(target);last=now;}SDL_Delay(1);
  }
  error_msg("Waiting for Xbox input timed out");return false;
}
bool zan_frame_done(uint32_t frame,uint64_t state_hash){
  if(!g.active || g.core.fault || frame!=g.core.frame)return false;
  if(state_hash){SDL_Log("ZAMN Android local checkpoint frame=%u hash=%08X%08X",frame,(unsigned)(state_hash>>32),(unsigned)state_hash);
    if(g.rollback)rb_record_hash(frame,state_hash);
    else znp_core_put_local_hash(&g.core,frame,state_hash);
  }
  znp_core_commit_frame(&g.core,frame);
  rb_publish_hashes(frame);
  if(g.core.fault){error_msg("Xbox/Android state hashes diverged");return false;}
  send_inputs(g.core.latest_local);pump_inputs();rb_publish_hashes(frame);
  if(g.core.fault){error_msg("Xbox/Android state hashes diverged");return false;}
  return true;
}
// Shared online pause/save transport mirrors xbox_netplay.cpp R49.7 API.
uint8_t zan_local_slot(void){return g.active?g.core.local_slot:0xffu;}
uint8_t zan_ruleset(void){return g.ruleset;}
uint64_t zan_rom_hash(void){return g.rom_hash;}
int zan_control_send(uint8_t kind,const void *payload,int bytes){
  if(!g.active||bytes<0||bytes>ZNP_MAX_PACKET-ZNP_HEADER-1)return 0;
  uint8_t b[ZNP_MAX_PACKET-ZNP_HEADER];b[0]=kind;
  if(bytes && payload)memcpy(b+1,payload,(size_t)bytes);
  return packet_peer(CONTROL,b,bytes+1);
}
int zan_control_recv(uint8_t *kind,void *payload,int capacity,uint32_t timeout_ms){
  if(!g.active||!kind||capacity<0)return -1;
  uint32_t begin=ms();uint8_t b[ZNP_MAX_PACKET+1];Packet p;char ascii[512];
  do{
    tick_nat();
    int rc=read_packet(b,sizeof(b)-1,&p,ascii,sizeof(ascii));
    if(rc<0)return -1;
    if(rc==1 && p.session==g.session){
      if(p.type==GOODBYE){error_msg("Peer left during shared pause");return -1;}
      if(p.type==PING && p.len>=8){packet_peer(PONG,p.payload,8);}
      if(p.type==CONTROL && p.len>=1){
        *kind=p.payload[0];int n=p.len-1;int copy=n<capacity?n:capacity;
        if(copy && payload)memcpy(payload,p.payload+1,(size_t)copy);
        return copy;
      }
      // Xbox deliberately discards stale INPUT packets during a shared pause.
      // Redundant input history repairs gaps when both sides resume.
    }
    if(!timeout_ms || ms()-begin>=timeout_ms)return 0;
    SDL_Delay(1);
  }while(1);
}
bool zan_resync_after_shared_load(uint32_t frame){
  if(!g.active)return false;
  uint8_t slot=g.core.local_slot,delay=g.delay;
  znp_core_init(&g.core,slot,delay);g.core.frame=frame;
  for(uint32_t f=frame;f<frame+delay;++f){
    if(!znp_core_put_input(&g.core,0,f,0)||!znp_core_put_input(&g.core,1,f,0))return false;
  }
  g.last_recv_hash=UINT32_MAX;
  if(!rb_init(s_runtime))return false;
  return true;
}
void zan_close(void){
  if(g.fd>=0){
    if(g.active){uint8_t bye[8]={0};wr32(bye+4,g.core.frame);packet_peer(GOODBYE,bye,8);}
    if(g.public_mode&&g.have_self&&g.cookie[0]){
      char msg[80];snprintf(msg,sizeof(msg),"ZDIR1|LEAVE|%s",g.cookie);dir_text(msg);dir_text(msg);
    }
    close(g.fd);g.fd=-1;
  }
  g.active=0;rb_reset();s_runtime=NULL;
}
