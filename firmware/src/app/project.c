/* SPDX-License-Identifier: GPL-3.0-only */
static int project_loaded;
int project_load(void){
 int n=plat_store_load(OBJ_PROJ,&cad_transfer,sizeof cad_transfer);
 project_loaded=n>0&&!cad_upgrade_transfer((unsigned)n);return project_loaded?0:-1;
}
void project_apply(void){if(project_loaded){cad_command(A_IMPORT,0,0);cad_render(0,0,0);}}
int project_snapshot(void){
 static uint32_t serial;uint32_t want=++serial,t=plat_ms();
 if(cad_command(A_EXPORT,0,(int)want))return -1;
 while(cad_transfer_ack!=want){
#ifndef EN_HOST
 fm1_wdt_feed();
#endif
 if(plat_ms()-t>100u)return -2;
 }return 0;
}
int project_save(void){
 if(cad_view.playing)return -3;
 if(cad_command(A_QUIET,0,0)||project_snapshot())return -2;
 return plat_store_save(OBJ_PROJ,&cad_transfer,sizeof cad_transfer);
}
