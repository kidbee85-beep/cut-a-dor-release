/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "app/plat.h"
static uint8_t flash[1048576],baseline[1048576];static int remaining=-1;
static int st_read(uint32_t off,void *dst,uint32_t n){assert(off+n<=sizeof flash);memcpy(dst,flash+off,n);return 0;}
static int st_erase(uint32_t off){assert(off==0xfc000||off==0xfd000||off==0xdc000||off==0xdd000);if(!remaining)return -1;memset(flash+off,255,4096);return 0;}
static int st_prog(uint32_t off,const void *src,uint32_t n){const uint8_t *p=src;for(unsigned i=0;i<n;i++){if(remaining==0)return -1;flash[off+i]&=p[i];if(remaining>0)remaining--;}return 0;}
#include "storage.c"
int main(void){memset(flash,255,sizeof flash);uint8_t a[2676],b[2676],out[2676];memset(a,17,sizeof a);memset(b,85,sizeof b);assert(st_save(OBJ_PROJ,a,sizeof a)==0);memcpy(baseline,flash,sizeof flash);
 for(int cut=0;cut<2710;cut++){memcpy(flash,baseline,sizeof flash);remaining=cut;st_save(OBJ_PROJ,b,sizeof b);assert(st_load(OBJ_PROJ,out,sizeof out)==sizeof out);assert(!memcmp(out,a,sizeof a)||!memcmp(out,b,sizeof b));}
 puts("Storage: 2,710 interrupted writes always recover old or complete new project");return 0;}
