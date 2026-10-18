/* SPDX-License-Identifier: GPL-3.0-only */
#define main display_main
#include "../host/display.c"
#undef main
static void hold(int b){buttons=1u<<b;step();now+=700;step();assert(ui.help);buttons=0;step();assert(ui.help);}
static void shot(const char *dir,const char *name){char path[1024];snprintf(path,sizeof path,"%s/%s.ppm",dir,name);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n240 240\n255\n");for(unsigned p=0;p<240*240;p++){uint16_t c=pixels[p];fputc((c>>11)*255/31,f);fputc(((c>>5)&63)*255/63,f);fputc((c&31)*255/31,f);}fclose(f);}
int main(int argc,char **argv){assert(argc==2);cad_init();cad_render(0,0,4096);ui_init();
 for(int b=0;b<10;b++)for(int l=0;l<8;l++)assert(text_w(&FONT_XS,help_context[b][l])<=224);
 for(int c=0;c<5;c++)for(int l=0;l<8;l++)assert(text_w(&FONT_XS,help_common[c][l])<=224);
 for(int b=0;b<=B_SEQ;b++){
  unsigned page0=ui.page,mode0=cad_view.mode,seed=cad_view.seed,tempo=cad_view.par[P_BPM];hold(b);
  assert(ui.help_button==b&&ui.page==page0&&cad_view.mode==mode0&&cad_view.seed==seed&&cad_view.par[P_BPM]==tempo&&!save_calls);
  enc[EN_PRESET]=5;step();assert(ui.help_card==5);enc[EN_PRESET]=1;step();assert(!ui.help_card);press(B_HOME);assert(!ui.help&&!save_calls);
 }
 hold(B_SAVE);press(B_SAVE);assert(!save_calls);press(B_SAVE);assert(save_calls==1);
 press(B_EDIT);assert(ui.page==9);enc[EN_K2]=45;step();assert(cad_view.track_level[0]==145);enc[EN_K3]=20;step();assert(cad_view.punch==70);
 keys=1u<<whites[3];step();keys=0;step();assert(ui.track==3);enc[EN_K2]=-10;step();assert(cad_view.track_level[3]==90&&cad_view.track_level[0]==145);
 keys=1u<<blacks[3];step();keys=0;step();assert(cad_view.mutes==8);press(B_EDIT);assert(ui.page==1);press(B_HOME);
 enc[EN_K4]=5;step();assert(cad_view.par[P_VOLUME]==87);
 keys=1u<<whites[0];step();assert(cad_view.fx_mask==1);hold(B_FX);assert(!cad_view.fx_mask);keys=0;step();assert(!cad_view.fx_mask);
 unsigned tick=cad_view.tick;press(B_PLAY);for(int n=0;n<100;n++)step();assert(cad_view.tick>tick&&ui.help);
  /* MIDI must be consumed even while a help card owns the controls. */
 cad_command(A_PARAM,P_SYNC,1);step();midi_packet=0x0000fc0fu;step();assert(!cad_view.playing);
 for(int c=0;c<6;c++){ui.help_card=c;step();char name[32];snprintf(name,sizeof name,"help-%d",c);shot(argv[1],name);}
 press(B_HOME);now=0xffffff00u;hold(B_HOME);assert(ui.help);press(B_HOME);
 /* Booting with a held SAVE must not write on release. */
 buttons=1u<<B_SAVE;ui_init();buttons=0;step();assert(save_calls==1);
 puts("Sensei: 10 long holds, no accidental Save/mode/tap, pagination, mixer keys, FX release, live transport, MIDI and timer wrap passed");return 0;
}
