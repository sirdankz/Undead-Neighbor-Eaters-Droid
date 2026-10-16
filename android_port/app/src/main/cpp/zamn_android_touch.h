#ifndef ZAMN_ANDROID_TOUCH_H
#define ZAMN_ANDROID_TOUCH_H
#include <stdbool.h>
#include <SDL.h>
/* Android-only, persistent, customizable on-screen controller. */
void zat_load(const char* internal_directory);
void zat_touch(SDL_FingerID id,float x,float y,bool down);
void zat_release_all(void);
uint16_t zat_poll(int w,int h,bool gamepad_connected);
void zat_draw(SDL_Renderer* renderer,int w,int h,bool gamepad_connected);
#endif
