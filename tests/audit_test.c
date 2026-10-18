/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "cad.h"
static int32_t audio[512];
static void cmd(int a,int i,int v){assert(!cad_command(a,i,v));cad_render(0,0,4096);}
int main(void){
 cad_init();cad_render(0,0,4096);
 for(int i=0;i<20;i++){cad_command(A_MIX_DELTA,0,1);cad_command(A_MIX_DELTA,8,1);cad_command(A_PARAM_DELTA,P_BPM,1);cad_command(A_STEP_DELTA,1,4+((1+1024)<<6));}
 cad_render(0,0,4096);assert(cad_view.track_level[0]==120&&cad_view.punch==70&&cad_view.par[P_BPM]==112);
 int velocity=cad_view.pattern.velocity[1][4];assert(velocity>=20);
 for(int i=0;i<3;i++)cad_command(A_STEP_DELTA,1,4+(1<<5)+((1+1024)<<6));cad_render(0,0,4096);assert(cad_view.pattern.delay[1][4]==3);
 cmd(A_PART,2,0);assert(cad_view.pending_part==2);cmd(A_PART,1,0);assert(cad_view.pending_part==255&&cad_view.part==1);cmd(A_PLAY,0,1);cad_render(audio,1,4096);assert(cad_view.section==0);
 cad_init();for(int t=0;t<8;t++)cmd(A_MUTE,t,0);cmd(A_PARAM,P_NOISE,0);cmd(A_FILL,0,0);cmd(A_PLAY,0,1);
 for(int b=0;b<1500;b++){cad_render(audio,256,4096);for(int i=0;i<512;i++)assert(audio[i]==0);}
 // A valid saved FX tape must start enabled and survive import; no old pending
 // sections or undo history may leak into a different imported project.
 cad_init();cmd(A_EXPORT,0,1);cad_project saved=cad_transfer;
 saved.count=2;saved.event[0]=(cad_event){0,0,FX_GATE,1};saved.event[1]=(cad_event){100,0,FX_GATE,0};
 cmd(A_NEW,0,0);cmd(A_PART,3,0);cad_transfer=saved;cmd(A_IMPORT,0,0);
 assert(cad_view.pending_part==255&&cad_view.section==0&&cad_view.tape_on==1);unsigned seed=cad_view.seed;cmd(A_UNDO,0,0);assert(cad_view.seed==seed);
 cmd(A_PLAY,0,1);cad_render(audio,1,4096);assert(cad_view.fx_mask==(1u<<FX_GATE));
 cad_init();cmd(A_TRACK_LEVEL,0,150);cmd(A_SETTING,S_LIGHT,0);cmd(A_UNDO,0,0);assert(cad_view.track_level[0]==100&&cad_view.setting[S_LIGHT]==0);
 cmd(A_SETTING,S_GLOW,3);cmd(A_SETTING,S_MOTION,0);cmd(A_SETTING,S_SPLASH,0);cmd(A_EXPORT,0,1);saved=cad_transfer;
 cad_init();cad_transfer=saved;cmd(A_IMPORT,0,0);assert(!memcmp(cad_project_read()->setting,saved.setting,4));
 cad_transfer=saved;cad_transfer.version=4;assert(!cad_upgrade_transfer(2672));assert(cad_transfer.version==5&&cad_transfer.setting[S_LIGHT]==3&&cad_transfer.setting[S_GLOW]==1&&cad_transfer.setting[S_MOTION]==1&&cad_transfer.setting[S_SPLASH]==1);
 cad_transfer=saved;cad_transfer.setting[S_LIGHT]=4;assert(cad_validate_project(&cad_transfer));cad_transfer=saved;cad_transfer.setting[S_SPLASH]=2;assert(cad_validate_project(&cad_transfer));
 puts("Audit: fast queued edits accumulate; fills obey mutes; stopped part cancels old intro; imports clear stale actions/undo and restore tape; settings persist and migrate v4 -> v5");
}
