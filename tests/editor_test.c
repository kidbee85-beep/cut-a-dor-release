/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "cad.h"
#define EN_VERSION "TEST"
static uint8_t flash[1048576],reply[320],cad_edit_busy;static uint32_t fm1_ms;static int flash_ok=1,erases,writes;
static uint32_t irq_save(void){return 0;}static void irq_restore(uint32_t x){(void)x;}static void fl_inval(uint32_t a,uint32_t b){(void)a;(void)b;}static void fm1_wdt_feed(void){}static void audio_silence(void){}
static int project_snapshot(void){cad_command(A_EXPORT,0,1);cad_render(0,0,0);return 0;}
static const uint8_t *fm1_xip_ptr(uint32_t off){assert(off<sizeof flash);return flash+off;}
static int fl_erase4k(uint32_t off,uint32_t *t){assert(off>=0xa0000&&off+4096<=0xdc000&&off%4096==0);memset(flash+off,255,4096);*t=1;erases++;return 0;}
static int fl_write(uint32_t off,const void *src,uint32_t n){assert(off>=0xa0000&&off+n<=0xdc000);const uint8_t *p=src;for(unsigned i=0;i<n;i++){assert((flash[off+i]&p[i])==p[i]);flash[off+i]=p[i];}writes++;return 0;}
static int ota_wire_send(const uint8_t *p,uint32_t n){assert(n<=sizeof reply);memcpy(reply,p,n);return 0;}
#include "app/editor.c"
static int call(int cmd,const uint8_t *payload,unsigned n){uint8_t frame[640]={125,67,65,68,0,5};frame[4]=(uint8_t)cmd;if(n)memcpy(frame+6,payload,n);assert(ced_frame(frame,n+6));return reply[7];}
static void number(uint8_t *a,unsigned n){a[0]=n&127;a[1]=(n>>7)&127;a[2]=(n>>14)&127;}
static void chunk(const uint8_t *data,unsigned off,unsigned n){uint8_t a[280]={0};number(a+1,off);unsigned p=4;for(unsigned i=0;i<n;){unsigned k=n-i;if(k>7)k=7;unsigned m=0;for(unsigned j=0;j<k;j++)m|=(data[i+j]>>7)<<j;a[p++]=(uint8_t)m;for(unsigned j=0;j<k;j++)a[p++]=data[i++]&127;}assert(call(11,a,p)==0);}
int main(void){memset(flash,255,sizeof flash);cad_init();cad_render(0,0,0);ced_scan();assert(call(1,0,0)==0);uint8_t legacy[]={125,70,76,1};assert(!ced_frame(legacy,4));
 uint8_t bank[644]={0};cad_bank *h=(cad_bank*)bank;h->magic=CAD_BANK_MAGIC;h->version=1;h->count=1;h->data_len=132;h->zone[0]=(cad_zone){0,256,16000,0,0,"TEST KICK"};h->data_crc=cad_crc32(bank+512,132);h->header_crc=cad_crc32(h,512);uint8_t begin[4]={0};number(begin+1,sizeof bank);
 cad_command(A_PLAY,0,1);cad_render(0,0,0);assert(call(10,begin,4)!=0);assert(!erases);cad_command(A_PLAY,0,0);cad_render(0,0,0);
 assert(call(10,begin,4)==0);assert(cad_edit_busy&&erases==20);uint8_t bad[5]={0,4,0,0,0};assert(call(11,bad,5)!=0);
 for(unsigned off=0;off<sizeof bank;off+=224){unsigned n=sizeof bank-off;if(n>224)n=224;chunk(bank+off,off,n);assert(flash[0xa0000]==255);}
 uint8_t end[]={0};assert(call(12,end,1)==0);assert(!cad_edit_busy);assert(!cad_validate_bank(flash+0xa0000,sizeof bank));assert(writes==2);
 assert(call(10,begin,4)==0);chunk(bank,0,224);fm1_ms=16000;ced_poll();assert(!cad_edit_busy&&!ced_active);assert(cad_validate_bank(flash+0xa0000,sizeof bank));assert(call(11,bad,5)!=0);
 puts("Editor: identity, stopped-only write, slot bounds, packet order, header-last commit, CRC and timeout passed");return 0;}
