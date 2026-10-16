#ifndef ZAMN_ANDROID_PAUSE_H
#define ZAMN_ANDROID_PAUSE_H
#include <stdint.h>
#include <stdbool.h>
#include <SDL.h>
#include "native/runtime.h"

// OFFLINE-only overlay. Online START+BACK uses zamn_android_online_pause.h
// and the Xbox R49.7 deterministic synchronized save/pause protocol.
void zap_init(const char *internal, uint64_t rom_hash, const char *player, int ruleset, bool online);
void zap_set_visible(bool on);
bool zap_visible(void);
void zap_draw(SDL_Renderer *rend, int width, int height);
// 1 resume, 2 touch editor, 3 gamepad remap, 4/6 save, 5/7 load, 8 exit.
int zap_hit(float nx, float ny);
int zap_key(SDL_Keycode key);
int zap_run_action(int action, ZamnNativeRuntime *rt, uint64_t *game_frame);
const char *zap_status(void);
void zap_open_android_settings(int mode);
#endif
