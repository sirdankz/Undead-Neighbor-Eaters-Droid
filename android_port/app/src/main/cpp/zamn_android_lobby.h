#ifndef ZAMN_ANDROID_LOBBY_H
#define ZAMN_ANDROID_LOBBY_H
typedef struct SDL_Renderer SDL_Renderer;
/* Android-only pre-game lobby compositor. No ZNP/console/server changes. */
void zalobby_attach(SDL_Renderer *renderer);
void zalobby_detach(void);
int zalobby_available(void);
/* 1=Start 2=Back 4/8=Delay -/+ 16=cycle RB 32=reset both AUTO. */
int zalobby_tick(const char *title, const char *room, const char *role,
                 const char *peer, const char *status, int delay, int rollback,
                 int rtt, int jitter, int can_start, int started,
                 int delay_choice, int rb_choice, int mobile_peer);
#endif
