#ifndef SDL_H
#define SDL_H
#include <stdint.h>
#define SDL_LOG_CATEGORY_APPLICATION 1
extern uint32_t SDL_GetTicks(void);
extern void SDL_Delay(uint32_t ms);
extern void SDL_Log(const char* format,...);
extern void SDL_LogError(int category,const char* format,...);
#endif
