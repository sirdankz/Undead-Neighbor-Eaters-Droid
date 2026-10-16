/*
 * ZAMN Android R5: offline native/COSIM prototype frontend.
 * User supplies their own ROM through SetupActivity; no game assets are shipped.
 * Uses existing translated ZAMN gameplay and LakeSnes compatibility PPU/APU.
 * SDL handles presentation, sound, gamepads and basic touch input.
 */
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_system.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native/runtime.h"
#include "zamn_android_net.h"
#include "zamn_android_touch.h"
#include "zamn_android_pause.h"
#include "zamn_android_online_pause.h"

enum { FB_W=512, FB_H=480, NATIVE_W=256, NATIVE_H=239, AUDIO_FRAMES=800 };
static uint8_t s_legacy[FB_W * FB_H * 4];
static uint8_t s_fast[NATIVE_W * NATIVE_H * 4];
static int16_t s_sound[AUDIO_FRAMES * 2];

static int load_local_save_player(const char *internal,char *dest,size_t capacity){
    SDL_strlcpy(dest,"LOCAL",capacity);
    if(!internal)return 0;
    char path[1024];SDL_snprintf(path,sizeof(path),"%s/zamn_session.cfg",internal);
    FILE*f=fopen(path,"r");if(!f)return 0;
    int ruleset=0;char line[128];while(fgets(line,sizeof(line),f)){
        if(strncmp(line,"player=",7)==0){
            char *v=line+7;size_t len=strcspn(v,"\r\n");v[len]=0;
            if(len>0&&len<=12){SDL_strlcpy(dest,v,capacity);}
        }else if(strncmp(line,"mode=",5)==0){
            char *v=line+5;
            if(strncmp(v,"normal_savable",14)==0)ruleset=0;
            else if(strncmp(v,"normal_nosave",13)==0)ruleset=1;
            else if(strncmp(v,"hardcore_savable",16)==0)ruleset=2;
            else if(strncmp(v,"hardcore_nosave",15)==0)ruleset=3;
            else ruleset=1; // Unknown modes fail closed for save/load.
        }
    }fclose(f);return ruleset;
}
static uint8_t* load_rom(const char* dir, size_t* out_size) {
    const char* names[] = {"zamn.sfc", "zamn.smc", "Zombies Ate My Neighbors.sfc"};
    for (int i=0;i<3;++i) {
        char filename[1024];
        if (SDL_snprintf(filename, sizeof(filename), "%s/%s", dir, names[i]) >= (int)sizeof(filename)) continue;
        FILE* fp=fopen(filename,"rb");
        if (!fp) continue;
        if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); continue; }
        long length=ftell(fp);
        if (length < 1024*1024 || length > 12*1024*1024) { fclose(fp); continue; }
        rewind(fp);
        uint8_t* bytes=(uint8_t*)malloc((size_t)length);
        if (!bytes) { fclose(fp); return NULL; }
        if (fread(bytes,1,(size_t)length,fp) != (size_t)length) { free(bytes); fclose(fp); continue; }
        fclose(fp);
        *out_size=(size_t)length;
        return bytes;
    }
    return NULL;
}

/* Gamepad mapping is Android-local, loaded once at game launch.
   An action always generates the Xbox-compatible SNES input bits. */
