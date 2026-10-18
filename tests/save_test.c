/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app/app.h"
static cad_project stored;static unsigned saves;static uint32_t now;
uint32_t plat_ms(void){cad_render(0,0,4096);return now++;}
int plat_store_save(uint32_t obj,const void *p,uint32_t n){assert(obj==OBJ_PROJ&&n==sizeof stored);memcpy(&stored,p,n);saves++;return 0;}
int plat_store_load(uint32_t obj,void *p,uint32_t n){assert(obj==OBJ_PROJ&&n==sizeof stored);memcpy(p,&stored,n);return n;}
#include "app/project.c"
static int32_t before[8192],after[8192];
static void setup(void){cad_init();cad_command(A_PARAM,P_NOISE,80);cad_command(A_PARAM,P_DUST,75);cad_command(A_PARAM,P_FILTER,55);cad_render(0,0,4096);}
static void sound(int32_t *p){cad_command(A_PLAY,0,1);cad_render(p,4096,4096);}
int main(void){setup();sound(before);assert(project_save()==-3&&!saves);
 setup();assert(!project_save()&&saves==1);sound(after);assert(!memcmp(before,after,sizeof before));
 cad_init();assert(!project_load());project_apply();sound(after);assert(!memcmp(before,after,sizeof before));
 puts("Save integration: stopped-only write; actual project_save and load/apply preserve the exact colored audio waveform");
}
