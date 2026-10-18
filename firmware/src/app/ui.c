/* SPDX-License-Identifier: GPL-3.0-only
 * Cut-A-Dor: eleven direct pages, three keyboard modes, always-visible knob meanings. */
#define K_BG RGB(20,14,11)
#define K_PAPER RGB(255,241,214)
#define K_CORAL RGB(240,82,50)
#define K_GOLD RGB(232,172,76)
#define K_MUTED RGB(189,164,138)
#define K_INK RGB(56,38,27)
#define K_DIM RGB(139,84,46)
#define K_MOON RGB(244,214,155)
static const char *const pages[]={"LIVE DOJO","BEAT DNA","PUNCH FX","FX TAPE","PARTS","SOUND CRATE","TAPE COLOR","MIDI SYNC","STEP DOJO","DRUM MIX","SETUP"};
static const char *const modes[]={"DJ","DRUM","BREAK"};
static const char *const tracks[]={"KICK","SNARE","HAT","OPEN","CLAP","PERC","RIM","EXTRA"};
static const int8_t knobs[11][4]={{P_BPM,P_SWING,P_EVOLVE,P_VOLUME},{P_STYLE,P_DENSITY,P_HUMAN,P_SPACE},{P_FXRATE,P_FXDEPTH,P_DUST,P_TAPE_MIX},{P_TAPE_BARS,P_GRID,P_FXRATE,P_TAPE_MIX},{P_AUTO,P_BASS,P_ROOT,P_BREAKLEVEL},{P_KIT,P_BREAK,P_BREAKLEVEL,P_VOLUME},{P_FILTER,P_HIGHPASS,P_DUST,P_NOISE},{P_SYNC,-1,-2,P_BPM},{0,0,0,0},{0,0,0,P_VOLUME},{0,0,0,0}};
static const char *const labels[11][4]={{"BPM","SWING","EVOLVE","MASTER"},{"STYLE","DENSITY","HUMAN","SPACE"},{"RATE","DEPTH","DUST","TAPE MIX"},{"BARS","GRID","RATE","TAPE MIX"},{"AUTO FILL","BASS","ROOT","BREAK LVL"},{"KIT","BREAK","BREAK LVL","LEVEL"},{"LOW PASS","HIGH PASS","DUST","NOISE"},{"SOURCE","USB OUT","IN CH","BPM"},{"TRACK","BAR","VELOCITY","NUDGE"},{"TRACK","LEVEL %","PUNCH","MASTER"},{"KEY LED","GLOW","MOTION","SPLASH"}};
static const uint8_t whites[16]={0,2,4,5,7,9,11,12,14,16,17,19,21,23,24,26};
static const uint8_t blacks[11]={1,3,6,8,10,13,15,18,20,22,25};
static struct {uint32_t buttons,keys,last_draw,msg_until,tap,boot_until,held_at[NB],pending,consumed,oct_pending;uint8_t help,help_button,help_card,oct_chord;uint8_t page,mode,partpick,track,editbar,editstep;int16_t par[P_NPARAM];char msg[2][30];} ui;
static void num(char *b,int n){char tmp[12];int i=0,j=0;if(n<0){b[j++]='-';n=-n;}do{tmp[i++]=(char)('0'+n%10);n/=10;}while(n);while(i)b[j++]=tmp[--i];b[j]=0;}
static const char *value_text(int p,int v,char *b){
 static const char *rates[]={"1/4","1/8","1/16","1/32","1/64"};
 static const char *grid[]={"FREE","1/16","1/32"};
 static const char *notes[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
 if(p==P_SYNC)return (const char*[]){"INT","USB","JACK"}[v];
 if(p==-1)return v?"ON":"OFF";if(p==-2&&v==0)return "ANY";if(p==-2&&v==17)return "CLOCK";
 if(p==P_FXRATE)return rates[v];if(p==P_GRID)return grid[v];if(p==P_AUTO)return v?"ON":"OFF";
 if(p==P_ROOT){const char *n=notes[v%12];int j=0;while(*n)b[j++]=*n++;b[j++]=(char)('0'+v/12-1);b[j]=0;return b;}
 num(b,v);return b;
}
static void copystr(char *d,const char *s,unsigned n){unsigned i=0;while(i+1<n&&s[i]){d[i]=s[i];i++;}d[i]=0;}
void ui_say(const char *a,const char *b){copystr(ui.msg[0],a,30);copystr(ui.msg[1],b,30);ui.msg_until=plat_ms()+1600u;}
int ui_dirty(void){return 1;}
static void parameter(int p,int delta){
 if(p<0){cad_command(A_MIDI_CONFIG,p==-1?0:2,p==-1?(delta>0):delta);return;}
 cad_command(A_PARAM_DELTA,p,delta);
}
static int setup_page(void){return ui.page==7||ui.page==10;}
static void page(int p){if(ui.page==8||p==8||ui.page==9||p==9||setup_page()||p==7||p==10)cad_command(A_MODE,0,ui.mode);ui.page=(uint8_t)p;}
static void edit_knob(int i,int d){
 if(i==0)ui.track=(uint8_t)((ui.track+d%8+8)%8);
 else if(i==1)ui.editbar=(uint8_t)(d>0?1:0);
 else {int st=ui.editbar*16+ui.editstep;if(d< -1024)d=-1024;if(d>1024)d=1024;
  cad_command(A_STEP_DELTA,ui.track,st+((i==3)<<5)+((d+1024)<<6));
 }
}
static void mode(int m){if(ui.page==8||ui.page==9||setup_page())page(0);ui.mode=(uint8_t)((m+3)%3);cad_command(A_MODE,0,ui.mode);ui_say("KEYS",modes[ui.mode]);}
static void tap(void){uint32_t now=plat_ms(),d=now-ui.tap;if(d>=273&&d<=1500){ui.par[P_BPM]=(int16_t)(60000u/d);cad_command(A_PARAM,P_BPM,ui.par[P_BPM]);}ui.tap=now;ui_say("TAP TEMPO","KEEP TAPPING");}
static void key_action(int i,int down){
 if(setup_page())return;
 if(ui.page==9){if(down&&i<8)ui.track=(uint8_t)i;return;}
 if(ui.page==8){if(down){ui.editstep=(uint8_t)i;cad_command(A_STEP,ui.track,ui.editbar*16+i);}return;}
 if(ui.mode==1){if(down)cad_command(A_PAD,i,110);return;}
 if(ui.mode==2){if(down)cad_command(A_SLICE,i,110);return;}
 if(i<8){cad_command(A_FX,i,down);return;}if(!down)return;
 switch(i){case 8:cad_command(A_LOCK,-1,0);break;case 9:cad_command(A_NEW,0,0);break;case 10:cad_command(A_FILL,0,0);break;case 11:case 12:cad_command(A_PART,i-11,0);break;case 13:cad_command(A_PART,3,0);break;case 14:cad_command(A_UNDO,0,0);break;case 15:cad_command(A_CLEAN,0,0);break;}
}
static void short_button(int button){
 if(ui.help){ui.help=0;ui.msg_until=0;return;}
 switch(button){
 case B_HOME:page(0);mode(0);break;
 case B_SEL:mode(ui.mode+1);break;
 case B_EDIT:page(ui.page==9?1:9);break;
 case B_FX:page(ui.page==2?6:2);break;
 case B_ENV:page(3);break;
 case B_SEQ:page(ui.page==8?4:8);break;
 case B_LFO:page(5);break;
 case B_GLO:page(ui.page==10?7:10);break;
 case B_ARP:if(!setup_page())tap();break;
 case B_SAVE:{int r=project_save();ui_say(r?"NOT SAVED":"PROJECT SAVED",r==-3?"STOP FIRST":r?"TRY AGAIN":"READY FOR NEXT TIME");break;}
 }
}
void ui_input_only(void){
 if(cad_edit_busy)return;
 int was_help=ui.help;
 uint32_t b=plat_buttons(),k=plat_keys(),pressed=b&~ui.buttons,released=ui.buttons&~b,changed=k^ui.keys,now=plat_ms();
 ui.buttons=b;ui.keys=k;if(pressed||changed||cad_view.playing)ui.boot_until=0;
 uint32_t page_held=(b|released)&((1u<<(B_SEQ+1))-1u);
 if(page_held&&(page_held&(page_held-1u)))ui.consumed|=page_held;
 /* Page actions happen on release. A consumed hold must never also save,
  * switch mode, tap tempo or edit a beat. PLAY remains immediate. */
 for(int i=0;i<=B_SEQ;i++){
  uint32_t bit=1u<<i;
  if(pressed&bit){ui.held_at[i]=now;ui.pending|=bit;if(!(page_held&(page_held-1u)))ui.consumed&=~bit;}
  if((b&ui.pending&bit)&&!(ui.consumed&bit)&&now-ui.held_at[i]>=700u){
   ui.consumed|=bit;ui.help=1;ui.help_button=(uint8_t)i;ui.help_card=0;ui.msg_until=0;
   cad_command(A_MODE,0,ui.mode); /* Release live punches, including recorded releases. */
  }
  if(released&ui.pending&bit){if(!(ui.consumed&bit))short_button(i);ui.pending&=~bit;ui.consumed&=~bit;}
 }
 if(pressed&(1u<<B_PLAY)){if(ui.page==7&&!cad_view.playing)cad_command(A_CONTINUE,0,0);else cad_command(A_PLAY,0,!cad_view.playing);}
 if(!ui.help&&!setup_page()&&(pressed&(1u<<B_REC))){page(3);cad_command(A_RECORD,0,0);}
 /* Both OCT keys belong exclusively to recovery. Defer mode changes until
  * release, and consume both releases after any overlapping chord. */
 uint32_t oct=(1u<<B_OCTDN)|(1u<<B_OCTUP);
 ui.oct_pending|=pressed&oct;
 if((b&oct)==oct)ui.oct_chord=1;
 if(!ui.help&&!was_help&&!ui.oct_chord){if(released&ui.oct_pending&(1u<<B_OCTDN))mode(ui.mode-1);if(released&ui.oct_pending&(1u<<B_OCTUP))mode(ui.mode+1);}
 ui.oct_pending&=~released;
 if(!(b&oct))ui.oct_chord=0;
 if(!ui.help&&!was_help){
 for(int i=0;i<16;i++)if(changed&(1u<<whites[i]))key_action(i,(k>>whites[i])&1u);
 for(int i=0;i<11;i++)if(!setup_page()&&(changed&k&(1u<<blacks[i]))){if(ui.page==8){if(i<8)ui.track=(uint8_t)i;else if(i==8)ui.editbar^=1;else if(i==9)cad_command(A_MUTE,ui.track,0);else cad_command(A_UNDO,0,0);continue;}if(i<8){cad_command(A_MUTE,i,0);ui.track=(uint8_t)i;ui_say(tracks[i],cad_view.mutes&(1u<<i)?"UNMUTED / BLACK KEY":"MUTED / BLACK KEY");}else if(ui.page==9){if(i==10)cad_command(A_UNDO,0,0);}else if(i==8)tap();else if(i==9)cad_command(A_TAPE_TOGGLE,0,0);else cad_command(A_UNDO,0,0);}
 }
 int d=plat_enc(EN_SELECT);if(ui.help){
  d+=plat_enc(EN_PRESET);if(d)ui.help_card=(uint8_t)((ui.help_card+d%6+6)%6);
  (void)plat_enc(EN_ALGO);for(int i=0;i<4;i++)(void)plat_enc(EN_K1+i);
 }else {if(d&&((ui.boot_until=0),1))page((ui.page+d%11+11)%11);
 d=plat_enc(EN_ALGO);if(d&&((ui.boot_until=0),1)){if(setup_page()){ui_say("SETUP / MIDI","GLO: SWITCH PAGE");}else if(ui.page==9){cad_command(A_UNDO,0,0);ui_say("UNDO","LAST EDIT RESTORED");}else if(ui.page==8){cad_command(d>0?A_REROLL:A_UNDO,ui.track,0);ui_say(d>0?"LANE REMIX":"UNDO","STEP DOJO");}else {cad_command(d>0?A_NEW:A_MUTATE,0,0);ui_say(d>0?"NEW POCKET":"MUTATE","LANDS ON NEXT BAR");}}
 d=plat_enc(EN_PRESET);if(d&&((ui.boot_until=0),1)){if(setup_page()){}else if(ui.page==9){ui.track=(uint8_t)((ui.track+d%8+8)%8);}else if(ui.page==8){ui.editstep=(uint8_t)((ui.editstep+d%16+16)%16);}else if(ui.page==4){ui.partpick=(uint8_t)((ui.partpick+d%5+5)%5);cad_command(A_PART,ui.partpick,0);}else parameter(P_KIT,d);}
 for(int i=0;i<4;i++){d=plat_enc(EN_K1+i);if(d&&((ui.boot_until=0),1)){if(ui.page==10){cad_command(A_SETTING_DELTA,i,d);}else if(ui.page==9){if(i==0)ui.track=(uint8_t)((ui.track+d%8+8)%8);else if(i==1)cad_command(A_MIX_DELTA,ui.track,d);else if(i==2)cad_command(A_MIX_DELTA,8,d);else parameter(P_VOLUME,d);}else if(ui.page==8)edit_knob(i,d);else parameter(knobs[ui.page][i],d);}}
 }
 uint32_t pkt,stamp;int budget=64;while(budget--&&plat_midi_in(&pkt,&stamp)){ui.boot_until=0;cad_midi_receive(pkt,(pkt&0x10u)?2u:1u,stamp);}
 uint32_t leds=(cad_view.playing?1u<<B_PLAY:0)|(cad_view.record?1u<<B_REC:0),kl=0;
 static const uint8_t page_button[]={B_HOME,B_EDIT,B_FX,B_ENV,B_SEQ,B_LFO,B_FX,B_GLO,B_SEQ,B_EDIT,B_GLO};leds|=1u<<page_button[ui.page];
 if(setup_page()){}
 else if(ui.page==9){kl|=1u<<whites[ui.track];}
 else if(ui.page==8){for(int i=0;i<16;i++)if(cad_view.pattern.velocity[ui.track][ui.editbar*16+i])kl|=1u<<whites[i];kl|=1u<<blacks[ui.track];}
 else if(ui.mode==0){for(int i=0;i<8;i++)if(cad_view.fx_mask&(1u<<i))kl|=1u<<whites[i];}
 else if(cad_view.playing)kl|=1u<<whites[cad_view.step%16];
 if(ui.page!=8&&!setup_page())for(int i=0;i<8;i++)if(cad_view.mutes&(1u<<i))kl|=1u<<blacks[i];
 plat_lighting(cad_view.setting[S_LIGHT],cad_view.setting[S_GLOW]);plat_play_red(cad_view.record);plat_leds(leds,kl|(setup_page()?0:k));
}
/* Pixel theatre: bounded rectangles and lines, no bitmap assets or heap.
 * Punches change the pose; normal animation follows transport, never flashes. */
static void star(int x,int y,int r,uint16_t c){cv_line(x-r,y,x+r,y,c);cv_line(x,y-r,x,y+r,c);cv_pset(x-1,y-1,K_PAPER);}
static void ninja(int x,int y){
 int move=cad_view.playing&&cad_view.setting[S_MOTION]?(cad_view.step&3):0,hit=!!cad_view.fx_mask,bob=move==2?2:0;y+=bob;
 cv_rect(x+7,y+60,65,2,K_DIM); /* tatami */
 cv_rect(x+34,y+6,27,24,K_INK);cv_rect(x+31,y+9,33,18,K_DIM);
 cv_rect(x+31,y+12,33,9,K_CORAL);cv_rect(x+34,y+15,24,6,K_PAPER);
 cv_rect(x+37,y+15,6,3,K_BG);cv_rect(x+52,y+15,6,3,K_BG);
 /* Horned headwrap: Matador, but make it ninja. */
 cv_rect(x+31,y,6,9,K_MOON);cv_rect(x+28,y,3,3,K_MOON);cv_rect(x+58,y,6,9,K_MOON);cv_rect(x+64,y,3,3,K_MOON);
 cv_rect(x+16,y+15,15,6,K_CORAL);cv_rect(x+10-move*2,y+18,12+move*2,6,K_CORAL);
 cv_rect(x+34,y+30,24,21,K_INK);cv_rect(x+37,y+30,6,18,K_GOLD);cv_rect(x+34,y+45,24,3,K_CORAL);
 cv_rect(x+31,y+51,9,9,K_DIM);cv_rect(x+25-move,y+57,15,3,K_PAPER);
 cv_rect(x+52,y+51,9+hit*6,6,K_DIM);cv_rect(x+58+hit*6,y+54,12,3,K_PAPER);
 cv_rect(x+22-hit*6,y+33,12+hit*6,6,K_DIM);cv_rect(x+16-hit*6,y+30,9,9,K_MOON);
 cv_rect(x+58,y+30,9,6,K_DIM);cv_rect(x+64,y+24,6,9,K_MOON);
 if(hit){cv_line(x+3,y+8,x+19,y-2,K_GOLD);cv_line(x+2,y+12,x+16,y+6,K_PAPER);star(x+10,y+31,6,K_MOON);}
 else star(x+72,y+13,move+2,K_DIM);
}
static void reel(int x,int y,int phase){
 cv_rect(x-14,y-11,28,22,K_INK);cv_line(x-10,y-14,x+10,y-14,K_DIM);cv_line(x-10,y+14,x+10,y+14,K_DIM);
 star(x,y,5,K_CORAL);for(int i=0;i<4;i++){int a=(i+phase)%4;int xx=x+(a==0?9:a==2?-9:0),yy=y+(a==1?9:a==3?-9:0);cv_rect(xx-2,yy-2,4,4,K_GOLD);}
}
static void header(void){
 cv_rect(0,0,240,24,K_INK);cv_rect(0,23,240,1,K_DIM);star(10,11,5,K_CORAL);
 cv_text(22,4,&FONT_B,"CUT-A-DOR",K_PAPER);cv_text(144,6,&FONT_XS,cad_view.record==2?"REC":cad_view.record==1?"ARM":cad_view.playing?"RUN":"STOP",K_GOLD);
 cv_text(190,6,&FONT_XS,ui.help?"HELP":ui.page==10?"SETUP":ui.page==9?"MIX":ui.page==8?"STEP":modes[ui.mode],K_CORAL);
}
static void top(void){
 char b[16];cv_begin(240,112,K_BG);header();
 cv_text(9,31,&FONT_XS,pages[ui.page],K_GOLD);
 if(ui.page==0){num(b,cad_view.par[P_BPM]);cv_text(9,49,&FONT_L,b,K_PAPER);cv_text(72,68,&FONT_XS,"BPM",K_MUTED);cv_text(9,86,&FONT_B,cad_style_name(cad_view.par[P_STYLE]),K_GOLD);cv_text(104,66,&FONT_B,cad_view.part?"B":"A",K_CORAL);ninja(145,42);if(cad_view.fx_mask){int fx=0;while(!(cad_view.fx_mask&(1u<<fx)))fx++;cv_text(143,29,&FONT_XS,cad_fx_name(fx),K_CORAL);}}
 else if(ui.page==1){cv_text(9,49,&FONT_M,cad_style_name(cad_view.par[P_STYLE]),K_PAPER);num(b,(int)(cad_view.seed%1000000u));cv_text(9,78,&FONT_XS,"BEAT ID",K_MUTED);cv_text(55,78,&FONT_B,b,K_GOLD);cv_text(9,96,&FONT_XS,"NEW > / < MUTATE",K_MUTED);for(int i=0;i<16;i++){int h=cad_view.pattern.velocity[C_HAT][i]/9;cv_rect(133+i*6,106-h,4,h,K_GOLD);if(cad_view.pattern.velocity[C_SNARE][i])cv_rect(133+i*6,106-h-4,4,2,K_CORAL);}}
 else if(ui.page==2){for(int i=0;i<8;i++){int x=8+i%4*57,y=50+i/4*25;cv_rect(x,y,53,21,cad_view.fx_mask&(1u<<i)?K_CORAL:K_INK);cv_text(x+3,y+4,&FONT_XS,cad_fx_name(i),cad_view.fx_mask&(1u<<i)?K_BG:K_PAPER);}}
 else if(ui.page==3){
 cv_text(9,49,&FONT_B,cad_view.record==2?"RECORDING":cad_view.record==1?(cad_view.event_count?"NEXT LOOP":"NEXT BAR"):cad_view.tape_on?"TAPE PLAY":"FX TAPE",K_PAPER);
 num(b,cad_view.event_count);cv_text(9,71,&FONT_B,b,K_CORAL);cv_text(40,74,&FONT_XS,"EVENTS",K_MUTED);
 int spin=cad_view.playing&&cad_view.setting[S_MOTION]?(int)(cad_view.tick/24)%4:0;reel(164,70,spin);reel(211,70,spin);cv_line(164,84,211,84,K_MOON);
 cv_rect(9,100,221,3,K_INK);if(cad_view.playing)cv_rect(9,100,(cad_view.tick%(cad_view.par[P_TAPE_BARS]*CAD_BAR))*221/(cad_view.par[P_TAPE_BARS]*CAD_BAR),3,K_GOLD);
 }
 else if(ui.page==4){const char *ps[]={"PART A","PART B","INTRO 2 BARS","DROP 4 BARS","OUTRO 1 BAR"};cv_text(9,49,&FONT_M,ps[ui.partpick],K_PAPER);cv_text(9,81,&FONT_XS,"PRESETS: CHOOSE PART",K_GOLD);cv_text(9,96,&FONT_XS,"SEQ: STEPS / CHANGES NEXT BAR",K_MUTED);}
 else if(ui.page==7){cv_text(9,49,&FONT_M,"SYNC THE ARENA",K_PAPER);cv_text(9,80,&FONT_XS,ui.par[P_SYNC]?"FOLLOW ONE SOURCE / NO CLOCK ECHO":"INTERNAL TEMPO / USB CLOCK MASTER",K_GOLD);cv_text(9,98,&FONT_XS,cad_view.midi_out_overflows?"MIDI OVERFLOW / CHECK USB HOST":"USB OUT = CLOCK, START, STOP",K_MUTED);}
 else if(ui.page==6){cv_text(9,49,&FONT_M,"DUST / HISS / TONE",K_PAPER);for(int x=9;x<230;x+=3){int y=80+((x*7+(int)(cad_view.setting[S_MOTION]?cad_view.tick%31:0))%13)-6;cv_rect(x,y,2,2,K_DIM);}cv_text(9,98,&FONT_XS,"REC CAPTURES ALL FOUR KNOBS",K_GOLD);}
 else {cad_sample smp;cv_text(9,49,&FONT_M,cad_kit_name(cad_view.par[P_KIT]),K_PAPER);if(cad_sample_get(cad_break_sample((unsigned)cad_view.par[P_BREAK]),&smp))cv_text(9,82,&FONT_B,smp.name,K_GOLD);cv_text(9,99,&FONT_XS,"76 FACTORY SOUNDS + 3 USER BANKS",K_MUTED);}
 if((int32_t)(ui.msg_until-plat_ms())>0){cv_rect(4,46,232,56,K_INK);cv_text(10,53,&FONT_B,ui.msg[0],K_CORAL);cv_text(10,77,&FONT_S,ui.msg[1],K_PAPER);}
 cv_blit(0,0);
}
static void bottom(void){
 char b[16];cv_begin(240,128,K_BG);cv_rect(8,0,224,1,K_INK);
 for(int i=0;i<16;i++){int on=cad_view.playing&&cad_view.step%16==i;cv_rect(8+i*14,8,11,7,on?K_CORAL:i%4==0?K_GOLD:K_INK);}
 for(int i=0;i<8;i++){int x=8+i*28;cv_rect(x,23,24,4,K_INK);if(!(cad_view.mutes&(1u<<i)))cv_rect(x,23,2+cad_view.hits[i]*22/127,4,cad_view.hits[i]>70?K_CORAL:K_GOLD);cv_text(x,30,&FONT_XS,tracks[i][0]=='S'?"SN":tracks[i][0]=='K'?"KD":tracks[i][0]=='H'?"HH":tracks[i][0]=='O'?"OH":tracks[i][0]=='C'?"CP":tracks[i][0]=='P'?"PC":tracks[i][0]=='R'?"RM":"XT",K_MUTED);}
 for(int i=0;i<4;i++){int x=8+i*58,p=knobs[ui.page][i];cv_text(x,54,&FONT_XS,labels[ui.page][i],K_MUTED);int value=p==-1?(cad_view.midi_config&1u):p==-2?(cad_view.midi_config>>1):cad_view.par[p];cv_text(x,69,&FONT_M,value_text(p,value,b),K_PAPER);cv_rect(x,99,49,2,K_INK);int max=p==-1?1:p==-2?17:cad_parameter_limit(p,32767),min=p<0?0:cad_parameter_limit(p,-32767);int w=max==min?0:49*(value-min)/(max-min);cv_rect(x,99,w,2,K_CORAL);}
 cv_text(8,112,&FONT_XS,ui.page==7?"PLAY: RESUME / HOME THEN PLAY: START":ui.mode==0?"EDIT: DRUM MIX / HOLD PAGE KEY: HELP":ui.mode==1?"WHITE: DRUMS  /  BLACK: MUTES":"WHITE: 16 SLICES  /  BLACK: MUTES",K_GOLD);cv_blit(0,112);
}
static const char *const short_tracks[]={"KD","SN","HH","OH","CP","PC","RM","XT"};
static void step_rows(int first,int y){
 for(int r=0;r<4;r++){int t=first+r,yy=y+r*12;
  cv_text(5,yy,&FONT_XS,short_tracks[t],t==ui.track?K_CORAL:K_MUTED);
  for(int st=0;st<16;st++){int x=34+st*12,v=cad_view.pattern.velocity[t][ui.editbar*16+st];
   cv_rect(x,yy,10,9,v?(v<65?K_DIM:v>105?K_MOON:K_GOLD):K_INK);
   if(st%4==0&&!v)cv_rect(x,yy,2,9,K_DIM);
   if(t==ui.track&&st==ui.editstep){cv_rect(x,yy+9,10,2,K_CORAL);cv_rect(x,yy,10,1,K_PAPER);}
   if(cad_view.playing&&cad_view.step==ui.editbar*16+st)cv_rect(x+4,yy+2,2,5,K_CORAL);
  }
  if(cad_view.mutes&(1u<<t))cv_text(227,yy,&FONT_XS,"M",K_CORAL);
 }
}
static void step_screen(void){
 char b[16];cv_begin(240,112,K_BG);header();cv_text(8,29,&FONT_B,"STEP DOJO",K_GOLD);
 cv_text(151,32,&FONT_XS,cad_view.part?"B / BAR":"A / BAR",K_MUTED);num(b,ui.editbar+1);cv_text(212,29,&FONT_B,b,K_CORAL);
 cv_text(8,48,&FONT_XS,"BLACK: TRACK   WHITE: HIT ON/OFF",K_MUTED);step_rows(0,64);cv_blit(0,0);
 cv_begin(240,128,K_BG);step_rows(4,0);int st=ui.editbar*16+ui.editstep;
 const char *lab[]={"TRACK","BAR","VELOCITY","NUDGE"};
 for(int i=0;i<4;i++){int x=8+i*58;cv_text(x,56,&FONT_XS,lab[i],K_MUTED);
  if(i==0)cv_text(x,72,&FONT_B,short_tracks[ui.track],K_CORAL);
  else {num(b,i==1?ui.editbar+1:i==2?cad_view.pattern.velocity[ui.track][st]:cad_view.pattern.delay[ui.track][st]);cv_text(x,72,&FONT_M,b,K_PAPER);}
 }
 cv_text(8,101,&FONT_XS,"PRESETS: STEP / ALGO: REMIX, UNDO",K_GOLD);
 cv_text(8,115,&FONT_XS,"HOLD SEQ: HELP / HOME: LIVE DJ",K_MUTED);
 if((int32_t)(ui.msg_until-plat_ms())>0){cv_rect(3,51,234,46,K_INK);cv_text(9,55,&FONT_B,ui.msg[0],K_CORAL);cv_text(9,78,&FONT_XS,ui.msg[1],K_PAPER);}
 cv_blit(0,112);
}
/* SENSEI: numbered physical keys, left to right, no memorised note names. */
static const char *const help_context[10][8]={
 {"FX: PUNCHES / TAPE COLOR", "K1 RATE / K2 DEPTH", "K3 DUST / K4 TAPE MIX", "WHITE 1-8: HOLD A PUNCH", "PRESS FX AGAIN: TONE PAGE", "LOW PASS / HIGH PASS", "DUST / NOISE", "REC CAN CAPTURE TONE KNOBS"},
 {"SELECT: KEYBOARD MODE", "SHORT PRESS: DJ > DRUM > BREAK", "DJ: PUNCH FX + BEAT ACTIONS", "DRUM: WHITE 1-16 PLAY PADS", "BREAK: WHITE 1-16 PLAY SLICES", "BLACK 1-8: MUTE DRUM LANES", "STEP / MIX USE THEIR OWN MAP", "MODE CHANGES RELEASE LIVE FX"},
 {"ENVELOPE: FX TAPE", "REC: ARM / FINISH A TAKE", "FIRST TAKE STARTS NEXT BAR", "OVERDUB STARTS NEXT LOOP", "K1 BARS / K2 QUANTIZE", "K3 RATE / K4 TAPE MIX", "DJ BLACK 10: TAPE BYPASS", "DJ WHITE 15: UNDO LAST TAKE"},
 {"LFO: SOUND CRATE", "K1 KIT / K2 BREAK", "K3 BREAK LEVEL / K4 MASTER", "PRESETS: CHOOSE DRUM KIT", "SELECT: DRUM OR BREAK MODE", "BREAK LEVEL: AUTO BREAK LAYER", "LIVE SLICES USE PAD VELOCITY", "EDIT: INDIVIDUAL DRUM LEVELS"},
 {"EDIT: DRUM MIX / BEAT DNA", "K1 TRACK / K2 LEVEL 0-200%", "K3 PUNCH / K4 MASTER", "WHITE 1-8: PICK DRUM LANE", "BLACK 1-8: MUTE DRUM LANE", "PRESETS: TRACK / ALGO: UNDO", "EDIT AGAIN: STYLE + GROOVE", "DNA: DENSITY / HUMAN / SPACE"},
 {"GLOBAL: SETUP / MIDI SYNC", "GLO AGAIN: SWITCH THESE PAGES", "SETUP: LIGHT / GLOW / MOTION", "KNOB 4: BOOT SPLASH ON / OFF", "MIDI: SOURCE / USB OUT / IN CH", "MIDI KNOB 4: INTERNAL TEMPO", "PLAY ON MIDI: RESUME POSITION", "KEYS / PRESETS / ALGO: SAFE"},
 {"HOME: LIVE DOJO", "K1 BPM / K2 SWING", "K3 EVOLVE / K4 MASTER", "EDIT: DRUM MIX IN ONE PRESS", "SELECT: DJ / DRUM / BREAK", "SEQ: STEP EDITOR / PARTS", "PLAY: START OR STOP", "TURN PRESETS: MORE HELP CARDS"},
 {"SAVE: REMEMBER THIS PROJECT", "SHORT PRESS WHILE STOPPED", "SAVES BEAT, MIX AND FX TAPE", "ALSO SAVES MIDI AND KIT MAP", "LONG HOLD ONLY SHOWS HELP", "BROWSER: EXPORT A BACKUP", "NEW PROJECT FORMAT: V5", "KEEP OLD BACKUP TO DOWNGRADE"},
 {"ARP: TAP TEMPO", "SHORT PRESS TO TAP THE BEAT", "TAP 2 OR MORE TIMES", "RANGE: 40-219 BPM", "INTERNAL SYNC USES THIS TEMPO", "EXTERNAL CLOCK SETS THE PACE", "OCT - / +: RELEASE TO CHANGE", "BOTH OCT 5 SEC: RECOVERY"},
 {"SEQUENCE: STEP DOJO / PARTS", "WHITE 1-16: HIT ON / OFF", "BLACK 1-8: SELECT TRACK", "BLACK 9: BAR 1 / 2", "BLACK 10: MUTE SELECTED TRACK", "BLACK 11: UNDO", "PRESETS: STEP / K3: VELOCITY", "SEQ AGAIN: PARTS / PRESETS: PICK"}
};
static const char *const help_common[5][8]={
 {"DJ WHITE KEYS / 1-8", "1 ROLL       2 STUTTER", "3 REVERSE    4 BRAKE", "5 FILTER     6 ECHO", "7 CRUSH      8 GATE", "HOLD FX / RELEASE: BACK TO BEAT", "REC: CAPTURE YOUR PERFORMANCE", "FX TAPE LOOPS YOUR MOVES"},
 {"DJ WHITE KEYS / 9-16", "9 LOCK ALL   10 NEW BEAT", "11 FILL      12 PART A", "13 PART B    14 DROP", "15 UNDO      16 CLEAN", "PARTS LAND ON THE NEXT BAR", "INTRO / OUTRO: SEQ > PARTS", "PRESETS ON PARTS CHOOSES FORM"},
 {"BLACK KEYS / DJ, DRUM, BREAK", "1 KICK       2 SNARE", "3 HAT        4 OPEN HAT", "5 CLAP       6 PERCUSSION", "7 RIM        8 EXTRA", "9 TAP TEMPO  10 TAPE BYPASS", "11 UNDO", "LIT BLACK 1-8 = MUTED"},
 {"STEP DOJO / TWO BARS", "K1 TRACK     K2 BAR", "K3 VELOCITY  K4 EARLY / LATE", "WHITE 1-16: TOGGLE A HIT", "PRESETS: SELECT STEP TO EDIT", "ALGO RIGHT: REMIX THIS LANE", "ALGO LEFT: UNDO LAST EDIT", "SMALL VELOCITIES = GHOST HITS"},
 {"DRUM MIX / MAKE IT KICK", "WHITE 1-8: SELECT A TRACK", "BLACK 1-8: MUTE A TRACK", "K2: LEVEL, 100% = ORIGINAL", "K3 PUNCH: GAIN + ATTACK", "K4 MASTER: OVERALL LEVEL", "PUNCH HAS SOFT PEAK PROTECTION", "OLD PROJECTS START AT PUNCH 0"}
};
static void help_screen(void){
 const char *const *lines=ui.help_card?help_common[ui.help_card-1]:help_context[ui.help_button];char b[8];
 cv_begin(240,112,K_BG);header();cv_text(8,29,&FONT_B,"SENSEI",K_CORAL);num(b,ui.help_card+1);cv_text(181,29,&FONT_B,b,K_GOLD);cv_text(197,32,&FONT_XS,"/ 6",K_MUTED);
 for(int i=0;i<4;i++)cv_text(8,51+i*16,&FONT_XS,lines[i],i?K_PAPER:K_GOLD);cv_blit(0,0);
 cv_begin(240,128,K_BG);for(int i=4;i<8;i++)cv_text(8,3+(i-4)*16,&FONT_XS,lines[i],K_PAPER);
 /* Key silhouette is a counting aid; numbering always starts on the left. */
 for(int i=0;i<16;i++){cv_rect(8+i*14,74,12,19,K_PAPER);if(i==0||i==7||i==15){num(b,i+1);cv_text(8+i*14,79,&FONT_XS,b,K_BG);}}
 for(int i=0;i<11;i++){int x=8;for(int j=0;j<16;j++)if(whites[j]<blacks[i])x=8+j*14+9;cv_rect(x,74,7,9,K_BG);}
 cv_text(8,98,&FONT_XS,"PRESETS / SELECT: NEXT HELP CARD",K_GOLD);cv_text(8,114,&FONT_XS,"SHORT PAGE KEY: CLOSE / PLAY: RUN",K_MUTED);cv_blit(0,112);
}
static void mix_screen(void){
 char b[16];cv_begin(240,112,K_BG);header();cv_text(8,29,&FONT_B,"DRUM MIX",K_GOLD);cv_text(133,32,&FONT_XS,"EDIT AGAIN: DNA",K_MUTED);
 for(int t=0;t<4;t++){int y=52+t*15;cv_text(8,y,&FONT_XS,tracks[t],t==ui.track?K_CORAL:K_PAPER);cv_rect(57,y+3,118,6,K_INK);cv_rect(57,y+3,cad_view.track_level[t]*118/200,6,cad_view.mutes&(1u<<t)?K_DIM:t==ui.track?K_CORAL:K_GOLD);num(b,cad_view.track_level[t]);cv_text(184,y,&FONT_XS,b,K_PAPER);if(cad_view.mutes&(1u<<t))cv_text(216,y,&FONT_XS,"M",K_CORAL);}
 cv_blit(0,0);cv_begin(240,128,K_BG);
 for(int t=4;t<8;t++){int y=(t-4)*15;cv_text(8,y,&FONT_XS,tracks[t],t==ui.track?K_CORAL:K_PAPER);cv_rect(57,y+3,118,6,K_INK);cv_rect(57,y+3,cad_view.track_level[t]*118/200,6,cad_view.mutes&(1u<<t)?K_DIM:t==ui.track?K_CORAL:K_GOLD);num(b,cad_view.track_level[t]);cv_text(184,y,&FONT_XS,b,K_PAPER);if(cad_view.mutes&(1u<<t))cv_text(216,y,&FONT_XS,"M",K_CORAL);}
 for(int i=0;i<4;i++){int x=8+i*58;cv_text(x,65,&FONT_XS,labels[9][i],K_MUTED);if(!i)cv_text(x,81,&FONT_B,short_tracks[ui.track],K_CORAL);else {num(b,i==1?cad_view.track_level[ui.track]:i==2?cad_view.punch:cad_view.par[P_VOLUME]);cv_text(x,81,&FONT_M,b,K_PAPER);}}
 cv_text(8,113,&FONT_XS,"WHITE: TRACK / BLACK: MUTE / HOLD: HELP",K_GOLD);cv_blit(0,112);
}
static const char *setting_value(int i,int v){static const char *const light[]={"OFF","LOW","MID","HIGH"};return i<2?light[v]:v?"ON":"OFF";}
static void settings_screen(void){
 char b[16];cv_begin(240,112,K_BG);header();cv_text(8,30,&FONT_B,"SETUP / MAKE IT YOURS",K_GOLD);
 cv_text(8,52,&FONT_XS,"KEY LED = ACTIVE BUTTONS + KEYS",K_PAPER);cv_text(8,69,&FONT_XS,"GLOW = IDLE BUTTON BACKLIGHT",K_PAPER);
 cv_text(8,88,&FONT_XS,"GLO AGAIN: MIDI SYNC / HOLD: HELP",K_GOLD);cv_blit(0,0);
 cv_begin(240,128,K_BG);for(int i=0;i<4;i++){int x=8+i*58;cv_text(x,8,&FONT_XS,labels[10][i],K_MUTED);cv_text(x,28,&FONT_B,setting_value(i,cad_view.setting[i]),K_PAPER);}
 cv_text(8,63,&FONT_XS,"AUDIO CPU",K_MUTED);num(b,(int)plat_cpu_pct());cv_text(67,63,&FONT_XS,b,K_GOLD);cv_text(116,63,&FONT_XS,"XRUNS",K_MUTED);num(b,(int)plat_xruns());cv_text(157,63,&FONT_XS,b,plat_xruns()?K_CORAL:K_GOLD);
 cv_text(8,82,&FONT_XS,"STOP + SHORT SAVE: KEEP SETTINGS",K_PAPER);cv_text(8,98,&FONT_XS,"KEYS OFF / PLAY STILL WORKS",K_GOLD);cv_text(8,112,&FONT_XS,"SELECT / OCT: BACK TO LIVE MODE",K_MUTED);cv_blit(0,112);
}
static void message_overlay(void){if((int32_t)(ui.msg_until-plat_ms())<=0)return;cv_begin(232,56,K_INK);cv_text(6,7,&FONT_B,ui.msg[0],K_CORAL);cv_text(6,31,&FONT_S,ui.msg[1],K_PAPER);cv_blit(4,46);}
static void boot_screen(void){
 cv_begin(240,112,K_BG);uint32_t elapsed=2600u-(ui.boot_until-plat_ms());
 cv_text(44,9,&FONT_XS,"SENSEI SAYS: MAKE IT KICK",K_GOLD);
 /* Gate, moon and tatami are code-drawn. The mask is our own Matador ninja. */
 cv_rect(44,30,152,5,K_CORAL);cv_rect(51,35,138,4,K_DIM);cv_rect(62,39,5,70,K_CORAL);cv_rect(173,39,5,70,K_CORAL);
 cv_rect(50,48,140,3,K_MOON);ninja(77,47);
 if(elapsed>700)star(200,65,6,K_GOLD);if(elapsed>1400)star(36,78,5,K_MOON);cv_blit(0,0);
 cv_begin(240,128,K_BG);int w=text_w(&FONT_L,"CUT-A-DOR");cv_text((240-w)/2,7,&FONT_L,"CUT-A-DOR",K_PAPER);
 cv_text(54,51,&FONT_S,"SMALL BOX. BIG KICK.",K_GOLD);
 cv_text(51,73,&FONT_XS,"THE MATADOR OF THE CUT",K_MOON);
 cv_rect(36,97,168,3,K_INK);cv_rect(36,97,168*(elapsed<2600?elapsed:2600)/2600,3,K_CORAL);
 cv_text(58,110,&FONT_XS,"TOUCH A CONTROL TO ENTER",K_MUTED);cv_blit(0,112);
}
void ui_init(void){memset(&ui,0,sizeof ui);for(int i=0;i<P_NPARAM;i++)ui.par[i]=cad_view.par[i];ui.keys=plat_keys();ui.buttons=plat_buttons();ui.boot_until=cad_view.setting[S_SPLASH]?plat_ms()+2600u:0;plat_lighting(cad_view.setting[S_LIGHT],cad_view.setting[S_GLOW]);lcd_fill(0,0,240,240,K_BG);}
void ui_frame(void){ui_input_only();if(plat_ms()-ui.last_draw<33u)return;ui.last_draw=plat_ms();for(int i=0;i<P_NPARAM;i++)ui.par[i]=cad_view.par[i];if(ui.boot_until&&(int32_t)(ui.boot_until-plat_ms())>0){boot_screen();return;}if(ui.help)help_screen();else if(ui.page==10)settings_screen();else if(ui.page==9)mix_screen();else if(ui.page==8)step_screen();else {top();bottom();}message_overlay();}
