/* SPDX-License-Identifier: GPL-3.0-only
 * Cut-A-Dor rhythm, sampler and performance engine. No heap, no blocking calls in audio. */
#include "cad.h"
#include "fastmath.h"
#if defined(EN_HOST) || defined(CAD_WASM)
#include <string.h>
#else
/* Freestanding pi32v2 target: implementations are in ../libc.c. */
void *memcpy(void *dst, const void *src, unsigned n);
void *memset(void *dst, int value, unsigned n);
#endif
#include "cad_samples.h"

static const char *styles[]={"BOOM BAP","LO-FI","FUNK BREAK","HALF TIME","HIP-HOP","FAST BREAK"};
static const char *kits[]={"DUSTY DOJO","CONCRETE","VINYL FIST","NIGHT SHIFT","IRON CRATE","SIDEWALK",
 "WARM STATIC","SUBWAY","DEEP POCKET","ROOFTOP","TAPE GHOST","STREET CUT","USER BANK 1","USER BANK 2","USER BANK 3"};
static const char *fxnames[]={"ROLL","STUTTER","REVERSE","BRAKE","FILTER","ECHO","CRUSH","GATE"};
const char *cad_style_name(int s){return styles[(unsigned)s%6];}
const char *cad_kit_name(int s){return kits[(unsigned)s%15];}
const char *cad_fx_name(int s){return fxnames[(unsigned)s%8];}
static cad_project project,undo_project;
cad_project cad_transfer;
volatile uint32_t cad_transfer_ack;
volatile cad_status cad_view;
static const uint8_t *bank_data[3];
static uint16_t break_ids[176];static unsigned break_cached;static uint8_t break_dirty=1;
typedef struct {uint8_t type,id; int32_t value;} command;
static command queue[128];
static volatile uint32_t qr,qw;
#define BARRIER() __asm__ volatile("" ::: "memory")
static uint32_t rng(uint32_t *s){uint32_t x=*s; x^=x<<13;x^=x>>17;x^=x<<5;return *s=x?x:0x1cba374du;}
static int clip(int v,int lo,int hi){return v<lo?lo:v>hi?hi:v;}
int cad_parameter_limit(int p,int v){
 static const int16_t lo[P_NPARAM]={40,50,0,0,0,0,0,0,0,0,0,0,1,0,0,24,0,0,0,0,0,0,0,0};
 static const int16_t hi[P_NPARAM]={220,70,100,100,5,100,100,100,14,100,4,100,8,2,100,60,1,59,100,2,100,100,100,100};
 return (unsigned)p<P_NPARAM?clip(v,lo[p],hi[p]):0;
}
uint32_t cad_crc32(const void *data,uint32_t n){const uint8_t *b=data;uint32_t c=~0u;while(n--){c^=*b++;for(int k=0;k<8;k++)c=(c>>1)^((0u-(c&1u))&0xedb88320u);}return ~c;}
int cad_validate_bank(const uint8_t *data,uint32_t n){
 if(!data||n<CAD_BANK_HEADER||n>CAD_BANK_SIZE)return -1;
 cad_bank h;memcpy(&h,data,sizeof h);
 if(h.magic!=CAD_BANK_MAGIC||h.version!=1||!h.count||h.count>16||h.data_len>CAD_BANK_SIZE-CAD_BANK_HEADER||h.data_len>n-CAD_BANK_HEADER)return -2;
 uint32_t crc=h.header_crc;h.header_crc=0;if(cad_crc32(&h,sizeof h)!=crc)return -3;
 if(cad_crc32(data+CAD_BANK_HEADER,h.data_len)!=h.data_crc)return -4;
 for(unsigned i=0;i<h.count;i++){cad_zone *z=&h.zone[i];uint32_t blocks=(z->frames+255u)/256u;
  if(!z->frames||z->frames>24u*24000u||z->rate<8000||z->rate>24000||z->category>8||z->off>h.data_len||blocks>(h.data_len-z->off)/132u||z->name[15])return -5;
  for(uint32_t b=0;b<blocks;b++)if(data[CAD_BANK_HEADER+z->off+b*132+2]>88)return -6;
 }return 0;
}
int cad_set_bank(int slot,const uint8_t *data,uint32_t n){if((unsigned)slot>2||cad_view.playing)return -1;bank_data[slot]=0;break_dirty=1;if(!data)return 0;int r=cad_validate_bank(data,n);if(!r)bank_data[slot]=data;return r;}
unsigned cad_factory_count(void){return CAD_FACTORY_COUNT;}
int cad_sample_get(unsigned id,cad_sample *out){
 if(id<CAD_FACTORY_COUNT){*out=cad_factory[id];return 1;}
 if(id<128||id>=176)return 0;
 unsigned slot=(id-128)/16,index=(id-128)%16;const uint8_t *b=bank_data[slot];if(!b)return 0;
 const cad_bank *h=(const cad_bank*)b;if(index>=h->count)return 0;const cad_zone *z=&h->zone[index];
 out->data=b+CAD_BANK_HEADER+z->off;out->frames=z->frames;out->rate=z->rate;out->category=z->category;out->name=z->name;return 1;
}
static void scan_breaks(void){if(!break_dirty)return;break_cached=0;cad_sample s;
 for(unsigned i=0;i<176;i++)if(cad_sample_get(i,&s)&&s.category==8)break_ids[break_cached++]=(uint16_t)i;
 break_dirty=0;
}
unsigned cad_break_count(void){scan_breaks();return break_cached;}
unsigned cad_break_sample(unsigned index){scan_breaks();return break_cached?break_ids[index%break_cached]:0;}
/* Curated by category, deliberately paired: acoustic kicks/snares/hats lead the crate.
 * Stable factory indices preserve the categories of existing project assignments. */
static const uint8_t kit_map[12][8]={
 {4,18,31,29,41,56,42,47},{5,19,33,38,40,57,43,48},
 {6,20,34,29,45,58,44,49},{7,21,35,38,41,59,42,47},
 {8,22,31,29,40,60,43,48},{9,23,33,38,46,55,44,49},
 {11,26,39,29,41,56,42,47},{12,27,28,38,45,61,43,48},
 {0,18,34,29,40,57,44,49},{3,19,32,38,46,58,42,47},
 {13,20,35,29,41,62,43,48},{2,21,30,38,40,63,44,49}
};
static void map_kit(void){int k=project.par[P_KIT];for(int i=0;i<16;i++){
 unsigned id=kit_map[((k<12?k:0)+(i/8))%12][i%8];
 if(k>=12){unsigned found[16],n=0;cad_sample smp;for(unsigned j=0;j<16;j++){unsigned candidate=128+(unsigned)(k-12)*16+j;if(cad_sample_get(candidate,&smp)&&smp.category==i%8)found[n++]=candidate;}if(n)id=found[(unsigned)(i/8)%n];}
 project.map[i]=(uint16_t)id;
}}
/* Sixteen two-bar kick conversations: the second bar answers the first.
 * These are rhythm vocabulary, not sampled performances. Seeded decoration is
 * repeatable; no random work runs per sample. */
