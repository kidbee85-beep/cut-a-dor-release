/* SPDX-License-Identifier: GPL-3.0-only */
#include "cad.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#include "midi_queue.h"
static int32_t out[512];static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"MIDI FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void cmd(int type,int id,int v){CHECK(!cad_command(type,id,v));cad_render(out,0,4096);}
static void render(unsigned n){while(n){unsigned k=n>256?256:n;cad_render(out,k,4096);n-=k;}}
static void msg(unsigned status,unsigned source,uint32_t ms){cad_midi_receive(15u|(status<<8),source,ms);cad_render(out,0,4096);}
static void follower(void){
 cad_init();cmd(A_PARAM,P_SYNC,1);msg(0xfa,1,0);render(5000);CHECK(cad_view.tick==0);
 for(unsigned i=0;i<240;i++){msg(0xf8,1,i*1000u/36u);render(1225);}
 unsigned stopped=cad_view.tick;CHECK(stopped>950);msg(0xfc,1,7000);render(22050);CHECK(!cad_view.playing&&cad_view.tick==stopped);
 msg(0xfb,1,7500);CHECK(cad_view.playing&&cad_view.tick==stopped);
 msg(0xf8,1,7500);render(1225);CHECK(cad_view.tick>=stopped&&cad_view.tick<=stopped+5);
 msg(0xfa,1,7600);CHECK(cad_view.tick==0);msg(0xf8,1,7600);render(1225);CHECK(cad_view.tick<=5);
 render(CAD_FS*3);CHECK(!cad_view.playing&&cad_view.clock_losses==1);
 puts("Continue retains position; Start resets; first pulse wait, Stop and clock-loss passed.");
}
static void jitter(void){
 /* 20,000 clock edges with realistic millisecond timestamp jitter. Each edge
  * must preserve absolute song position, regardless of rounded display BPM. */
 for(unsigned bpm=73;bpm<=173;bpm+=100){
  cad_init();cmd(A_PARAM,P_SYNC,1);msg(0xfa,1,0);
  uint32_t last=0;
  for(unsigned i=0;i<20000;i++){
   uint32_t at=(uint32_t)(((uint64_t)i*CAD_FS*60u)/(bpm*24u));
   if(i)at+=(i%5u)*17u;
   if(at>last)render(at-last);last=at;
   msg(0xf8,1,at*1000ull/CAD_FS);
   CHECK(cad_view.tick==i*4u);CHECK(cad_view.playing);
  }
  CHECK(!cad_view.clock_losses&&!cad_view.queue_overflows);
 }
 puts("40,000 jittered input edges: exact song position at every pulse, no accumulated tempo-rounding drift.");
}
static void source_channel(void){
 cad_init();cmd(A_MIDI_CONFIG,0,1);cmd(A_PARAM,P_SYNC,2);uint32_t packet,frame;while(cad_midi_out_take(&packet,&frame)){}
 msg(0xfa,1,0);CHECK(!cad_view.playing);msg(0xfa,2,0);CHECK(cad_view.playing);
 for(unsigned i=0;i<12;i++){msg(0xf8,1,i*28);render(1225);}CHECK(cad_view.tick==0);
 for(unsigned i=0;i<24;i++){msg(0xf8,2,400+i*28);render(1225);}CHECK(cad_view.tick>85&&cad_view.tick<100);
 CHECK(!cad_midi_out_take(&packet,&frame));msg(0xfc,1,2000);CHECK(cad_view.playing);msg(0xfc,2,2000);CHECK(!cad_view.playing);
 cad_init();cmd(A_MIDI_CONFIG,1,10);cad_midi_receive(9u|(0x90u<<8)|(36u<<16)|(110u<<24),1,0);render(128);CHECK(cad_view.peak==0);
 cad_midi_receive(9u|(0x99u<<8)|(36u<<16)|(110u<<24),1,0);render(128);CHECK(cad_view.hits[C_KICK]);
 cad_init();cmd(A_MIDI_CONFIG,1,17);cad_midi_receive(9u|(0x99u<<8)|(36u<<16)|(110u<<24),1,0);render(4096);CHECK(cad_view.hits[C_KICK]==0);
 puts("USB/TRS source isolation, no clock echo, channel10 and clock-only filtering passed.");
}
static void master(void){
 for(int bpm=40;bpm<=220;bpm+=30){
  cad_init();cmd(A_PARAM,P_BPM,bpm);cmd(A_PARAM,P_EVOLVE,0);cmd(A_MIDI_CONFIG,0,1);cmd(A_PLAY,0,1);
  uint32_t packet,frame,prev=0,count=0,starts=0;unsigned n=CAD_FS*60u;
  while(n){unsigned k=n>256?256:n;render(k);n-=k;
   while(cad_midi_out_take(&packet,&frame)){CHECK((packet&255u)==15u);unsigned status=(packet>>8)&255u;
    if(status==0xfa){starts++;CHECK(frame==0);}
    else {CHECK(status==0xf8);uint64_t ideal=((uint64_t)count*CAD_FS*60u+(unsigned)bpm*24u-1u)/((unsigned)bpm*24u);CHECK(frame==ideal);CHECK(!count||frame>prev);prev=frame;count++;}
   }
  }
  CHECK(starts==1&&count==(unsigned)bpm*24u);CHECK(!cad_view.midi_out_overflows);
  cmd(A_PLAY,0,0);CHECK(cad_midi_out_take(&packet,&frame)&&((packet>>8)&255u)==0xfc);uint32_t at=cad_view.tick;
  render(256);while(cad_midi_out_take(&packet,&frame))CHECK(((packet>>8)&255u)==0xf8);
  cmd(A_CONTINUE,0,0);CHECK(cad_view.tick==at);CHECK(cad_midi_out_take(&packet,&frame)&&((packet>>8)&255u)==0xfb);CHECK(cad_midi_out_take(&packet,&frame)&&((packet>>8)&255u)==0xf8);
 }
 puts("USB clock engine: exact24PPQN over60s at40..220BPM, sample timestamps, Start/Stop/Continue passed.");
}
static void queues(void){
 mi_w=mi_r=mo_w=mo_r=0;uint32_t base=0xfffffff0u;
 for(unsigned i=0;i<MQ;i++){CHECK(midi_rx_put(i,base+i));CHECK(midi_tx_put(i,base+i));}
 CHECK(!midi_rx_put(99,0)&&midi_rx_drops==1);CHECK(!midi_tx_put(99,0)&&midi_tx_drops==1);
 for(unsigned i=0;i<MQ;i++){CHECK(!midi_tx_ready(base+i-1));CHECK(midi_tx_ready(base+i));CHECK(midi_out_q[mo_r%MQ]==i);mo_r++;CHECK(midi_in_stamp[mi_r%MQ]==base+i);mi_r++;}
 CHECK(!midi_tx_ready(500));
 mi_w=mi_r=mo_w=mo_r=0xfffffff0u;
 for(unsigned i=0;i<200;i++){CHECK(midi_rx_put(i,i));CHECK(midi_tx_put(i,i));CHECK(midi_tx_ready(i));CHECK(midi_out_q[mo_r%MQ]==i);mo_r++;mi_r++;}
 cad_init();cmd(A_MIDI_CONFIG,0,1);cmd(A_PARAM,P_BPM,220);cmd(A_PLAY,0,1);render(CAD_FS*2);CHECK(cad_view.midi_out_overflows>0);uint32_t p,f;unsigned count=0;while(cad_midi_out_take(&p,&f))count++;CHECK(count==64);
 cad_midi_out_fault();CHECK(!cad_view.playing&&!(cad_view.midi_config&1u));
 render(44100);CHECK(!cad_midi_out_take(&p,&f));
 puts("Bounded RX/TX queues: timestamps, overflow, deadlines and32-bit wrap passed.");
}
static void migration(void){
 cad_init();cad_transfer=*cad_project_read();cad_transfer.version=2;cad_transfer.midi_config=255;CHECK(!cad_upgrade_transfer(2660));CHECK(cad_transfer.version==5&&cad_transfer.midi_config==0);
 cad_transfer.midi_config=36;CHECK(cad_validate_project(&cad_transfer));cad_transfer.midi_config=35;CHECK(!cad_validate_project(&cad_transfer));
 puts("Project migration defaults and new MIDI setting bounds passed.");
}
int main(void){follower();jitter();source_channel();master();queues();migration();printf("MIDI PASS: %u checks; software timing only, physical USB/TRS jitter still requires hardware.\n",checks);return 0;}
