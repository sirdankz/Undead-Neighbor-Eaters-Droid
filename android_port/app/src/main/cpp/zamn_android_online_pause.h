#ifndef ZAMN_ANDROID_ONLINE_PAUSE_H
#define ZAMN_ANDROID_ONLINE_PAUSE_H
#include <stdint.h>
#include <SDL.h>
#include "native/runtime.h"
// Mirror of the Xbox R49.7 shared START+BACK menu. Both peers stop the frame.
// 0: peer disconnected/failed gate, 1: resumed, 2: loaded synchronized state.
void zon_init(const char *internal, uint64_t rom_hash, const char *player, int ruleset);
int zon_run(ZamnNativeRuntime *rt, uint32_t *frame, unsigned owner_slot,
            SDL_Renderer *r, SDL_Texture *background, const SDL_Rect *source,
            SDL_AudioDeviceID audio);
#endif