static const uint16_t kick_a[16]={0x0441,0x4209,0x0101,0x0881,0x0483,0x8449,0x0841,0x0401,0x0241,0x2081,0x0821,0x0429,0x4009,0x1081,0x0405,0x8201};
static const uint16_t kick_b[16]={0x4481,0x0841,0x8401,0x8405,0x8141,0x4905,0x4009,0x44c1,0x8421,0x0249,0x8801,0x4181,0x0489,0x8409,0x4241,0x0485};
static const uint16_t hats[8]={0x5555,0x4545,0x5545,0x454d,0x5555,0x6565,0x5955,0x555d};
static int anchor_at(const cad_project *p,int t,int st){return t==C_KICK?st%16==0:t==C_SNARE?(p->par[P_STYLE]==3?st%16==8:st%8==4):0;}
static int scratch_gap(const cad_project *p,int t,int st){int n=p->par[P_SPACE]>=75?8:p->par[P_SPACE]>=35?4:0;return t>=C_HAT&&n&&st>=32-n;}
/* LIVE pocket timing in 96-PPQN ticks. HUMAN=0 leaves manual nudges only.
 * Signed hats lean forward; backbeats lay back. Kicks keep the downbeat firm.
 * At 92 BPM one tick is 6.79 ms. No accumulated timing drift. */
int cad_step_tick(const cad_project *p,int part,int t,int st){
 if((unsigned)part>1||(unsigned)t>=8||(unsigned)st>=32)return -1;
 int style=p->par[P_STYLE],human=p->par[P_HUMAN],j=st%16;
 int feel=0,ghost=p->pattern[part].velocity[t][st]<65;
 if(t==C_KICK)feel=j%4==0?0:(style==2?-2:style==1?2:1);
 else if(t==C_SNARE)feel=ghost?-2:(style==1?6:style==3?4:style==5?1:3);
 else if(t==C_HAT)feel=j%2?(style==1?4:2):(j%4==2?-3:-1);
 else if(t==C_CLAP)feel=4;
 else feel=((st*3+t*5+(int)(p->seed&3))%7)-3;
 int swing=(st%2?(p->par[P_SWING]-50)*48/100:0);
 int offset=feel*human/100+p->pattern[part].delay[t][st];
 return (st*24+swing+offset+768)%768;
}
void cad_generate(cad_project *p,int part,uint32_t seed){
 if((unsigned)part>1)return;uint32_t s=seed?seed:1;cad_pattern *q=&p->pattern[part];
 int style=p->par[P_STYLE],dens=p->par[P_DENSITY],family=(int)(seed%16u);
 s^=(uint32_t)(style+1)*0x85ebca6bu;
 unsigned f=(unsigned)(family+style*3)%16;
 uint32_t km=(uint32_t)kick_a[f]|((uint32_t)kick_b[f]<<16),hm=(uint32_t)hats[f%8]|((uint32_t)hats[(f+3)%8]<<16);
 if(style==2||style==5){km|=1u<<((family&1)?14:6);hm|=family&1?0x80802020u:0x22000808u;}
 if(style==3){km&=0x81018101u;hm&=0x55555555u;}
 km|=1u|(1u<<16); /* kick on step 1 of each bar */
 for(int t=0;t<8;t++){if(p->locks&(1u<<t))continue;for(int st=0;st<32;st++){
  int j=st%16,on=0,prob=0,vel=0;uint32_t rv=rng(&s),chance=rng(&s)%100u;
  if(t==C_KICK){on=(km>>st)&1u;prob=(j==3||j==10||j==15)?dens/8:0;vel=on?(j%4?88+(int)(rv%13):114):65;}
  else if(t==C_SNARE){on=anchor_at(p,t,st);prob=(j==3||j==11)?dens/2:(j==7||j==15)?dens/3:0;if(style==2||style==5)prob=prob*3/2;vel=on?(j==12?105:116):24+(int)(rv%22);}
  else if(t==C_HAT){on=(hm>>st)&1u;prob=j%2?dens/3:dens/12;vel=on?(j%4==0?78:j%4==2?51:35):29;}
  else if(t==C_OPEN){prob=((j+family)%8==6)?dens:0;vel=58;}
  else if(t==C_CLAP){prob=anchor_at(p,C_SNARE,st)&&((st/8+family)%3==0)?dens:0;vel=49;}
  else if(t==C_PERC){prob=j%4==(family+3)%4?(st>=16?dens:dens/3):0;vel=44+(int)(rv%23);}
  else if(t==C_RIM){prob=j==(family*3+7)%16?dens:0;vel=52;}
  else {prob=st==26+family%4?dens/2:0;vel=47;}
  on=on||(int)chance<prob;if(scratch_gap(p,t,st))on=0;
  q->velocity[t][st]=(uint8_t)(on?clip(vel+(int)(rv%11)-5,1,127):0);
  /* Generated pocket lives in the live HUMAN control; reserve stored delay
   * for deliberate per-hit edits so they survive regeneration of other lanes. */
  q->delay[t][st]=0;
 }}
}
void cad_mutate(cad_project *p,int part,uint32_t seed){
 if((unsigned)part>1)return;uint32_t s=seed?seed:1;cad_pattern *q=&p->pattern[part];
 uint32_t touched[8]={0};unsigned need=2+rng(&s)%4u,done=0;
 for(unsigned k=0;k<256&&done<need;k++){
  int t=(int)(rng(&s)%8u),st=(int)(rng(&s)%32u);uint32_t bit=1u<<st;
  if((p->locks&(1u<<t))||(touched[t]&bit)||anchor_at(p,t,st)||scratch_gap(p,t,st))continue;
  touched[t]|=bit;q->velocity[t][st]=q->velocity[t][st]?0:(uint8_t)(t==C_KICK?88:t==C_SNARE?35:45+rng(&s)%25u);done++;
 }
}
static unsigned pattern_distance(const cad_project *p,int a,int b){unsigned n=0;
 for(int t=0;t<8;t++)if(!(p->locks&(1u<<t)))for(int st=0;st<32;st++)
  if(!!p->pattern[a].velocity[t][st]!=!!p->pattern[b].velocity[t][st])n+=(unsigned)(t==C_KICK?3:t==C_SNARE?4:t==C_HAT?1:2);
 return n;
}
int cad_validate_project(const cad_project *p){
 if(p->magic!=CAD_PROJECT_MAGIC||p->version!=CAD_VERSION||p->part>1||p->count>CAD_EVENTS||p->tape_bars<1||p->tape_bars>8||(p->midi_config>>1)>17)return -1;
 for(int k=0;k<8;k++)if(p->track_level[k]>200)return -8;
 if(p->punch>100||p->reserved[0]||p->reserved[1]||p->reserved[2])return -8;
 for(int k=0;k<S_NSETTING;k++)if(p->setting[k]>(k<2?3:1))return -8;
 for(int k=0;k<P_NPARAM;k++)if(cad_parameter_limit(k,p->par[k])!=p->par[k])return -2;
 for(int k=0;k<16;k++)if(p->map[k]>=CAD_FACTORY_COUNT&&(p->map[k]<128||p->map[k]>=176))return -3;
 for(int p0=0;p0<2;p0++)for(int t=0;t<8;t++)for(int st=0;st<32;st++)if(p->pattern[p0].velocity[t][st]>127||p->pattern[p0].delay[t][st]<-8||p->pattern[p0].delay[t][st]>8)return -4;
 for(int i=0;i<p->count;i++){const cad_event *e=&p->event[i];if(e->tick>=p->tape_bars*CAD_BAR||e->kind>1||(e->kind==0&&(e->id>7||e->value>1))||(e->kind==1&&(e->id!=P_FILTER&&e->id!=P_FXRATE&&e->id!=P_FXDEPTH&&e->id!=P_DUST&&e->id!=P_NOISE&&e->id!=P_HIGHPASS)))return -5;
  if(e->kind==1&&cad_parameter_limit(e->id,e->value)!=e->value)return -6;
 }return 0;
}

