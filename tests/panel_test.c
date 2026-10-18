/* SPDX-License-Identifier: GPL-3.0-only */
#include "app/panel_map.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    panel_t p = PANEL_DEFAULT;
    assert(panel_valid(&p));
    for (int i=0;i<NB;i++) for(int j=0;j<NB;j++) if(i!=j) {
        p=PANEL_DEFAULT;p.btn[i]=p.btn[j];assert(!panel_valid(&p));
        p=PANEL_DEFAULT;uint8_t v=p.btn[i];p.btn[i]=p.btn[j];p.btn[j]=v;
        assert(panel_valid(&p));
    }
    for (int i=0;i<NE;i++) for(int j=0;j<NE;j++) if(i!=j) {
        p=PANEL_DEFAULT;p.enc[i]=p.enc[j];assert(!panel_valid(&p));
        p=PANEL_DEFAULT;uint8_t v=p.enc[i];p.enc[i]=p.enc[j];p.enc[j]=v;
        p.dir[i]=-1;assert(panel_valid(&p));
    }
    for(int i=0;i<NB;i++){p=PANEL_DEFAULT;p.btn[i]=14;assert(!panel_valid(&p));}
    for(int i=0;i<NE;i++){
        p=PANEL_DEFAULT;p.enc[i]=7;assert(!panel_valid(&p));
        p=PANEL_DEFAULT;p.dir[i]=0;assert(!panel_valid(&p));
        p.dir[i]=2;assert(!panel_valid(&p));
    }
    p=PANEL_DEFAULT;p.magic=0;assert(!panel_valid(&p));
    puts("Panel calibration: every duplicate button/encoder rejected, valid permutations and directions retained; bad ranges and magic rejected");
}