static int g_bindings[12]={0,2,1,100,101,6,4,11,12,13,14,10};
static const unsigned g_action_bits[12]={1u<<0,1u<<1,1u<<8,1u<<12,(1u<<13)|(1u<<0),1u<<3,1u<<2,1u<<4,1u<<5,1u<<6,1u<<7,1u<<10};
static void load_gamepad_mapping(const char*dir){
    if(!dir)return;
    char name[1024];if(SDL_snprintf(name,sizeof(name),"%s/zamn_gamepad.cfg",dir)>=(int)sizeof(name))return;
    FILE*f=fopen(name,"r");if(!f)return;
    char hdr[32];if(!fgets(hdr,sizeof(hdr),f)||strncmp(hdr,"R8",2)){fclose(f);return;}
    for(int i=0;i<12;i++){int code=-1;if(fscanf(f,"%d",&code)!=1)break;
        if((code>=0&&code<=14)||code==100||code==101)g_bindings[i]=code;
    }
    fclose(f);
}
static int pad_control(SDL_GameController*pad,int code){
    if(code==100)return SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_TRIGGERLEFT)>16000;
    if(code==101)return SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_TRIGGERRIGHT)>16000;
    return code>=0&&code<=14&&SDL_GameControllerGetButton(pad,(SDL_GameControllerButton)code);
}
static uint16_t poll_controller(SDL_GameController* pad) {
    if (!pad || !SDL_GameControllerGetAttached(pad)) return 0;
    uint16_t bits=0;
    for(int i=0;i<12;i++)if(pad_control(pad,g_bindings[i]))bits|=(uint16_t)g_action_bits[i];
    // Analog stick movement remains independent of the remappable digital directions.
    const int x=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_LEFTX);
    const int y=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_LEFTY);
    if(x < -16000)bits|=1u<<6;
    if(x > 16000)bits|=1u<<7;
    if(y < -16000)bits|=1u<<4;
    if(y > 16000)bits|=1u<<5;
    return bits;
}

/* Gamepad HUD layout is loaded from an Android-only private config file.
   Native game state and netplay packet formats are not changed. */
static bool wants_password_mode(const char* internal){
    if(!internal)return false;
    char filename[1024];
    if(SDL_snprintf(filename,sizeof(filename),"%s/zamn_boot.cfg",internal)>=(int)sizeof(filename))return false;
    FILE* fp=fopen(filename,"r");if(!fp)return false;
    char text[32]={0};bool yes=fgets(text,sizeof(text),fp)&&strncmp(text,"password",8)==0;
    fclose(fp);return yes;
}
static void launch_rom_password_screen(ZamnNativeRuntime* game){
    if(!zamn_runtime_reset_to_title(game,false)){
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,"R5 password title reset failed; original title remains available");return;
    }
    // Same native title selector the Original Xbox uses: 0=START, 4=PASSWORD.
    zamn_runtime_title_control(game,false,1);
    ZamnFrameResult f;
    zamn_runtime_set_pad(game,0,1u<<3);
    bool ok=zamn_runtime_frame(game,NULL,0,&f);
    zamn_runtime_set_pad(game,0,0);
    SDL_Log("R5 entered ROM password screen: %s",ok?"frame advanced":"frame failed");
}

static SDL_GameController* first_pad(int which) {
    for(int i=0, found=0;i<SDL_NumJoysticks();++i) if(SDL_IsGameController(i)) {
        if(found++ == which) return SDL_GameControllerOpen(i);
    }
    return NULL;
}