int cad_upgrade_transfer(unsigned bytes){
 if(cad_transfer.magic!=CAD_PROJECT_MAGIC)return -7;
 if(bytes==2656u&&cad_transfer.version==1u){
  uint8_t *p=(uint8_t*)&cad_transfer;
  for(unsigned i=2656u;i>56u;i--)p[i+3u]=p[i-1u];
  cad_transfer.par[P_NOISE]=cad_transfer.par[P_HIGHPASS]=0;
  cad_transfer.midi_config=0;cad_transfer.version=3;bytes=2660;
 }
 if(bytes==2660u&&(cad_transfer.version==2u||cad_transfer.version==3u)){
  if(cad_transfer.version==2u)cad_transfer.midi_config=0;
  for(int i=0;i<8;i++)cad_transfer.track_level[i]=100;
  cad_transfer.punch=0; /* Preserve the saved listening level on upgrade. */
  memset(cad_transfer.reserved,0,sizeof cad_transfer.reserved);cad_transfer.version=4;bytes=2672;
 }
 if(bytes==2672u&&cad_transfer.version==4u){
  cad_transfer.setting[S_LIGHT]=3;cad_transfer.setting[S_GLOW]=1;cad_transfer.setting[S_MOTION]=cad_transfer.setting[S_SPLASH]=1;cad_transfer.version=CAD_VERSION;
 }else if(bytes!=sizeof cad_transfer||cad_transfer.version!=CAD_VERSION)return -7;
 return cad_validate_project(&cad_transfer);
}

/* Independently seekable IMA blocks: 256 nibbles after predictor/index/reserved. */
static const int16_t ima_step[89]={7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767};
static const int8_t ima_delta[8]={-1,-1,-1,-1,2,4,6,8};
typedef struct {cad_sample smp;uint32_t pos,frac,start,end,step,decoded,block,age,interp_at;int pred,idx,active,category,track,s0,s1;float gain,fade;} voice;
static voice voices[CAD_VOICES];static uint32_t voice_age;
#ifdef EN_HOST
uint32_t cad_test_decode_steps; /* CPU-work regression counter, absent on the device. */
#define CAD_DECODE_STEP() (++cad_test_decode_steps)
#else
#define CAD_DECODE_STEP() ((void)0)
#endif
static int decode(voice *v,uint32_t at){uint32_t block=at>>8;if(block!=v->block||at+1u<v->decoded){const uint8_t *p=v->smp.data+block*132;v->pred=(int16_t)(p[0]|p[1]<<8);v->idx=p[2];v->decoded=block*256;v->block=block;}
 while(v->decoded<=at){CAD_DECODE_STEP();uint32_t k=v->decoded&255u;uint8_t b=v->smp.data[block*132+4+k/2];int nib=(k&1)?b>>4:b&15,st=ima_step[v->idx],diff=st>>3;if(nib&1)diff+=st>>2;if(nib&2)diff+=st>>1;if(nib&4)diff+=st;v->pred=clip(v->pred+((nib&8)?-diff:diff),-32768,32767);v->idx=clip(v->idx+ima_delta[nib&7],0,88);v->decoded++;}return v->pred;}
static voice *trigger(unsigned sample,int velocity,uint32_t start,uint32_t length,int track){cad_sample smp;if(!cad_sample_get(sample,&smp))return 0;if(start>=smp.frames)return 0;
 int chosen=-1;uint32_t age=~0u;for(int i=0;i<CAD_VOICES;i++){if(!voices[i].active){chosen=i;break;}if(voices[i].age<age){age=voices[i].age;chosen=i;}}
 if(smp.category==C_HAT)for(int i=0;i<CAD_VOICES;i++)if(voices[i].category==C_OPEN)voices[i].fade=0.90f;
 voice *v=&voices[chosen];memset(v,0,sizeof *v);v->smp=smp;v->pos=v->start=start;v->end=length&&length<smp.frames-start?start+length:smp.frames;v->step=((uint32_t)smp.rate<<16)/CAD_FS;v->block=v->interp_at=~0u;v->active=1;v->category=smp.category;v->track=track;v->age=++voice_age;v->gain=0.30f*(float)clip(velocity,0,127)/127.0f;v->fade=1.0f;
 if((unsigned)track<8)cad_view.hits[track]=127;return v;
}

