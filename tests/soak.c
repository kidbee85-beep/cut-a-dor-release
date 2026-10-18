/* SPDX-License-Identifier: GPL-3.0-only */
#include "cad.h"
#include <stdio.h>
#include <assert.h>
static int32_t out[512];
int main(void){cad_init();cad_command(A_PARAM,P_BPM,93);cad_command(A_PARAM,P_BASS,75);cad_command(A_PARAM,P_BREAKLEVEL,65);cad_command(A_MIDI_CONFIG,0,1);cad_command(A_PLAY,0,1);unsigned clocks=0;unsigned total=44100*1800u,at=0;while(at<total){unsigned n=total-at>256?256:total-at;if(at%(44100*8u)<256){cad_command(A_NEW,0,0);cad_command(A_FX,(int)(at/(44100*8u)%8),1);}if(at%(44100*8u)>44100*7u&&at%(44100*8u)<44100*7u+256)cad_command(A_CLEAN,0,0);cad_render(out,n,4096);for(unsigned i=0;i<2*n;i++)assert(out[i]>=-8388607&&out[i]<=8388607);uint32_t packet,frame;while(cad_midi_out_take(&packet,&frame))if(((packet>>8)&255u)==0xf8)clocks++;at+=n;}assert(cad_view.tick==1800u*93u*96u/60u);assert(!cad_view.queue_overflows&&!cad_view.midi_out_overflows);assert(clocks==93u*24u*30u);printf("30-minute offline soak: %u frames, %u ticks, no timing drift or queue loss; USB Clock OUT also checked\n",at,cad_view.tick);return 0;}
