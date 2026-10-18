/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "cad.h"
static void cmd(int a,int i,int v){assert(!cad_command(a,i,v));cad_render(0,0,4096);}
int main(void){
 _Static_assert(sizeof(cad_project)==2676,"v5 layout");
 cad_init();cad_render(0,0,4096);assert(cad_view.punch==50);
 cmd(A_TRACK_LEVEL,1,175);assert(cad_view.track_level[1]==175);cmd(A_UNDO,0,0);assert(cad_view.track_level[1]==100);
 cmd(A_PUNCH,0,999);assert(cad_view.punch==100);cmd(A_UNDO,0,0);assert(cad_view.punch==50);
 cmd(A_TRACK_LEVEL,0,-100);cmd(A_TRACK_LEVEL,7,999);cmd(A_PUNCH,0,-1);cmd(A_EXPORT,0,1);
 assert(cad_transfer.track_level[0]==0&&cad_transfer.track_level[7]==200&&cad_transfer.punch==0);
 cad_project saved=cad_transfer;cmd(A_TRACK_LEVEL,0,100);cad_transfer=saved;cmd(A_IMPORT,0,0);assert(cad_view.track_level[0]==0&&cad_view.track_level[7]==200);
 cad_transfer=saved;cad_transfer.track_level[0]=201;assert(cad_validate_project(&cad_transfer));
 cad_transfer=saved;cad_transfer.reserved[2]=1;assert(cad_validate_project(&cad_transfer));
 cad_transfer=saved;cad_transfer.punch=101;assert(cad_validate_project(&cad_transfer));
 for(unsigned v=2;v<=3;v++){cad_transfer=saved;cad_transfer.version=v;cad_transfer.midi_config=v==2?255:21;assert(!cad_upgrade_transfer(2660));assert(cad_transfer.version==5&&cad_transfer.punch==0);for(int t=0;t<8;t++)assert(cad_transfer.track_level[t]==100);assert(cad_transfer.midi_config==(v==2?0:21));assert(!memcmp(cad_transfer.pattern,saved.pattern,sizeof saved.pattern));}
 cad_transfer=saved;assert(cad_upgrade_transfer(2660));cad_transfer.version=3;assert(cad_upgrade_transfer(2672));
 cad_init();cad_render(0,0,4096);cmd(A_MUTE,2,0);assert(cad_view.dirty&&cad_view.mutes==4);
 puts("Mixer: bounds, independent lane levels, undo, v2/v3 migration, corrupt sizes and saved state passed");
}