static CAD_POOL int16_t history[CAD_RING],capture[CAD_RING],echo_line[CAD_RING];
static uint32_t hw,ew,cap_len,cap_pos;static float brake_pos,brake_rate,filter_mem,filter_mem2,echo_lp,wet_mix,out_prev,bass_phase,bass_env,bass_hz;
static uint32_t noise_rng;
static float hp_mem,hp_smooth,hp_mix,noise_mem,noise_smooth,dust_smooth,grain_env,grain_lp;
static uint16_t held_fx,tape_fx,record_down,last_mask;
static uint8_t fill_left,return_part,pending=255,started,sub,has_undo,tape_bypass,clean_bypass,section;
static uint32_t tape_origin,section_end,cross_left;
static float mix_prev,cross_from,cut_smooth;
static float track_gain[8],master_smooth,punch_smooth,attack_fast,attack_slow;
static void slice(unsigned id,int velocity){
 cad_sample smp;unsigned sid=cad_break_sample((unsigned)project.par[P_BREAK]);
 if(!cad_sample_get(sid,&smp)||smp.category!=8)return;
 uint32_t a=smp.frames*id/16,b=smp.frames*(id+1)/16;
 voice *v=trigger(sid,velocity,a,b-a,-1);
 if(v){float speed=(float)smp.frames*project.par[P_BPM]/(240.0f*CAD_FS);v->step=(uint32_t)(fm_clampf(speed,0.05f,4.0f)*65536.0f);}
}
static uint32_t phase,tick,frame_count,record_start,clock_last,clock_frames,clock_ppqn;
static uint32_t clock_stamp;static uint8_t clock_stamp_valid;
/* Only short realtime packets leave this bounded audio-owned queue. */
typedef struct {uint32_t packet,frame;} midi_event;
static midi_event midi_out[64];static uint32_t midi_w,midi_r,midi_phase;
static int clock_master(void){return (project.midi_config&1u)&&!project.par[P_SYNC];}
static void midi_emit(unsigned status){
 if(midi_w-midi_r>=64u){cad_view.midi_out_overflows++;return;}
 midi_out[midi_w++&63u]=(midi_event){0x0fu|(status<<8),frame_count};
}
int cad_midi_out_take(uint32_t *packet,uint32_t *frame){
 if(midi_r==midi_w)return 0;midi_event e=midi_out[midi_r++&63u];*packet=e.packet;*frame=e.frame;return 1;
}
static int clock_source(int source){return project.par[P_SYNC]&&(source==0||source==project.par[P_SYNC]);}

