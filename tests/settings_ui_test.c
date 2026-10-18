/* SPDX-License-Identifier: GPL-3.0-only */
#define main display_main
#include "../host/display.c"
#undef main
int main(int argc,char **argv){(void)argc;(void)argv;cad_init();cad_render(0,0,4096);ui_init();press(B_GLO);assert(ui.page==10);
 for(int i=0;i<4;i++)enc[EN_K1+i]=-100;step();step();assert(!light_level&&!glow_level);for(int i=0;i<4;i++)assert(!cad_view.setting[i]);
 unsigned seed=cad_view.seed,mutes=cad_view.mutes,kit=cad_view.par[P_KIT];
 for(int page0=7;page0<=10;page0+=3){ui.page=page0;for(int mode0=0;mode0<3;mode0++){ui.mode=mode0;
  for(int key=0;key<27;key++){keys=1u<<key;step();keys=0;step();}
  enc[EN_ALGO]=1;enc[EN_PRESET]=3;step();assert(cad_view.seed==seed&&cad_view.mutes==mutes&&cad_view.par[P_KIT]==kit&&!cad_view.fx_mask&&!cad_view.record);
  press(B_REC);assert(!cad_view.record);press(B_ARP);assert(ui.tap==0);
 }}
 // Repeated events arriving before the audio callback must all count.
 press(B_HOME);press(B_EDIT);for(int n=0;n<7;n++){enc[EN_K2]=1;ui_input_only();}cad_render(0,0,4096);assert(cad_view.track_level[0]==107);
 // Recovery chord never changes mode, including staggered key release.
 press(B_HOME);unsigned m=ui.mode;buttons=1u<<B_OCTDN;step();buttons|=1u<<B_OCTUP;step();buttons=1u<<B_OCTUP;step();buttons=0;step();assert(ui.mode==m);
 press(B_OCTUP);assert(ui.mode==(m+1)%3);
 // Releasing a key held during startup must not perform a fresh action.
 buttons=1u<<B_OCTUP;ui_init();m=ui.mode;buttons=0;step();assert(ui.mode==m);
 // Concurrent page keys including SAVE have no short side effect.
 for(int b=0;b<=B_SEQ;b++){if(b==B_SAVE)continue;buttons=(1u<<B_SAVE)|(1u<<b);step();buttons=1u<<B_SAVE;step();buttons=0;step();assert(!save_calls);}
 // A muted startup splash preference survives UI initialization.
 ui_init();assert(!ui.boot_until);cad_command(A_SETTING,S_SPLASH,1);cad_render(0,0,4096);ui_init();assert(ui.boot_until);
 puts("Settings UI: all 27 keys in 3 modes safe on SETUP/MIDI; big encoders non-destructive; live lighting; fast mixer turns; exclusive recovery chord; SAVE chords suppressed; splash preference passed");
}
