#ifndef ZAMN_ANDROID_NET_H
#define ZAMN_ANDROID_NET_H
#include <stdint.h>
#include <stdbool.h>
typedef struct SDL_Renderer SDL_Renderer;
#include "native/runtime.h"

typedef struct {
    char mode[16];            /* offline | direct | public */
    char host[64];            /* Xbox host LAN IPv4, direct only */
    char room[16];            /* existing public room ID */
    char username[16];        /* directory display name */
    int delay_choice;        /* -1 AUTO, otherwise selected base input delay */
    int rollback_choice;     /* -1 AUTO, otherwise 0..3 */
    char token[40];           /* R49.9 auth token; private app storage */
} ZanConfig;

/* Draw a mobile-friendly Xbox-style lobby while waiting for network handshakes. */
void zan_attach_lobby(SDL_Renderer *renderer);
void zan_detach_lobby(void);
/* True when the player chose the lobby Back/Cancel control. */
bool zan_was_cancelled(void);
bool zan_read_config(ZanConfig *cfg, const char *internal_dir);
bool zan_connect(const ZanConfig *cfg, uint64_t rom_hash, ZamnNativeRuntime *rt);
/* R3 pre-input rollback check; must run before sampling this frame. */
bool zan_prepare_frame(uint32_t frame);
/* Returns true with Xbox-compatible P1/P2 pads, or false on timeout/hash mismatch. */
bool zan_frame_inputs(uint32_t frame, uint16_t local, uint16_t *p1, uint16_t *p2);
bool zan_frame_done(uint32_t frame, uint64_t state_hash);
// Xbox R49.7 CONTROL channel used only while BOTH peers are stopped at a shared
// deterministic Start+Back pause frame. Never consume these from the normal pump.
uint8_t zan_local_slot(void);
uint8_t zan_ruleset(void);
uint64_t zan_rom_hash(void);
int zan_control_send(uint8_t kind,const void *payload,int bytes);
// Returns received data size (>0), 0 when poll expires, -1 on disconnect.
int zan_control_recv(uint8_t *kind,void *payload,int capacity,uint32_t timeout_ms);
// Restore lockstep state only after both peers committed the same snapshot.
bool zan_resync_after_shared_load(uint32_t frame);
void zan_close(void);
const char *zan_error(void);
#endif
