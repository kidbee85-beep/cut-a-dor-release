/* SPDX-License-Identifier: GPL-3.0-only */
#include "cad.h"
#include <stddef.h>
void *memcpy(void *d,const void *s,size_t n){unsigned char *a=d;const unsigned char *b=s;while(n--)*a++=*b++;return d;}
void *memset(void *d,int v,size_t n){unsigned char *a=d;while(n--)*a++=(unsigned char)v;return d;}
static int32_t out[512],status[80];
static unsigned char banks[3][CAD_BANK_SIZE] __attribute__((aligned(8)));
int32_t *web_render(unsigned frames){if(frames>256)frames=256;cad_render(out,frames,4096);uint32_t p,t;while(cad_midi_out_take(&p,&t)){}return out;}
int32_t *web_status(void){
 status[0]=cad_view.playing;status[1]=cad_view.tick;status[2]=cad_view.step;status[3]=cad_view.part;status[4]=cad_view.pending_part;
 status[5]=cad_view.fx_mask;status[6]=cad_view.record;status[7]=cad_view.event_count;status[8]=cad_view.mode;status[9]=cad_view.locks;status[10]=cad_view.mutes;
 status[11]=cad_view.tape_on;status[12]=cad_view.section;status[13]=cad_view.peak;status[14]=cad_view.queue_overflows;status[15]=cad_view.record_overflows;
 for(int i=0;i<P_NPARAM;i++)status[16+i]=cad_view.par[i];for(int i=0;i<16;i++)status[40+i]=cad_project_read()->map[i];
 for(int i=0;i<8;i++)status[56+i]=cad_view.hits[i];status[64]=(int32_t)cad_view.seed;status[65]=cad_view.clock_losses;status[66]=cad_view.midi_config;status[67]=cad_view.midi_out_overflows;for(int i=0;i<8;i++)status[68+i]=cad_view.track_level[i];status[76]=cad_view.punch;return status;
}
void *web_project(void){return &cad_transfer;}
unsigned web_project_size(void){return sizeof cad_transfer;}
void web_export(void){cad_command(A_EXPORT,0,1);cad_render(out,0,4096);}
int web_import(unsigned bytes){int r=cad_upgrade_transfer(bytes?bytes:sizeof cad_transfer);if(!r){cad_command(A_IMPORT,0,0);cad_render(out,0,4096);}return r;}
void *web_bank(unsigned slot){return slot<3?banks[slot]:0;}
int web_bank_load(unsigned slot,unsigned n){cad_command(A_PANIC,0,0);cad_render(out,0,4096);return slot<3?cad_set_bank((int)slot,banks[slot],n):-1;}

const void *web_pattern(void){return cad_project_read()->pattern[cad_view.part].velocity;}
