/* SPDX-License-Identifier: GPL-3.0-only */
#include "cad.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
static unsigned checks;
#define CHECK(c) do {checks++;if(!(c)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c);return 1;}}while(0)
static uint8_t banks[3][CAD_BANK_SIZE] __attribute__((aligned(8)));
static int32_t audio[512];
int main(int argc,char **argv){
 CHECK(argc==4);cad_init();
 for(int slot=0;slot<3;slot++){
  FILE *f=fopen(argv[slot+1],"rb");CHECK(f);size_t n=fread(banks[slot],1,CAD_BANK_SIZE,f);CHECK(!ferror(f));CHECK(fgetc(f)==EOF);fclose(f);
  CHECK(n>512&&n<=81920);CHECK(cad_validate_bank(banks[slot],n)==0);
  CHECK(cad_validate_bank(banks[slot],n-1)<0);banks[slot][n-1]^=1;CHECK(cad_validate_bank(banks[slot],n)<0);banks[slot][n-1]^=1;
  banks[slot][20]^=1;CHECK(cad_validate_bank(banks[slot],n)<0);banks[slot][20]^=1;CHECK(cad_set_bank(slot,banks[slot],n)==0);
 }
 for(int slot=0;slot<3;slot++){
  cad_command(A_PARAM,P_KIT,12+slot);cad_render(audio,0,4096);const cad_project *p=cad_project_read();
  for(int pad=0;pad<16;pad++){
   unsigned expected=128+16*slot+2*(pad%8)+(pad/8);CHECK(p->map[pad]==expected);cad_sample s;
   CHECK(cad_sample_get(expected,&s)==1);CHECK(s.frames>0&&s.rate>=16000&&s.rate<=22050);CHECK(s.category==pad%8);
   cad_command(A_PANIC,0,0);cad_command(A_PAD,pad,127);uint64_t energy=0;
   for(int block=0;block<173;block++){cad_render(audio,256,4096);for(unsigned k=0;k<512;k++)energy+=(audio[k]<0?-(int64_t)audio[k]:audio[k]);}
   CHECK(energy>0);CHECK(!cad_view.queue_overflows);
  }
  for(int fx=0;fx<8;fx++){
   cad_command(A_PANIC,0,0);cad_command(A_FX,fx,1);
   for(int pad=0;pad<16;pad++)cad_command(A_PAD,pad,127);
   for(int block=0;block<173;block++)cad_render(audio,256,4096);
   cad_command(A_CLEAN,0,0);cad_render(audio,0,4096);CHECK(!cad_view.queue_overflows);
  }
 }
 printf("PASS %u checks; 48 sounds validated, mapped and rendered; 24 all-16-pad FX stress runs; no sanitizer errors.\n",checks);return 0;
}
