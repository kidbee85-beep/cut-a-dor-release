/* SPDX-License-Identifier: GPL-3.0-only
 * Cut-A-Dor editor protocol v1. F0 7D 43 41 44 command sequence payload F7.
 * Every reply echoes command/sequence and starts with status (0 success).
 * Bank writes are sequential, stopped-only, bounded to three 80 KiB slots;
 * the 512-byte header is committed last. A cancelled upload cannot appear valid.
 */
#define CAD_BANK_BASE 0xA0000u
static uint8_t ced_tx[300],ced_buf[512] __attribute__((aligned(4)));
static uint32_t ced_n,ced_slot,ced_next,ced_total,ced_active,ced_time;
static uint32_t ced_u21(const uint8_t *p){return p[0]|(uint32_t)p[1]<<7|(uint32_t)p[2]<<14;}
static uint32_t ced_unpack(const uint8_t *p,uint32_t n,uint8_t *o,uint32_t max){uint32_t j=0;while(n){uint32_t mask=*p++;n--;if(!n)return 0;for(unsigned b=0;b<7&&n;b++,n--){if(j>=max)return 0;o[j++]=(uint8_t)(*p++|((mask>>b)&1)<<7);}}return j;}
static void ced_pack(const uint8_t *p,uint32_t n){while(n){uint32_t k=n>7?7:n;uint8_t m=0;for(unsigned i=0;i<k;i++)m|=(p[i]>>7)<<i;ced_tx[ced_n++]=m;for(unsigned i=0;i<k;i++)ced_tx[ced_n++]=p[i]&127;p+=k;n-=k;}}
static uint32_t ced_off(unsigned slot){return CAD_BANK_BASE+slot*CAD_BANK_SIZE;}
static void ced_inval(unsigned slot){uint32_t f=irq_save();fl_inval(ced_off(slot),CAD_BANK_SIZE);irq_restore(f);}
static void ced_scan(void){for(int k=0;k<3;k++)cad_set_bank(k,fm1_xip_ptr(ced_off(k)),CAD_BANK_SIZE);}
static int ced_stop(void){if(cad_view.playing)return 0;cad_command(A_QUIET,0,0);return !project_snapshot();}
static void ced_poll(void){if(ced_active&&fm1_ms-ced_time>15000u)ced_active=cad_edit_busy=0;}
static int ced_frame(const uint8_t *p,uint32_t n){
 if(n<4||p[0]!=0x7d||p[1]!=0x43||p[2]!=0x41||p[3]!=0x44)return 0;
 if(n<6)return 1;
 unsigned cmd=p[4],seq=p[5];const uint8_t *a=p+6;uint32_t na=n-6;int err=1;
 ced_tx[0]=0xf0;ced_tx[1]=0x7d;ced_tx[2]=0x43;ced_tx[3]=0x41;ced_tx[4]=0x44;ced_tx[5]=(uint8_t)cmd;ced_tx[6]=(uint8_t)seq;ced_n=8;
 if(ced_active&&fm1_ms-ced_time>15000u)ced_active=cad_edit_busy=0;
 if(cmd==1&&na==0){const char *s="CUT-A-DOR " EN_VERSION;while(*s&&ced_n<70)ced_tx[ced_n++]=(uint8_t)*s++;ced_tx[ced_n++]=0;ced_tx[ced_n++]=1;ced_tx[ced_n++]=3;ced_tx[ced_n++]=cad_view.playing;err=0;}
 else if(cmd==2&&na==4&&!ced_active){int type=a[0],id=a[1],value=a[2]|a[3]<<7;
  if(type>=A_PLAY&&type<=A_MAP&&type!=A_EXPORT&&type!=A_IMPORT){err=cad_command(type,id==127?-1:id,value)?2:0;}}
 else if(cmd==10&&na==4&&a[0]<3&&flash_ok&&!ced_active&&ced_stop()){
  ced_slot=a[0];ced_total=ced_u21(a+1);if(ced_total>=CAD_BANK_HEADER&&ced_total<=CAD_BANK_SIZE){
   cad_set_bank((int)ced_slot,0,0);err=0;ced_active=cad_edit_busy=0;
   for(uint32_t o=0;o<CAD_BANK_SIZE&&!err;o+=4096){audio_silence();uint32_t took;err=fl_erase4k(ced_off(ced_slot)+o,&took)?3:0;fm1_wdt_feed();}
   ced_inval(ced_slot);if(!err){ced_active=cad_edit_busy=1;ced_next=0;ced_time=fm1_ms;memset(ced_buf,0,sizeof ced_buf);}
  }
 }
 else if(cmd==11&&na>=5&&ced_active&&!cad_view.playing&&a[0]==ced_slot){
  uint32_t off=ced_u21(a+1);uint8_t chunk[224] __attribute__((aligned(4)));uint32_t len=ced_unpack(a+4,na-4,chunk,sizeof chunk);
  if(len&&off==ced_next&&off<=ced_total&&len<=ced_total-off){err=0;
   uint32_t head=off<CAD_BANK_HEADER?CAD_BANK_HEADER-off:0;if(head>len)head=len;
   if(head)memcpy(ced_buf+off,chunk,head);
   if(len>head){audio_silence();if(fl_write(ced_off(ced_slot)+off+head,chunk+head,len-head))err=3;}
   if(!err){ced_next+=len;ced_time=fm1_ms;}
  }
 }
 else if(cmd==12&&na==1&&ced_active&&a[0]==ced_slot&&!cad_view.playing&&ced_next==ced_total){
  cad_bank h;memcpy(&h,ced_buf,sizeof h);uint32_t crc=h.header_crc;h.header_crc=0;ced_inval(ced_slot);
  if(h.magic==CAD_BANK_MAGIC&&h.version==1&&h.count>0&&h.count<=16&&h.data_len==ced_total-512&&cad_crc32(&h,sizeof h)==crc&&cad_crc32(fm1_xip_ptr(ced_off(ced_slot)+512),h.data_len)==h.data_crc){
   /* Full zone validation before committing: validate a header with the same parser's bounds. */
   err=0;for(unsigned i=0;i<h.count;i++){cad_zone *z=&h.zone[i];uint32_t blocks=(z->frames+255)/256;
    if(!z->frames||z->frames>24u*24000u||z->rate<8000||z->rate>24000||z->category>8||z->off>h.data_len||blocks>(h.data_len-z->off)/132||z->name[15]){err=4;break;}
    const uint8_t *d=fm1_xip_ptr(ced_off(ced_slot)+512+z->off);for(uint32_t j=0;j<blocks;j++)if(d[j*132+2]>88){err=4;break;}
   }
   if(!err){audio_silence();err=fl_write(ced_off(ced_slot),ced_buf,512)?3:0;ced_inval(ced_slot);if(!err)err=cad_set_bank((int)ced_slot,fm1_xip_ptr(ced_off(ced_slot)),ced_total)?4:0;}
  }else err=4;ced_active=cad_edit_busy=0;
 }
 else if(cmd==13&&na==0){ced_active=cad_edit_busy=0;err=0;}
 else if(cmd==20&&na==0&&!ced_active){err=project_snapshot()?2:0;if(!err){unsigned size=sizeof cad_transfer;ced_tx[ced_n++]=size&127;ced_tx[ced_n++]=(size>>7)&127;}}
 else if(cmd==21&&na==3&&!ced_active){unsigned off=ced_u21(a),size=sizeof cad_transfer;if(off<size){unsigned len=size-off;if(len>224)len=224;ced_pack((const uint8_t*)&cad_transfer+off,len);err=0;}}
 ced_tx[7]=(uint8_t)err;ced_tx[ced_n++]=0xf7;ota_wire_send(ced_tx,ced_n);return 1;
}
