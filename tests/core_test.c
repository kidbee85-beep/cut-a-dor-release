/* SPDX-License-Identifier: GPL-3.0-only */
#include "cad.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern uint32_t cad_test_decode_steps;
static int32_t audio[512];static unsigned checks;
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x);exit(1);}}while(0)
static void render(unsigned n){while(n){unsigned k=n>256?256:n;cad_render(audio,k,4096);for(unsigned i=0;i<k*2;i++){CHECK(audio[i]>=-8388607&&audio[i]<=8388607);CHECK(audio[i]==audio[i^1]);}n-=k;}}
static void cmd(int a,int i,int v){CHECK(!cad_command(a,i,v));cad_render(audio,0,4096);}
static void to_tick(unsigned t){unsigned guard=0;while(cad_view.tick<t&&guard++<100000)render(16);CHECK(cad_view.tick>=t);}
static void patterns(void){cad_init();cad_project a=*cad_project_read(),b=a;uint32_t hashes[600];unsigned distinct=0;
 for(int style=0;style<6;style++)for(int seed=1;seed<=100;seed++){
  a.par[P_STYLE]=b.par[P_STYLE]=style;cad_generate(&a,0,seed);cad_generate(&b,0,seed);CHECK(!memcmp(&a.pattern[0],&b.pattern[0],sizeof(cad_pattern)));CHECK(!cad_validate_project(&a));
  for(int bar=0;bar<2;bar++){CHECK(a.pattern[0].velocity[C_KICK][bar*16]>0);if(style==3)CHECK(a.pattern[0].velocity[C_SNARE][bar*16+8]>0);else {CHECK(a.pattern[0].velocity[C_SNARE][bar*16+4]>0);CHECK(a.pattern[0].velocity[C_SNARE][bar*16+12]>0);}}
  hashes[distinct++]=cad_crc32(&a.pattern[0],sizeof(cad_pattern));
 }
 unsigned unique=0;for(unsigned i=0;i<distinct;i++){int seen=0;for(unsigned j=0;j<i;j++)if(hashes[i]==hashes[j])seen=1;if(!seen)unique++;}CHECK(unique>590);
 cmd(A_LOCK,C_KICK,0);a=*cad_project_read();cmd(A_NEW,0,0);b=*cad_project_read();CHECK(!memcmp(a.pattern[a.part].velocity[0],b.pattern[b.part].velocity[0],32));
 printf("Generator: %u distinct / %u seeded patterns; anchors and lock preserved\n",unique,distinct);
}
static unsigned onset_diff(const cad_pattern *a,const cad_pattern *b,int weighted){unsigned n=0;for(int t=0;t<8;t++)for(int st=0;st<32;st++)if(!!a->velocity[t][st]!=!!b->velocity[t][st])n+=weighted?(t==0?3:t==1?4:t==2?1:2):1;return n;}
static unsigned count_hits(const cad_pattern *p){unsigned n=0;for(int t=0;t<8;t++)for(int st=0;st<32;st++)n+=p->velocity[t][st]!=0;return n;}
static void groove_structure(void){
 unsigned minimum=999,maximum_mutation=0;
 for(int style=0;style<6;style++){
  cad_init();cmd(A_PARAM,P_STYLE,style);unsigned families=0;
  for(int i=0;i<600;i++){
   cad_project before=*cad_project_read();cmd(A_NEW,0,0);cad_project after=*cad_project_read();
   unsigned d=onset_diff(&before.pattern[before.part],&after.pattern[after.part],1);CHECK(d>=14);if(d<minimum)minimum=d;families|=1u<<(after.seed%16u);
   cad_project mutated=after;cad_mutate(&mutated,after.part,after.seed);unsigned m=onset_diff(&after.pattern[after.part],&mutated.pattern[after.part],0);CHECK(m>=2&&m<=5);if(m>maximum_mutation)maximum_mutation=m;
   for(int bar=0;bar<2;bar++){CHECK(mutated.pattern[after.part].velocity[0][bar*16]);CHECK(mutated.pattern[after.part].velocity[1][bar*16+(style==3?8:4)]);}
  }CHECK(families==65535);
 }
 cad_init();cad_project p=*cad_project_read();for(int style=0;style<6;style++)for(unsigned seed=1;seed<100;seed++){
  p.par[P_STYLE]=style;unsigned prev=0;for(int density=0;density<=100;density+=50){p.par[P_DENSITY]=density;cad_generate(&p,0,seed);unsigned n=count_hits(&p.pattern[0]);CHECK(n>=prev);prev=n;}
  p.par[P_SPACE]=100;cad_generate(&p,0,seed);for(int t=2;t<8;t++)for(int st=24;st<32;st++)CHECK(!p.pattern[0].velocity[t][st]);p.par[P_SPACE]=35;
 }
 for(int t=0;t<8;t++){cad_init();cmd(A_LOCK,t,0);cad_project old=*cad_project_read();cmd(A_NEW,0,0);const cad_project *q=cad_project_read();CHECK(!memcmp(old.pattern[old.part].velocity[t],q->pattern[q->part].velocity[t],32));CHECK(!memcmp(old.pattern[old.part].delay[t],q->pattern[q->part].delay[t],32));cmd(A_MUTATE,0,0);q=cad_project_read();CHECK(!memcmp(old.pattern[old.part].velocity[t],q->pattern[q->part].velocity[t],32));}
 cad_init();cmd(A_LOCK,-1,0);p=*cad_project_read();cmd(A_NEW,0,0);CHECK(!memcmp(&p.pattern[p.part],&cad_project_read()->pattern[cad_project_read()->part],sizeof(cad_pattern)));
 cad_init();p=*cad_project_read();cmd(A_MUTATE,0,0);cmd(A_UNDO,0,0);CHECK(!memcmp(&p,cad_project_read(),sizeof p));
 cmd(A_PLAY,0,1);to_tick(100);p=*cad_project_read();cmd(A_NEW,0,0);CHECK(cad_view.part==p.part);cmd(A_UNDO,0,0);to_tick(384);CHECK(cad_view.part==p.part);CHECK(!memcmp(&p,cad_project_read(),sizeof p));
 cmd(A_STEP,0,1);CHECK(cad_project_read()->pattern[p.part].velocity[0][1]!=p.pattern[p.part].velocity[0][1]);cmd(A_UNDO,0,0);CHECK(!memcmp(&p,cad_project_read(),sizeof p));
 printf("Groove structure: 3,600 New beats, weighted onset distance >= %u, all 16 families/style; Mutate 2-%u hits; density, scratch windows, locks, undo and bar switching passed\n",minimum,maximum_mutation);
}
static void tape(void){cad_init();cmd(A_PARAM,P_EVOLVE,0);cmd(A_PARAM,P_TAPE_BARS,2);cmd(A_PARAM,P_GRID,0);cmd(A_PLAY,0,1);to_tick(391);cmd(A_RECORD,0,0);to_tick(768);CHECK(cad_view.record==2);cmd(A_FX,0,1);to_tick(800);cmd(A_FX,0,0);cmd(A_PARAM,P_FILTER,23);to_tick(1536);CHECK(!cad_view.record);CHECK(cad_view.event_count==3);CHECK(cad_project_read()->event[0].tick==0);CHECK(cad_view.fx_mask&1);to_tick(1568);CHECK(!(cad_view.fx_mask&1));
 cmd(A_CLEAN,0,0);CHECK(!cad_view.fx_mask&&!cad_view.tape_on);cmd(A_FX,1,1);CHECK(cad_view.fx_mask==2);cmd(A_FX,1,0);CHECK(!cad_view.fx_mask);
 cmd(A_UNDO,0,0);CHECK(cad_view.event_count==0);
 cad_init();cmd(A_PARAM,P_TAPE_BARS,1);cmd(A_PARAM,P_GRID,0);cmd(A_RECORD,0,0);render(1);CHECK(cad_view.record==2);
 for(int i=0;i<8;i++)cmd(A_FX,i,1);
 for(int i=0;i<280;i++){to_tick((unsigned)i+1);cmd(A_PARAM,P_FILTER,i%101);}
 to_tick(384);CHECK(cad_view.record==0);CHECK(cad_view.record_overflows>0);CHECK(cad_view.event_count<=256);CHECK(!cad_validate_project(cad_project_read()));
 to_tick(767);CHECK(!cad_view.fx_mask);printf("FX tape: bar-aligned record/replay, undo, clean, 256-event capacity and releases passed\n");
}
static void banks(void){unsigned frames=80000,bytes=(frames+255)/256*132,total=512+bytes;uint8_t *buf=calloc(1,total);cad_bank *b=(cad_bank*)buf;b->magic=CAD_BANK_MAGIC;b->version=1;b->count=1;b->data_len=bytes;b->zone[0]=(cad_zone){0,frames,16000,8,0,"LONG TEST"};
 b->data_crc=cad_crc32(buf+512,bytes);b->header_crc=cad_crc32(buf,512);CHECK(!cad_validate_bank(buf,total));cad_init();CHECK(!cad_set_bank(0,buf,total));cmd(A_MAP,0,128);cad_test_decode_steps=0;cmd(A_PAD,0,127);render(CAD_FS*6);CHECK(cad_test_decode_steps==frames);CHECK(!cad_set_bank(0,0,0));
 for(unsigned i=0;i<total;i+=397){buf[i]^=1;CHECK(cad_validate_bank(buf,total)!=0);buf[i]^=1;}
 CHECK(cad_validate_bank(buf,total-1)!=0);CHECK(cad_validate_bank(0,total)!=0);free(buf);
 printf("Bank validation: CRC, truncation, bounds and 80,000-frame sample and linear decode work passed\n");
}
static void performance(void){cad_init();cmd(A_PARAM,P_EVOLVE,0);cmd(A_PARAM,P_BASS,80);cmd(A_PARAM,P_BREAKLEVEL,90);cmd(A_PLAY,0,1);
 for(int k=0;k<12;k++){cmd(A_PARAM,P_KIT,k);cmd(A_PARAM,P_BREAK,k);for(int fx=0;fx<8;fx++){cmd(A_FX,fx,1);render(2048);cmd(A_FX,fx,0);render(512);}for(int p=0;p<16;p++)cmd(A_PAD,p,127);render(4096);cmd(A_CLEAN,0,0);}
 cmd(A_PART,4,0);to_tick(((cad_view.tick/384)+2)*384);CHECK(!cad_view.playing);
 cad_init();for(int i=0;i<128;i++)CHECK(!cad_command(A_PARAM,P_BPM,i));CHECK(cad_command(A_PLAY,0,1)==-1);cad_render(audio,0,4096);CHECK(cad_view.queue_overflows==1);
 for(int b=40;b<=220;b+=30){cad_init();cmd(A_PARAM,P_BPM,b);cmd(A_PARAM,P_EVOLVE,0);cmd(A_PLAY,0,1);unsigned n=CAD_FS*20;render(n);CHECK(cad_view.tick==n/CAD_FS*(unsigned)b*96/60);}
 printf("Audio: all kits/FX, maximum voice use, outro, queue bounds and tempo drift passed\n");
}
static void midi_clock(void){
 cad_init();cmd(A_PARAM,P_SYNC,1);cmd(A_START,0,0);
 for(int i=0;i<240;i++){cmd(A_CLOCK,0,0);render(1225);}
 CHECK(cad_view.tick>=950&&cad_view.tick<=960);CHECK(cad_view.par[P_BPM]==90);
 render(CAD_FS*3);CHECK(!cad_view.playing&&cad_view.clock_losses==1);
 cmd(A_START,0,0);cmd(A_CLOCK,0,0);render(1225);CHECK(cad_view.playing);cmd(A_STOP,0,0);CHECK(!cad_view.playing);
 puts("MIDI clock: 24 PPQN, tempo following, clock loss, restart and stop passed");
}
int main(void){CHECK(sizeof(cad_project)<=3840);patterns();groove_structure();tape();banks();performance();midi_clock();printf("PASS (%u assertions), project %zu B, %u factory sounds\n",checks,sizeof(cad_project),cad_factory_count());return 0;}
