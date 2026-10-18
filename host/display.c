/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "cad.h"
#include "app/app.h"
static int light_level,glow_level;
static int save_calls;static uint32_t midi_packet;
static uint16_t pixels[240*240];static uint32_t now,buttons,keys;static int enc[NE];static uint8_t cad_edit_busy;
static void lcd_sync(void){}
static void lcd_fill(uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint16_t c){for(unsigned j=y;j<y+h&&j<240;j++)for(unsigned i=x;i<x+w&&i<240;i++)pixels[j*240+i]=c;}
static void lcd_blit(uint32_t x,uint32_t y,uint32_t w,uint32_t h,const uint16_t *p){for(unsigned j=0;j<h&&j+y<240;j++)for(unsigned i=0;i<w&&i+x<240;i++){uint16_t c=p[j*w+i];pixels[(j+y)*240+i+x]=(c>>8)|(c<<8);}}
#include "gfx.c"
uint32_t plat_ms(void){return now;}uint32_t plat_buttons(void){return buttons;}uint32_t plat_keys(void){return keys;}
int32_t plat_enc(int i){int v=enc[i];enc[i]=0;return v;}uint32_t plat_master(void){return 4096;}void plat_leds(uint32_t b,uint32_t k){(void)b;(void)k;}void plat_glow(int x){(void)x;}void plat_lighting(int x,int y){light_level=x;glow_level=y;}uint32_t plat_cpu_pct(void){return 24;}uint32_t plat_xruns(void){return 0;}void plat_play_red(int x){(void)x;}
int plat_midi_in(uint32_t *p,uint32_t *t){if(midi_packet){*p=midi_packet;*t=now;midi_packet=0;return 1;}return 0;}void plat_midi_out(uint32_t p){(void)p;}int project_save(void){save_calls++;return cad_view.playing?-3:0;}
#include "app/ui.c"
static int32_t audio[512];
static void step(void){now+=40;cad_render(audio,256,4096);ui_frame();cad_render(audio,0,4096);}
static void press(int b){buttons=1u<<b;step();buttons=0;step();}
int main(int argc,char **argv){if(argc!=2)return 1;cad_init();cad_render(0,0,4096);ui_init();
 press(B_SEL);if(cad_view.mode!=1)return 2;press(B_SEL);if(cad_view.mode!=2)return 3;press(B_HOME);if(cad_view.mode!=0)return 4;
 press(B_FX);if(ui.page!=2)return 6;press(B_FX);if(ui.page!=6)return 7;enc[EN_K4]=27;step();if(cad_view.par[P_NOISE]!=37)return 8;press(B_HOME);
 press(B_GLO);assert(ui.page==10);press(B_GLO);if(ui.page!=7)return 9;
 enc[EN_K2]=1;step();if(!(cad_view.midi_config&1))return 10;
 enc[EN_K3]=10;step();if((cad_view.midi_config>>1)!=10)return 11;
 enc[EN_K1]=2;step();if(cad_view.par[P_SYNC]!=2)return 12;
 enc[EN_K3]=-99;step();if(cad_view.midi_config>>1)return 13;
 enc[EN_K1]=-2;step();enc[EN_K2]=-1;step();
 press(B_HOME);keys=1u<<whites[0];step();assert(cad_view.fx_mask&1);press(B_SEQ);assert(ui.page==8&&!cad_view.fx_mask);keys=0;step();
 keys=1u<<blacks[1];step();keys=0;step();assert(ui.track==1);
 enc[EN_K2]=1;step();assert(ui.editbar==1);
 int v=cad_view.pattern.velocity[1][31];keys=1u<<whites[15];step();keys=0;step();assert(ui.editstep==15&&cad_view.pattern.velocity[1][31]!=(unsigned)v);
 enc[EN_K3]=-1000;step();assert(!cad_view.pattern.velocity[1][31]);enc[EN_K3]=43;step();assert(cad_view.pattern.velocity[1][31]==43);
 enc[EN_K4]=-100;step();assert(cad_view.pattern.delay[1][31]==-8);
 keys=1u<<blacks[10];step();keys=0;step();assert(cad_view.pattern.delay[1][31]==0);
 press(B_SEQ);assert(ui.page==4);press(B_SEQ);assert(ui.page==8);press(B_HOME);assert(ui.page==0);
 now+=2000;cad_command(A_PLAY,0,1);for(int i=0;i<11;i++){
  ui.page=(uint8_t)i;ui.msg_until=0;step();char path[1024];snprintf(path,sizeof path,"%s/screen-%d.ppm",argv[1],i);FILE *f=fopen(path,"wb");if(!f)return 5;fprintf(f,"P6\n240 240\n255\n");for(unsigned p=0;p<240*240;p++){uint16_t c=pixels[p];fputc((c>>11)*255/31,f);fputc(((c>>5)&63)*255/63,f);fputc((c&31)*255/31,f);}fclose(f);
 }
 for(int frame=0;frame<24;frame++){
  if(frame==8)cad_command(A_FX,0,1);if(frame==13)cad_command(A_FX,0,0);
  for(int n=0;n<32;n++)cad_render(audio,256,4096);ui.page=0;ui.msg_until=0;step();
  char path[1024];snprintf(path,sizeof path,"%s/live-%02d.ppm",argv[1],frame);FILE *f=fopen(path,"wb");if(!f)return 20;
  fprintf(f,"P6\n240 240\n255\n");for(unsigned p=0;p<240*240;p++){uint16_t c=pixels[p];fputc((c>>11)*255/31,f);fputc(((c>>5)&63)*255/63,f);fputc((c&31)*255/31,f);}fclose(f);
 }
 puts("UI: DJ/DRUM/BREAK and HOME routes passed; step grid editing and MIDI knobs; eleven native 240x240 screens rendered");return 0;
}
