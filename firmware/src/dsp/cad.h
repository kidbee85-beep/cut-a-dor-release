/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
#define CAD_FS 44100u
#define CAD_TRACKS 8
#define CAD_STEPS 32
#define CAD_VOICES 16
#define CAD_EVENTS 256
#define CAD_BANK_SIZE 81920u
#define CAD_BANK_HEADER 512u
#define CAD_BANK_MAGIC 0x42444143u
#define CAD_PROJECT_MAGIC 0x50444143u
#define CAD_VERSION 5u
#define CAD_PPQN 96u
#define CAD_BAR 384u
#define CAD_RING 32768u
#if defined(EN_HOST) || defined(CAD_WASM)
#define CAD_POOL
#else
#define CAD_POOL __attribute__((section(".pool")))
#endif
enum { C_KICK,C_SNARE,C_HAT,C_OPEN,C_CLAP,C_PERC,C_RIM,C_EXTRA };
enum { FX_ROLL,FX_STUTTER,FX_REVERSE,FX_BRAKE,FX_FILTER,FX_ECHO,FX_CRUSH,FX_GATE };
enum { P_BPM,P_SWING,P_EVOLVE,P_FILTER,P_STYLE,P_DENSITY,P_HUMAN,P_SPACE,P_KIT,P_DUST,
       P_FXRATE,P_FXDEPTH,P_TAPE_BARS,P_GRID,P_BASS,P_ROOT,P_AUTO,P_BREAK,P_VOLUME,
       P_SYNC,P_TAPE_MIX,P_BREAKLEVEL,P_NOISE,P_HIGHPASS,P_NPARAM };
enum { A_PLAY=1,A_PARAM,A_NEW,A_FX,A_RECORD,A_CLEAN,A_UNDO,A_PART,A_FILL,A_LOCK,A_MUTE,
       A_PAD,A_SLICE,A_MODE,A_TAPE_TOGGLE,A_CLOCK,A_START,A_STOP,A_EXPORT,A_IMPORT,A_MAP,A_PANIC,A_MUTATE,A_STEP,A_REROLL,
       A_CONTINUE,A_MIDI_CONFIG,A_SILENCE,A_STEP_VALUE,A_STEP_DELAY,A_TRACK_LEVEL,A_PUNCH,A_SETTING,A_PARAM_DELTA,A_MIX_DELTA,A_STEP_DELTA,A_SETTING_DELTA,A_QUIET };
enum { S_LIGHT,S_GLOW,S_MOTION,S_SPLASH,S_NSETTING };
typedef struct { uint32_t off,frames; uint16_t rate; uint8_t category,reserved; char name[16]; } cad_zone;
typedef struct { uint32_t magic; uint16_t version,count; uint32_t data_len,data_crc,header_crc;
                 char name[12]; cad_zone zone[16]; uint8_t reserved[32]; } cad_bank;
_Static_assert(sizeof(cad_bank)==CAD_BANK_HEADER,"bank layout");
typedef struct { const uint8_t *data; uint32_t frames; uint16_t rate; uint8_t category; const char *name; } cad_sample;
typedef struct { uint8_t velocity[CAD_TRACKS][CAD_STEPS]; int8_t delay[CAD_TRACKS][CAD_STEPS]; } cad_pattern;
typedef struct { uint16_t tick; uint8_t kind,id; uint16_t value; } cad_event;
typedef struct {
    uint32_t magic,version,seed;
    int16_t par[P_NPARAM];
    uint16_t map[16];
    uint8_t locks,mutes,part,midi_config; /* bit0 USB clock out; bits1..5 input:0 any,1..16 channel,17 clock only */
    cad_pattern pattern[2];
    uint16_t count,tape_bars;
    cad_event event[CAD_EVENTS];
    uint8_t track_level[8],punch,reserved[3]; /* v4 tail; earlier field offsets stay stable */
    uint8_t setting[S_NSETTING]; /* v5: lights, glow, motion, splash */
} cad_project;
_Static_assert(sizeof(cad_project)<=3840,"project fits atomic flash object");
typedef struct {
    uint32_t tick,bar,frames,peak,queue_overflows,record_overflows,clock_losses;
    uint16_t fx_mask,event_count;
    uint8_t playing,part,pending_part,step,record,mode,locks,mutes,tape_on,dirty;
    uint8_t hits[8],section,midi_config;
    uint32_t midi_out_overflows;
    int16_t par[P_NPARAM];
    uint32_t seed;
    cad_pattern pattern; /* audio-owned UI copy; never read mutable project from UI */
    uint8_t track_level[8],punch,setting[S_NSETTING];
} cad_status;
extern volatile cad_status cad_view;
extern cad_project cad_transfer; /* snapshot/import mailbox, owned after acknowledged A_EXPORT */
extern volatile uint32_t cad_transfer_ack;
void cad_init(void);
int cad_command(int type,int id,int value); /* main thread -> audio, bounded SPSC queue */
void cad_render(int32_t *stereo,uint32_t frames,uint32_t gain_q12);
int cad_validate_project(const cad_project *p);
int cad_upgrade_transfer(unsigned bytes); /* validate v5 or migrate v1/v2/v3/v4 in place */
uint32_t cad_crc32(const void *data,uint32_t n);
int cad_validate_bank(const uint8_t *data,uint32_t n);
int cad_set_bank(int slot,const uint8_t *data,uint32_t n); /* stopped audio only */
int cad_sample_get(unsigned id,cad_sample *out);
unsigned cad_factory_count(void);
const char *cad_style_name(int style);
const char *cad_kit_name(int kit);
const char *cad_fx_name(int fx);
int cad_parameter_limit(int p,int value);
int cad_step_tick(const cad_project *p,int part,int track,int step);
void cad_generate(cad_project *p,int part,uint32_t seed);
void cad_mutate(cad_project *p,int part,uint32_t seed);
const cad_project *cad_project_read(void); /* stopped host tests only */

unsigned cad_break_count(void);
unsigned cad_break_sample(unsigned index);
/* UI producer -> audio commands. Source1 USB,2 TRS; receive time is wrap-safe ms. */
void cad_midi_receive(uint32_t packet,unsigned source,uint32_t received_ms);
/* Audio producer -> platform scheduled output. Absolute engine frame timestamps. */
int cad_midi_out_take(uint32_t *packet,uint32_t *frame);
void cad_midi_out_fault(void); /* audio owner: stop and disable a stalled output */
