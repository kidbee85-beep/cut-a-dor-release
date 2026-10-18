/* SPDX-License-Identifier: GPL-3.0-only */
#include "cad.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void command(int a,int i,int v){assert(!cad_command(a,i,v));cad_render(0,0,4096);}
int main(void){
 cad_init();command(A_PARAM,P_EVOLVE,0);
 for(int track=0;track<8;track++)for(int st=0;st<32;st++){
  cad_project old=*cad_project_read();int part=old.part;
  command(A_STEP_VALUE,track,st+(37<<5));assert(cad_project_read()->pattern[part].velocity[track][st]==37);
  command(A_UNDO,0,0);assert(!memcmp(&old,cad_project_read(),sizeof old));
  command(A_STEP_DELAY,track,st+(0<<5));assert(cad_project_read()->pattern[part].delay[track][st]==-8);
  command(A_UNDO,0,0);assert(!memcmp(&old,cad_project_read(),sizeof old));
 }
 cad_project old=*cad_project_read();command(A_STEP_VALUE,8,4095);command(A_STEP_DELAY,0,544);command(A_STEP_VALUE,0,-1);assert(!memcmp(&old,cad_project_read(),sizeof old));
 command(A_STEP_VALUE,1,12+(43<<5));command(A_STEP_DELAY,1,12+(16<<5));command(A_EXPORT,0,123);cad_project saved=cad_transfer;command(A_NEW,0,0);cad_transfer=saved;command(A_IMPORT,0,0);assert(!memcmp(&saved,cad_project_read(),sizeof saved));
 unsigned early=0,late=0;
 for(int style=0;style<6;style++)for(int human=0;human<=100;human+=10)for(int swing=50;swing<=70;swing+=10){
  cad_project p=saved;p.par[P_STYLE]=style;p.par[P_HUMAN]=human;p.par[P_SWING]=swing;
  for(int tr=0;tr<8;tr++)for(int st=0;st<32;st++){
   p.pattern[p.part].delay[tr][st]=0;int at=cad_step_tick(&p,p.part,tr,st);assert(at>=0&&at<768);
   int reference=st*24+(st%2?(swing-50)*48/100:0);
   if(!human)assert(at==reference);
   if(tr==C_KICK&&st%16==0)assert(at==st*24);
   if(at>reference&&at-reference<24)late++;
   if(at<reference&&reference-at<24)early++;
  }
 }assert(early&&late);
 command(A_EXPORT,0,8);cad_transfer.count=2;cad_transfer.event[0]=(cad_event){24,0,0,1};cad_transfer.event[1]=(cad_event){48,0,0,0};command(A_IMPORT,0,0);
 command(A_PARAM,P_TAPE_BARS,4);assert(cad_project_read()->count==2);
 command(A_PARAM,P_TAPE_BARS,2);assert(!cad_project_read()->count);command(A_UNDO,0,0);assert(cad_project_read()->count==2&&cad_project_read()->par[P_TAPE_BARS]==4);
 printf("Step editor: 256 velocity/nudge/undo edits, packed bounds, save/import, live signed groove timing (%u early, %u late), unchanged tape length and undo passed.\n",early,late);
 return 0;
}