static int tape_par[P_NPARAM];
static int fx_param(int p){return tape_par[p]>=0?tape_par[p]:project.par[p];}
static void reset_tape_params(void){for(int i=0;i<P_NPARAM;i++)tape_par[i]=-1;}
static void snapshot(void){undo_project=project;has_undo=1;}
static void fx_set(int id,int down,int from_tape){uint16_t *mask=from_tape?&tape_fx:&held_fx;uint16_t before=held_fx|tape_fx;
 if(down)*mask|=(uint16_t)(1u<<id);else *mask&=(uint16_t)~(1u<<id);
 if(down&&id<4&&!(before&15u)){
  unsigned div=1u<<(unsigned)fx_param(P_FXRATE);cap_len=(uint32_t)(CAD_FS*30u/(unsigned)project.par[P_BPM])/div;
  if(cap_len>CAD_RING-1)cap_len=CAD_RING-1;if(cap_len<64)cap_len=64;
  for(uint32_t i=0;i<cap_len;i++)capture[i]=history[(hw-cap_len+i)&(CAD_RING-1)];cap_pos=0;brake_pos=0;brake_rate=1;
 }
}
static void append_at(unsigned pos,int kind,int id,int value){
 if(kind==1)for(int i=(int)project.count-1;i>=0;i--)if(project.event[i].tick==pos&&project.event[i].kind==1&&project.event[i].id==id){project.event[i].value=(uint16_t)value;return;}
 unsigned limit=kind==0&&!value?CAD_EVENTS:CAD_EVENTS-8;
 if(project.count>=limit){cad_view.record_overflows++;return;}
 project.event[project.count++]=(cad_event){(uint16_t)pos,(uint8_t)kind,(uint8_t)id,(uint16_t)value};
 if(kind==0){if(value)record_down|=(uint16_t)(1u<<id);else record_down&=(uint16_t)~(1u<<id);}
 cad_view.dirty=1;
}
static void append_event(int kind,int id,int value){
 if(cad_view.record!=2)return;
 uint32_t len=project.tape_bars*CAD_BAR,pos=(tick-tape_origin)%len;
 unsigned grid=project.par[P_GRID]==1?24u:project.par[P_GRID]==2?12u:1u;
 pos=((pos+grid/2)/grid)*grid;if(pos>=len)pos=len-1;
 append_at(pos,kind,id,value);
}
static void end_record(void){
 if(cad_view.record==2){unsigned len=project.tape_bars*CAD_BAR;
  unsigned pos=tick-record_start>=len?len-1:(tick-tape_origin)%len;
  for(int i=0;i<8;i++)if(record_down&(1u<<i))append_at(pos,0,i,0);
 }
 cad_view.record=0;record_down=held_fx=tape_fx=0;cad_view.tape_on=project.count!=0;reset_tape_params();
}
static void clean(void){end_record();held_fx=tape_fx=0;tape_bypass=clean_bypass=1;cad_view.tape_on=0;reset_tape_params();}
static void play(int on){
 int was_playing=cad_view.playing;
 if(!on&&cad_view.record)end_record();
 cad_view.playing=(uint8_t)(on!=0);held_fx=tape_fx=0;reset_tape_params();
 if(on){tick=phase=tape_origin=0;started=0;clock_last=frame_count;clock_ppqn=0;clock_stamp_valid=0;section=0;
  if(clock_master()){midi_phase=0;midi_emit(0xfa);midi_emit(0xf8);}
 }else {for(int i=0;i<CAD_VOICES;i++)voices[i].fade=0.94f;bass_env=0;
  if(was_playing&&clock_master())midi_emit(0xfc);
 }
}
void cad_midi_out_fault(void){
 cad_view.midi_out_overflows++;play(0);midi_r=midi_w;project.midi_config&=(uint8_t)~1u;cad_view.midi_config=project.midi_config;cad_view.dirty=1;
}
static void resume(void){
 if(cad_view.playing)return;
 cad_view.playing=1;clock_last=frame_count;clock_ppqn=0;clock_stamp_valid=0;
 /* Keep position, part, section, tape origin and fractional tick. Reconstruct the
  * recorded controls at this position, without retriggering sequencer notes. */
 held_fx=tape_fx=0;reset_tape_params();
 if(cad_view.tape_on&&!tape_bypass){unsigned pos=(tick-tape_origin)%(project.tape_bars*CAD_BAR);
  int last[8+P_NPARAM];for(unsigned i=0;i<8+P_NPARAM;i++)last[i]=-1;
  for(unsigned i=0;i<project.count;i++){
   const cad_event *e=&project.event[i];unsigned key=e->kind?8u+e->id:e->id;
   if(e->tick>pos||(int)e->tick<last[key])continue;last[key]=e->tick;
   if(e->kind==0){if(e->value)tape_fx|=(uint16_t)(1u<<e->id);else tape_fx&=(uint16_t)~(1u<<e->id);}
   else tape_par[e->id]=e->value;
  }
 }
 if(clock_master()){midi_phase=0;midi_emit(0xfb);midi_emit(0xf8);}
}
static void on_tick(void);
static void apply(int type,int id,int value){
 switch(type){
 case A_PARAM_DELTA:if((unsigned)id<P_NPARAM){
  int n=project.par[id],d=clip(value,-32767,32767);if(id==P_TAPE_BARS)n=d>0?(n<2?2:n<4?4:8):d<0?(n>4?4:n>2?2:1):n;else n+=d;
  apply(A_PARAM,id,n);
 }break;
 case A_MIX_DELTA:if((unsigned)id<=8)apply(id==8?A_PUNCH:A_TRACK_LEVEL,id,(id==8?project.punch:project.track_level[id])+clip(value,-32767,32767));break;
 case A_STEP_DELTA:if((unsigned)id<8&&(unsigned)value<131136u){
  int st=value&31,delay=(value>>5)&1,d=(value>>6)-1024;
  int n=delay?project.pattern[project.part].delay[id][st]:project.pattern[project.part].velocity[id][st];
  n=clip(n+d,delay?-8:0,delay?8:127);apply(delay?A_STEP_DELAY:A_STEP_VALUE,id,st+((n+(delay?8:0))<<5));
 }break;
 case A_SETTING_DELTA:if((unsigned)id<S_NSETTING)apply(A_SETTING,id,project.setting[id]+clip(value,-32767,32767));break;
 case A_SETTING:if((unsigned)id<S_NSETTING){project.setting[id]=(uint8_t)clip(value,0,id<2?3:1);cad_view.dirty=1;}break;
 case A_QUIET:play(0);memset(voices,0,sizeof voices);bass_env=0;held_fx=tape_fx=record_down=0;reset_tape_params();break;
 case A_PLAY:play(value);break;
 case A_PARAM:if((unsigned)id<P_NPARAM){int v=cad_parameter_limit(id,value);
  if(id==P_SYNC&&v!=project.par[id]){play(0);clock_ppqn=clock_frames=phase=0;clock_stamp_valid=0;}
  if(id==P_TAPE_BARS&&v!=project.par[id]&&project.count){snapshot();end_record();project.count=0;}project.par[id]=(int16_t)v;clean_bypass=0;if(id==P_KIT)map_kit();if(id==P_FILTER||id==P_FXRATE||id==P_FXDEPTH||id==P_DUST||id==P_NOISE||id==P_HIGHPASS){tape_par[id]=-1;append_event(1,id,v);}cad_view.dirty=1;}break;
 case A_NEW:case A_MUTATE:{
  snapshot();int dest=1-project.part;project.pattern[dest]=project.pattern[project.part];
  if(type==A_MUTATE){project.seed+=0x9e3779b9u;cad_mutate(&project,dest,project.seed);}
  else for(int attempt=0;attempt<8;attempt++){project.seed+=0x9e3779b9u;cad_generate(&project,dest,project.seed);if(pattern_distance(&project,project.part,dest)>=14)break;}
  pending=(uint8_t)dest;if(!cad_view.playing){project.part=pending;pending=255;}cad_view.dirty=1;break;}
 case A_STEP:if((unsigned)id<8&&(unsigned)value<32){snapshot();cad_pattern *p=&project.pattern[project.part];p->velocity[id][value]=p->velocity[id][value]?0:100;cad_view.dirty=1;}break;
 case A_STEP_VALUE:case A_STEP_DELAY:if((unsigned)id<8&&(unsigned)value<(type==A_STEP_VALUE?4096u:544u)){
  int st=value&31,n=value>>5;cad_pattern *p=&project.pattern[project.part];
  int old=type==A_STEP_VALUE?p->velocity[id][st]:p->delay[id][st]+8;
  if(old!=n){snapshot();if(type==A_STEP_VALUE)p->velocity[id][st]=(uint8_t)n;else p->delay[id][st]=(int8_t)(n-8);cad_view.dirty=1;}
 }break;
 case A_REROLL:if((unsigned)id<8){snapshot();uint8_t locks=project.locks;project.locks=(uint8_t)~(1u<<(unsigned)id);project.seed+=0x9e3779b9u;cad_generate(&project,project.part,project.seed);project.locks=locks;cad_view.dirty=1;}break;
 case A_FX:if((unsigned)id<8){clean_bypass=0;fx_set(id,value,0);append_event(0,id,value!=0);}break;
 case A_RECORD:if(cad_view.record){end_record();}else {snapshot();project.tape_bars=(uint16_t)project.par[P_TAPE_BARS];cad_view.record=1;tape_bypass=0;if(!cad_view.playing)play(1);cad_view.record=1;}break;
 case A_CLEAN:clean();break;
 case A_UNDO:if(has_undo){if(project.par[P_SYNC]!=undo_project.par[P_SYNC]||project.midi_config!=undo_project.midi_config)play(0);pending=255;uint8_t settings[S_NSETTING];memcpy(settings,project.setting,sizeof settings);project=undo_project;memcpy(project.setting,settings,sizeof settings);has_undo=0;clean();cad_view.dirty=1;}break;
 case A_PART:if((unsigned)id<5){if(cad_view.playing)pending=(uint8_t)id;else if(id<2){project.part=(uint8_t)id;pending=255;section=0;cad_view.dirty=1;}else pending=(uint8_t)id;}break;
 case A_FILL:fill_left=16;return_part=project.part;break;
 case A_LOCK:project.locks^=(uint8_t)(id<0?255u:1u<<(unsigned)(id%8));cad_view.dirty=1;break;
 case A_MUTE:if((unsigned)id<8){project.mutes^=(uint8_t)(1u<<id);cad_view.dirty=1;}break;
 case A_TRACK_LEVEL:if((unsigned)id<8){int n=clip(value,0,200);if(project.track_level[id]!=n){snapshot();project.track_level[id]=(uint8_t)n;cad_view.dirty=1;}}break;
 case A_PUNCH:{int n=clip(value,0,100);if(project.punch!=n){snapshot();project.punch=(uint8_t)n;cad_view.dirty=1;}}break;
 case A_PAD:if((unsigned)id<16)trigger(project.map[id],value,0,0,id%8);break;
 case A_SLICE:if((unsigned)id<16)slice((unsigned)id,value>0?clip(value,1,127):110);break;
 case A_MODE:cad_view.mode=(uint8_t)clip(value,0,2);for(int i=0;i<8;i++)if(held_fx&(1u<<i)){append_event(0,i,0);fx_set(i,0,0);}break;
 case A_TAPE_TOGGLE:cad_view.tape_on=(uint8_t)!cad_view.tape_on;tape_bypass=!cad_view.tape_on;clean_bypass=0;tape_fx=0;reset_tape_params();break;
 case A_EXPORT:cad_transfer=project;BARRIER();cad_transfer_ack=(uint32_t)value;break;
 case A_IMPORT:if(!cad_validate_project(&cad_transfer)){
  play(0);project=cad_transfer;memset(voices,0,sizeof voices);bass_env=0;
  pending=255;section=fill_left=has_undo=0;return_part=project.part;held_fx=tape_fx=record_down=0;
  tape_origin=0;tape_bypass=clean_bypass=0;reset_tape_params();cad_view.tape_on=project.count!=0;cad_view.dirty=0;
 }break;
 case A_MAP:if((unsigned)id<16&&((unsigned)value<CAD_FACTORY_COUNT||(value>=128&&value<176))){project.map[id]=(uint16_t)value;cad_view.dirty=1;}break;
 case A_PANIC:play(0);clean();memset(voices,0,sizeof voices);break;
 case A_START:if(clock_source(id))play(1);break;
 case A_CONTINUE:if(clock_source(id)||(!id&&!project.par[P_SYNC]))resume();break;
 case A_STOP:if(clock_source(id))play(0);break;
 case A_CLOCK:if(clock_source(id)){
  uint32_t elapsed=frame_count-clock_last;clock_last=frame_count;
  if(id){uint32_t stamp=(uint32_t)value,ms=stamp-clock_stamp;
   elapsed=clock_stamp_valid&&ms>=8u&&ms<=80u?(ms*CAD_FS+500u)/1000u:0;
   clock_stamp=stamp;clock_stamp_valid=1;
  }
  if(elapsed>=450u&&elapsed<=3500u){clock_frames=clock_frames?(clock_frames*7+elapsed)/8:elapsed;project.par[P_BPM]=(int16_t)clip((int)(CAD_FS*60u/(clock_frames*24u)),40,220);}
  if(cad_view.playing){
   /* Every received edge anchors exactly four internal ticks. Finish a late
    * sub-tick before the next edge; integer BPM rounding must never drift. */
   if(started)while(cad_view.playing&&clock_ppqn){clock_ppqn--;tick++;on_tick();}
   clock_ppqn=4;phase=0;
  }
 }break;
 case A_MIDI_CONFIG:{unsigned cfg=project.midi_config;
  if(id==0){unsigned on=value!=0;if(on!=(cfg&1u)){play(0);cfg=(cfg&~1u)|on;midi_phase=0;}}
  else if(id==1||id==2)cfg=(cfg&1u)|((unsigned)clip((id==2?(int)(cfg>>1):0)+clip(value,-32767,32767),0,17)<<1);
  project.midi_config=(uint8_t)cfg;cad_view.dirty=1;
 }break;
 case A_SILENCE:memset(voices,0,sizeof voices);bass_env=0;clean();break;
 }
}
int cad_command(int type,int id,int value){uint32_t w=qw;if(w-qr>=128){cad_view.queue_overflows++;return -1;}queue[w&127]=(command){(uint8_t)type,(uint8_t)id,value};BARRIER();qw=w+1;return 0;}
static void process_queue(void){uint32_t w=qw;BARRIER();while(qr!=w){command c=queue[qr&127];qr++;apply(c.type,c.id==255?-1:c.id,c.value);}}
static void on_tick(void){
 uint32_t cycle=tick%768u,bar=tick/CAD_BAR,len=project.tape_bars*CAD_BAR;
 if(tick%CAD_BAR==0){
  if(section&&bar>=section_end){if(section==4){section=0;play(0);return;}project.part=return_part;section=0;}
  if(pending<5){if(pending<2){project.part=pending;section=0;}else{section=pending;return_part=project.part;section_end=bar+(section==2?2u:section==3?4u:1u);}pending=255;}
  if(cad_view.record==2&&tick-record_start>=len)end_record();
  if(cad_view.record==1&&(!project.count||(tick-tape_origin)%len==0)){
   record_start=tick;if(!project.count)tape_origin=tick;
   cad_view.record=2;cad_view.tape_on=1;tape_bypass=clean_bypass=0;record_down=0;
   for(int i=0;i<8;i++)if(held_fx&(1u<<i))append_at(0,0,i,1);
  }
  if(project.par[P_AUTO]&&bar&&bar%8==7)fill_left=16;
  if(bar&&bar%4==0&&project.par[P_EVOLVE]&&project.locks!=255){uint32_t s=project.seed^bar;
   if(rng(&s)%100u<(unsigned)project.par[P_EVOLVE]){cad_mutate(&project,project.part,project.seed+bar);cad_view.dirty=1;}}
 }
 uint32_t tp=(tick-tape_origin)%len;
 if(tp==0){tape_fx=0;reset_tape_params();}
 if(cad_view.tape_on&&!tape_bypass)for(unsigned e=0;e<project.count;e++){cad_event *v=&project.event[e];if(v->tick==tp){if(v->kind==0)fx_set(v->id,v->value,1);else tape_par[v->id]=v->value;}}
 cad_pattern *p=&project.pattern[project.part];
 for(int t=0;t<8;t++){
  if(project.mutes&(1u<<t))continue;
  if(section==2&&t!=C_HAT&&t!=C_RIM&&!(bar%2&&t==C_KICK))continue;
  if(section==3&&(t==C_KICK||t==C_SNARE))continue;
  if(section==4&&tick%CAD_BAR>192u)continue;
  for(int st=0;st<32;st++){
   if(!p->velocity[t][st])continue;int pos=cad_step_tick(&project,project.part,t,st);
   if((uint32_t)pos!=cycle||!p->velocity[t][st])continue;
   if(project.par[P_SPACE]>60&&bar%8>=4&&t>=2&&st%16>=8)continue;
   trigger(project.map[t],p->velocity[t][st],0,0,t);if(t==C_KICK&&project.par[P_BASS]){bass_env=0.5f;int note=project.par[P_ROOT]+(st%16>=12?3:st%8==6?7:0);bass_hz=440.0f*fm_exp2f((note-69)/12.0f);}
  }
 }
 if(fill_left&&tick%24==0){int t=tick%48?C_SNARE:C_PERC;if(!(project.mutes&(1u<<t)))trigger(project.map[t],50+(int)(tick%31),0,0,t);fill_left--;}
 if(project.par[P_BREAKLEVEL]&&tick%24==0&&section!=3)slice((tick/24)%16,project.par[P_BREAKLEVEL]);
}
void cad_init(void){
 memset(&project,0,sizeof project);memset((void*)&cad_view,0,sizeof cad_view);memset(voices,0,sizeof voices);memset(history,0,sizeof history);memset(capture,0,sizeof capture);memset(echo_line,0,sizeof echo_line);
 for(int i=0;i<8;i++){project.track_level[i]=100;track_gain[i]=1;}project.punch=50;punch_smooth=0.5f;master_smooth=0.82f;attack_fast=attack_slow=0;
 project.setting[S_LIGHT]=3;project.setting[S_GLOW]=1;project.setting[S_MOTION]=project.setting[S_SPLASH]=1;
 project.magic=CAD_PROJECT_MAGIC;project.version=CAD_VERSION;project.seed=248;project.tape_bars=4;
 int16_t defaults[P_NPARAM]={92,58,35,92,0,62,65,20,0,24,2,75,4,1,0,33,0,0,82,0,100,0,10,0};memcpy(project.par,defaults,sizeof defaults);map_kit();cad_generate(&project,0,248);cad_generate(&project,1,9517);
 qw=qr=0;midi_w=midi_r=midi_phase=0;clock_stamp=0;clock_stamp_valid=0;voice_age=0;tick=phase=frame_count=clock_last=clock_frames=clock_ppqn=0;hw=ew=cap_pos=0;cap_len=128;brake_pos=0;brake_rate=1;held_fx=tape_fx=0;fill_left=has_undo=tape_bypass=clean_bypass=started=sub=section=0;record_down=last_mask=0;tape_origin=section_end=cross_left=0;mix_prev=cross_from=0;cut_smooth=0.78f;noise_rng=0x7da3a19du;hp_mem=hp_mix=noise_mem=noise_smooth=dust_smooth=grain_env=grain_lp=filter_mem2=0;hp_smooth=0.001f;pending=255;filter_mem=echo_lp=wet_mix=out_prev=bass_phase=bass_env=0;reset_tape_params();cad_transfer_ack=0;
}
const cad_project *cad_project_read(void){return &project;}
void cad_render(int32_t *out,uint32_t n,uint32_t gain){
 process_queue();for(uint32_t f=0;f<n;f++){
  if(clock_master()){if(midi_phase>=CAD_FS*60u){midi_phase-=CAD_FS*60u;midi_emit(0xf8);}midi_phase+=(uint32_t)project.par[P_BPM]*24u;}
  if(cad_view.playing){if(!started&&(!project.par[P_SYNC]||clock_ppqn)){on_tick();started=1;}if(project.par[P_SYNC]){uint32_t period=clock_frames?clock_frames:CAD_FS*60u/((uint32_t)project.par[P_BPM]*24u);if(started&&clock_ppqn)phase+=4u;if(phase>=period&&clock_ppqn){phase-=period;clock_ppqn--;tick++;on_tick();}if(frame_count-clock_last>CAD_FS*2u){cad_view.clock_losses++;play(0);}}
   else {phase+=(uint32_t)project.par[P_BPM]*CAD_PPQN;if(phase>=CAD_FS*60u){phase-=CAD_FS*60u;tick++;on_tick();}}
  }
  for(int t=0;t<8;t++)track_gain[t]+=0.0025f*(project.track_level[t]*0.01f-track_gain[t]);
  float dry=0;for(int i=0;i<CAD_VOICES;i++){voice *v=&voices[i];if(!v->active)continue;uint32_t at=v->pos;if(at>=v->end){v->active=0;continue;}float env=at-v->start<16?(float)(at-v->start)/16.0f:1;if(v->end-at<32)env=(float)(v->end-at)/32.0f;/* Cache the two decoded endpoints; an oversampled frame never re-decodes a block. */
   if(at!=v->interp_at){v->s0=(v->interp_at!=~0u&&at==v->interp_at+1u)?v->s1:decode(v,at);v->s1=at+1u<v->end?decode(v,at+1u):v->s0;v->interp_at=at;}
   float sample=(float)v->s0+(float)(v->s1-v->s0)*(v->frac/65536.0f);dry+=sample*(v->gain*env/32768.0f)*((unsigned)v->track<8?track_gain[v->track]:1.0f);v->frac+=v->step;v->pos+=v->frac>>16;v->frac&=65535u;v->gain*=v->fade;if(v->gain<0.0001f)v->active=0;}
  float bass=0;if(bass_env>0.0001f&&project.par[P_BASS]){bass_phase+=bass_hz/CAD_FS;if(bass_phase>=1)bass_phase-=1;bass=fm_sinf(bass_phase*FM_TWO_PI)*bass_env*(project.par[P_BASS]/300.0f);bass_env*=0.99975f;}
  float effected=dry;uint16_t mask=held_fx|tape_fx;int depth=fx_param(P_FXDEPTH);
  if(mask!=last_mask){cross_from=mix_prev;cross_left=128;last_mask=mask;}
  if((sub++&1)==0){history[hw++&(CAD_RING-1)]=(int16_t)(fm_clampf(dry,-1,1)*32767);
   if(mask&15){if(!cap_len)cap_len=128;uint32_t p=cap_pos%cap_len;if(mask&(1u<<FX_REVERSE))p=cap_len-1-p;if(mask&(1u<<FX_BRAKE)){p=(uint32_t)brake_pos%cap_len;brake_pos+=brake_rate;brake_rate*=0.9997f;}effected=(float)capture[p]/32768.0f;unsigned edge=cap_pos%cap_len;if(edge<32)effected*=edge/32.0f;else if(cap_len-edge<32)effected*=(cap_len-edge)/32.0f;if(mask&(1u<<FX_STUTTER))if((cap_pos/(cap_len/4+1))&1u)effected=0;cap_pos++;}
   unsigned delay=(unsigned)(CAD_FS*15u/(unsigned)project.par[P_BPM]);if(delay>=CAD_RING)delay=CAD_RING-1;
   float e=(float)echo_line[(ew-delay)&(CAD_RING-1)]/32768.0f;echo_lp+=0.45f*(e-echo_lp);
   float send=(mask&(1u<<FX_ECHO))?effected:0;echo_line[ew++&(CAD_RING-1)]=(int16_t)(fm_clampf(send+echo_lp*0.52f,-0.9f,0.9f)*32767);
   out_prev=effected;
  }else if(mask&15)effected=out_prev;
  /* Full-rate tone path: the normal beat no longer passes through a 22 kHz hold.
   * Two gentle low-pass stages, an independent high-pass and smoothed controls. */
  float tone=fx_param(P_FILTER)/100.0f;
  float cut=(mask&(1u<<FX_FILTER))?0.035f:0.012f+0.988f*tone*tone*tone;
  cut_smooth+=0.0025f*(cut-cut_smooth);
  filter_mem+=cut_smooth*(effected-filter_mem);filter_mem2+=cut_smooth*(filter_mem-filter_mem2);effected=filter_mem2;
  float hp=fx_param(P_HIGHPASS)/100.0f;hp_smooth+=0.0025f*(0.001f+0.2f*hp*hp*hp-hp_smooth);
  hp_mix+=0.0025f*((hp>0?1.0f:0.0f)-hp_mix);hp_mem+=hp_smooth*(effected-hp_mem);effected-=hp_mem*hp_mix;
  if(mask&(1u<<FX_CRUSH)){float q=32.0f+(100-depth)*12.0f;effected=(float)(int)(effected*q)/q;}
  if(mask&(1u<<FX_GATE))if((tick/12)&1u)effected=0;
  effected+=echo_lp*0.45f;
  float target=clean_bypass?0:(mask?depth/100.0f:1.0f)*project.par[P_TAPE_MIX]/100.0f;wet_mix+=0.01f*(target-wet_mix);
  float mix=dry+(effected-dry)*wet_mix+bass;
  dust_smooth+=0.0025f*((clean_bypass?0:fx_param(P_DUST)/100.0f)-dust_smooth);
  grain_lp+=0.32f*(mix-grain_lp);mix+=(grain_lp-mix)*(dust_smooth*0.35f);
  mix=mix*(1.0f+dust_smooth*1.5f)/(1.0f+fm_fabsf(mix)*dust_smooth*1.5f);
  /* Independent deterministic noise generator; no effect on beat seeds. Silence
   * after stop, and no random one-sample spikes masquerading as vinyl damage. */
  grain_env+=0.0025f*((cad_view.playing||fm_fabsf(dry)>0.0001f?1.0f:0.0f)-grain_env);
  noise_smooth+=0.0025f*((clean_bypass?0:fx_param(P_NOISE)/100.0f)-noise_smooth);
  float white=((int)(rng(&noise_rng)&65535u)-32768)/32768.0f;
  noise_mem+=0.16f*(white-noise_mem);mix+=noise_mem*(noise_smooth*grain_env*0.02f);
  if(cross_left){float a=(128-cross_left)/128.0f;mix=cross_from+(mix-cross_from)*a;cross_left--;}mix_prev=mix;
  master_smooth+=0.0025f*(project.par[P_VOLUME]*0.01f-master_smooth);
  punch_smooth+=0.0025f*(project.punch*0.01f-punch_smooth);
  float magnitude=fm_fabsf(mix);attack_fast+=0.035f*(magnitude-attack_fast);attack_slow+=0.001f*(magnitude-attack_slow);
  float transient=fm_clampf((attack_fast-attack_slow)*4.0f,0,0.35f);
  mix*=master_smooth*(1.0f+2.5f*punch_smooth)*(1.0f+transient*punch_smooth);
  /* Monotonic soft knee above -3 dBFS; leave quiet transients untouched.
   * PUNCH 0 retains the legacy transfer. No allocation or look-ahead delay. */
  magnitude=fm_fabsf(mix);
  if(punch_smooth>0.0001f&&magnitude>0.70f){float over=magnitude-0.70f;float limited=0.70f+over/(1.0f+over/0.25f);mix=mix<0?-limited:limited;}
  mix=fm_clampf(mix,-0.95f,0.95f);int32_t sample=(int32_t)(mix*(float)(gain>4096?4096:gain)*(8388607.0f/4096.0f));out[f*2]=out[f*2+1]=sample;
  uint32_t pk=(uint32_t)(fm_fabsf(mix)*32767);if(pk>cad_view.peak)cad_view.peak=pk;frame_count++;
 }
 cad_view.midi_config=project.midi_config;cad_view.section=section;cad_view.tick=tick;cad_view.bar=tick/CAD_BAR;cad_view.frames=frame_count;cad_view.step=(uint8_t)(tick/24%32);cad_view.part=project.part;cad_view.pending_part=pending;cad_view.fx_mask=held_fx|tape_fx;cad_view.event_count=project.count;cad_view.locks=project.locks;cad_view.mutes=project.mutes;cad_view.seed=project.seed;
 for(int i=0;i<S_NSETTING;i++)cad_view.setting[i]=project.setting[i];
 for(int t=0;t<8;t++)cad_view.track_level[t]=project.track_level[t];cad_view.punch=project.punch;
 memcpy((void*)&cad_view.pattern,&project.pattern[project.part],sizeof(cad_pattern));
 for(int i=0;i<P_NPARAM;i++)cad_view.par[i]=project.par[i];cad_view.peak=cad_view.peak*15/16;for(int i=0;i<8;i++)if(cad_view.hits[i])cad_view.hits[i]--;
}

void cad_midi_receive(uint32_t packet,unsigned source,uint32_t received_ms){
 if(source!=1u&&source!=2u)return;
 unsigned status=(packet>>8)&255u,d1=(packet>>16)&127u,d2=(packet>>24)&127u;
 if(status==0xf8)cad_command(A_CLOCK,(int)source,(int32_t)received_ms);
 else if(status==0xfa)cad_command(A_START,(int)source,0);
 else if(status==0xfb)cad_command(A_CONTINUE,(int)source,0);
 else if(status==0xfc)cad_command(A_STOP,(int)source,0);
 else {unsigned ch=cad_view.midi_config>>1;if(ch==17u||(ch&&ch!=(status&15u)+1u))return;
  if((status&0xf0u)==0x90u&&d2&&d1>=36u&&d1<52u)cad_command(cad_view.mode==2?A_SLICE:A_PAD,(int)d1-36,(int)d2);
  else if((status&0xf0u)==0xb0u){if(d1>=20u&&d1<28u)cad_command(A_FX,(int)d1-20,d2>=64u);else if(d1==120u||d1==123u)cad_command(A_SILENCE,0,0);}
 }
}