int SDL_main(int argc, char** argv) {
    (void)argc; (void)argv;
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"SDL_Init: %s",SDL_GetError());
        return 1;
    }
    const char* dir=SDL_AndroidGetExternalStoragePath();
    if (!dir) { SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"No app storage"); SDL_Quit(); return 2; }
    size_t romsize=0;
    uint8_t* rom=load_rom(dir,&romsize);
    if(!rom) { SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Could not load ZAMN ROM in %s",dir); SDL_Quit(); return 3; }
    /* Keep exact Xbox ROM FNV64 seed (Xbox_main.cpp HashBytes64). */
    uint64_t romhash=UINT64_C(1469598103934665603);
    for(size_t i=0;i<romsize;++i){romhash^=rom[i];romhash*=UINT64_C(1099511628211);}
    ZamnNativeRuntime* game=zamn_runtime_create(rom,(int)romsize,ZAMN_RUNTIME_NATIVE_CUTOVER);
    free(rom);
    if(!game) { SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"ZAMN native core init failed"); SDL_Quit(); return 4; }

    const char* internal=SDL_AndroidGetInternalStoragePath();
    zat_load(internal);
    load_gamepad_mapping(internal);
    if(wants_password_mode(internal)){
        SDL_Log("R5 offline: launching original in-game password entry");
        launch_rom_password_screen(game);
    }
    SDL_Window* window=SDL_CreateWindow("Undead Neighbor Eaters",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
                                       960,540,SDL_WINDOW_FULLSCREEN|SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Renderer* renderer=window?SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC):NULL;
    if(!renderer && window) renderer=SDL_CreateRenderer(window,-1,0);
    SDL_Texture* native=renderer?SDL_CreateTexture(renderer,SDL_PIXELFORMAT_XRGB8888,
                                                   SDL_TEXTUREACCESS_STREAMING,NATIVE_W,NATIVE_H):NULL;
    SDL_Texture* fallback=renderer?SDL_CreateTexture(renderer,SDL_PIXELFORMAT_XRGB8888,
                                                     SDL_TEXTUREACCESS_STREAMING,FB_W,FB_H):NULL;
    if(!native || !fallback) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Video init failed: %s",SDL_GetError());
        if(native)SDL_DestroyTexture(native); if(fallback)SDL_DestroyTexture(fallback);
        if(renderer)SDL_DestroyRenderer(renderer); if(window)SDL_DestroyWindow(window);
        zamn_runtime_destroy(game); SDL_Quit(); return 5;
    }
    SDL_SetTextureBlendMode(native,SDL_BLENDMODE_NONE);
    SDL_SetTextureBlendMode(fallback,SDL_BLENDMODE_NONE);

    SDL_AudioSpec want={0}, have={0};
    want.freq=48000; want.format=AUDIO_S16SYS; want.channels=2; want.samples=1024;
    SDL_AudioDeviceID audio=SDL_OpenAudioDevice(NULL,0,&want,&have,0);
    if (audio) SDL_PauseAudioDevice(audio,0);
    ZanConfig online_cfg;
    const bool online_requested=zan_read_config(&online_cfg,SDL_AndroidGetInternalStoragePath());
    bool online_active=false;
    char save_player[20];
    int session_ruleset=load_local_save_player(internal,save_player,sizeof(save_player));
    zap_init(internal,romhash,save_player,session_ruleset,online_requested);
    zon_init(internal,romhash,online_requested && online_cfg.username[0]?
             online_cfg.username:save_player,session_ruleset);
    if(online_requested) {
        SDL_Log("ZAMN R2 selected crossplay mode=%s ROM=%08X%08X",online_cfg.mode,
                (unsigned)(romhash>>32),(unsigned)romhash);
        zan_attach_lobby(renderer);
        if(!zan_connect(&online_cfg,romhash,game)) {
            const bool cancelled=zan_was_cancelled();
            zan_detach_lobby();
            if(!cancelled)SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"ZAMN Crossplay Connection Failed",
                                                   zan_error(),window);
            SDL_DestroyTexture(native);SDL_DestroyTexture(fallback);
            if(audio)SDL_CloseAudioDevice(audio);
            SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);
            zamn_runtime_destroy(game);SDL_Quit();return 6;
        }
        zan_detach_lobby();
        online_active=true;
        // The host chooses the ruleset; do not trust the offline profile mode.
        zon_init(internal,romhash,online_cfg.username[0]?online_cfg.username:save_player,
                 (int)zan_ruleset());
    }
    SDL_GameController* p1=first_pad(0);
    SDL_GameController* p2=first_pad(1);
    uint64_t total_native=0, total=0;
    const uint64_t hz=SDL_GetPerformanceFrequency();
    uint64_t next=SDL_GetPerformanceCounter();
    bool quit=false;
    bool ever_pressed=false;
    bool pause_was_down=false;
    int online_pause_cooldown=0;
    SDL_Texture *last_texture=native;
    SDL_Rect last_src={0,0,NATIVE_W,224};
    while(!quit) {
        // Offline uses the Android-local pause. Online runs normally except
        // at the Xbox-compatible deterministic START+BACK shared-pause barrier.
        if(online_active && !zan_prepare_frame((uint32_t)total)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"ZAMN prepare/rollback fault: %s",zan_error());
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"ZAMN Rollback Stopped",zan_error(),window);
            break;
        }
        SDL_Event e;
        while(SDL_PollEvent(&e)) {
            switch(e.type) {
                case SDL_QUIT: quit=true; break;
                case SDL_APP_WILLENTERBACKGROUND: if(audio)SDL_PauseAudioDevice(audio,1); zat_release_all(); break;
                case SDL_APP_DIDENTERFOREGROUND:
                    zat_load(internal);load_gamepad_mapping(internal);
                    if(audio && (!zap_visible()||online_active))SDL_PauseAudioDevice(audio,0);
                    break;
                case SDL_CONTROLLERDEVICEADDED:
                case SDL_CONTROLLERDEVICEREMOVED:
                    if(p1) SDL_GameControllerClose(p1);
                    if(p2) SDL_GameControllerClose(p2);
                    p1=first_pad(0); p2=first_pad(1); break;
                case SDL_FINGERDOWN:
                    if(zap_visible()){
                        int action=zap_hit(e.tfinger.x,e.tfinger.y);
                        if(action){int result=zap_run_action(action,game,&total);
                            if(result==2||result==3)zap_open_android_settings(result==2?1:2);
                            if(result==8)quit=true;
                            if(result==1){zat_release_all();if(audio)SDL_PauseAudioDevice(audio,0);}
                            next=SDL_GetPerformanceCounter();
                        }
                    }else zat_touch(e.tfinger.fingerId,e.tfinger.x,e.tfinger.y,true);
                    break;
                case SDL_FINGERMOTION:
                    if(!zap_visible())zat_touch(e.tfinger.fingerId,e.tfinger.x,e.tfinger.y,true);
                    break;
                case SDL_FINGERUP:
                    if(!zap_visible())zat_touch(e.tfinger.fingerId,e.tfinger.x,e.tfinger.y,false);
                    break;
                case SDL_KEYDOWN: {
                    if(zap_visible()){
                        int a=zap_key(e.key.keysym.sym);
                        if(a){int r=zap_run_action(a,game,&total);
                            if(r==2||r==3)zap_open_android_settings(r==2?1:2);
                            if(r==8)quit=true;
                            if(r==1){zat_release_all();if(audio)SDL_PauseAudioDevice(audio,0);}
                            next=SDL_GetPerformanceCounter();
                        }
                    }else if(e.key.keysym.sym==SDLK_ESCAPE && !online_active){
                        zap_set_visible(true);zat_release_all();
                        if(audio)SDL_PauseAudioDevice(audio,1);
                    } // Online never launches the Android-only local settings overlay.
                    break;
                }
                case SDL_CONTROLLERBUTTONDOWN:
                    if(zap_visible()){
                        SDL_Keycode key=0;
                        switch(e.cbutton.button){
                            case SDL_CONTROLLER_BUTTON_A:key=SDLK_RETURN;break;
                            case SDL_CONTROLLER_BUTTON_B:case SDL_CONTROLLER_BUTTON_START:key=SDLK_ESCAPE;break;
                            case SDL_CONTROLLER_BUTTON_DPAD_UP:key=SDLK_UP;break;
                            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:key=SDLK_DOWN;break;
                        }
                        if(key){int a=zap_key(key);if(a){int r=zap_run_action(a,game,&total);
                            if(r==2||r==3)zap_open_android_settings(r==2?1:2);
                            if(r==8)quit=true;
                            if(r==1){zat_release_all();if(audio)SDL_PauseAudioDevice(audio,0);}
                            next=SDL_GetPerformanceCounter();
                        }}
                    }
                    break;
                default: break;
            }
        }
        if(quit)break;
        int touchW=0,touchH=0;SDL_GetRendererOutputSize(renderer,&touchW,&touchH);
        bool pad_connected=p1&&SDL_GameControllerGetAttached(p1);
        if(zap_visible() && !online_active){
            int dw=touchW,dh=(int)(touchW*3.f/4.f);
            if(dh>touchH){dh=touchH;dw=(int)(touchH*4.f/3.f);}
            SDL_Rect d={(touchW-dw)/2,(touchH-dh)/2,dw,dh};
            SDL_SetRenderDrawColor(renderer,0,0,0,255);SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer,last_texture,&last_src,&d);
            zap_draw(renderer,touchW,touchH);SDL_RenderPresent(renderer);
            SDL_Delay(16);continue;
        }
        uint16_t raw_input=poll_controller(p1)|zat_poll(touchW,touchH,pad_connected);
        bool pause_down=(raw_input&(1u<<3))!=0;
        // At the ROM title START must still enter the game. Intercept START only
        // once the original stage HUD establishes we're actually in gameplay.
        bool in_game=zamn_runtime_player_stage_hud(game);
        if(!online_active && in_game && !zap_visible() && pause_down && !pause_was_down){
            zap_set_visible(true);zat_release_all();
            if(audio&&!online_active)SDL_PauseAudioDevice(audio,1);
            if(!online_active)continue;
        }
        pause_was_down=pause_down;
        uint16_t local_input=zap_visible()?0:(in_game && !online_active ?
            (uint16_t)(raw_input&~(1u<<3)):raw_input);
        if(online_active){
            // Xbox input.cpp: START+BACK is an out-of-band deterministic
            // 0x8000 pause request, NOT two SNES buttons. Honor remapped and
            // physical controller Start+Back, plus ST+BK touchscreen chord.
            bool physical_chord=p1 && SDL_GameControllerGetAttached(p1) &&
                SDL_GameControllerGetButton(p1,SDL_CONTROLLER_BUTTON_START) &&
                SDL_GameControllerGetButton(p1,SDL_CONTROLLER_BUTTON_BACK);
            if(((local_input&(1u<<3)) && (local_input&(1u<<2))) || physical_chord){
                local_input=(uint16_t)((local_input&~((1u<<2)|(1u<<3)))|0x8000u);
            }
        }
        if(local_input || poll_controller(p2)) ever_pressed=true;
        uint16_t pad1=local_input,pad2=poll_controller(p2);
        if(online_active && !zan_frame_inputs((uint32_t)total,local_input,&pad1,&pad2)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"ZAMN frame input fault: %s",zan_error());
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"ZAMN Crossplay Stopped",zan_error(),window);
            break;
        }
        if(online_active){
            if(online_pause_cooldown>0)--online_pause_cooldown;
            if(((pad1|pad2)&0x8000u) && online_pause_cooldown==0){
                uint32_t shared_frame=(uint32_t)total;
                int pause_result=zon_run(game,&shared_frame,(pad1&0x8000u)?0u:1u,
                                         renderer,last_texture,&last_src,audio);
                if(!pause_result){
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                                 "Xbox shared pause failed: peer controls/barrier mismatch");
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                      "ZAMN Shared Online Pause Failed",
                      "Both devices must synchronize before resuming or loading.",window);
                    break;
                }
                online_pause_cooldown=15;
                zat_release_all();
                next=SDL_GetPerformanceCounter();
                if(pause_result==2){total=shared_frame;continue;}
            }
        }
        // Xbox masks the two system-chord bits before entering SNES gameplay.
        zamn_runtime_set_pad(game,0,(uint16_t)(pad1&0x1fffu));
        zamn_runtime_set_pad(game,1,(uint16_t)(pad2&0x1fffu));
        ZamnFrameResult frame;
        if(!zamn_runtime_frame(game,s_fast,NATIVE_W*4,&frame)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Native runtime returned failure"); break;
        }
        ++total; total_native += frame.native_video ? 1u : 0u;
        // Offline-only, ROM-derived 256x224 title frame for the Xbox-style Android launcher.
        // Capture at frame 120 only if there has been no controller/touch input.
        // Saves raw XRGB pixels in app-local storage; no bundled proprietary artwork.
        if(!online_active && total==120 && !ever_pressed && frame.native_video && frame.width==256 && frame.height>=224){
            char outpath[1024];
            if(SDL_snprintf(outpath,sizeof(outpath),"%s/zamn_title_256x224.xrgb",dir)<(int)sizeof(outpath)){
                FILE* out=fopen(outpath,"wb");
                if(out){fwrite(s_fast,4,NATIVE_W*224,out);fclose(out);SDL_Log("R4 captured ROM-derived menu backdrop");}
            }
        }
        SDL_Texture* texture=fallback;
        SDL_Rect src={0,16,FB_W,448};
        if(frame.native_video && frame.width>0 && frame.width<=NATIVE_W &&
           frame.height>0 && frame.height<=NATIVE_H) {
            texture=native; src.x=0; src.y=0; src.w=frame.width; src.h=frame.height;
            SDL_UpdateTexture(texture,NULL,s_fast,NATIVE_W*4);
        } else {
            zamn_runtime_copy_legacy_frame(game,s_legacy,FB_W*4,NULL,0,FB_H);
            SDL_UpdateTexture(texture,NULL,s_legacy,FB_W*4);
        }
        last_texture=texture;last_src=src;
        int w=0,h=0; SDL_GetRendererOutputSize(renderer,&w,&h);
        int draww=w, drawh=(int)(w*3.0f/4.0f);
        if(drawh>h){drawh=h;draww=(int)(h*4.0f/3.0f);}
        SDL_Rect dst={(w-draww)/2,(h-drawh)/2,draww,drawh};
        SDL_SetRenderDrawColor(renderer,0,0,0,255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer,texture,&src,&dst);
        if(!zap_visible())zat_draw(renderer,w,h,pad_connected);
        if(zap_visible())zap_draw(renderer,w,h);
        SDL_RenderPresent(renderer);
        if(audio) {
            zamn_runtime_audio(game,s_sound,AUDIO_FRAMES);
            // Prevent runaway latency when a frame takes longer than 16.7 ms.
            if(SDL_GetQueuedAudioSize(audio)>48000u*4u/4u) SDL_ClearQueuedAudio(audio);
            SDL_QueueAudio(audio,s_sound,sizeof(s_sound));
        }
        if((total%120)==0) SDL_Log("ZAMN R1 frames=%llu native_video=%llu native_calls_last=%llu",
                                   (unsigned long long)total,(unsigned long long)total_native,
                                   (unsigned long long)frame.native_calls);
        if(online_active) {
            uint64_t sync_hash=0;
            /* Same checkpoint timing and runtime hashing as Xbox R49.9.7. */
            if(((total)%30)==0) sync_hash=zamn_runtime_state_hash(game);
            if(!zan_frame_done((uint32_t)(total-1),sync_hash)) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"ZAMN sync fault: %s",zan_error());
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"ZAMN Crossplay Desync",zan_error(),window);
                break;
            }
        }
        next+=hz/60;
        uint64_t now=SDL_GetPerformanceCounter();
        if(now<next) {
            unsigned ms=(unsigned)((next-now)*1000/hz);
            if(ms>1)SDL_Delay(ms-1);
        } else if(now>next+hz/8) next=now;
    }
    if(online_active)zan_close();
    if(audio)SDL_CloseAudioDevice(audio);
    if(p1)SDL_GameControllerClose(p1);
    if(p2)SDL_GameControllerClose(p2);
    SDL_DestroyTexture(native); SDL_DestroyTexture(fallback);
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window);
    zamn_runtime_destroy(game); SDL_Quit();
    return 0;
}
