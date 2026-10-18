/* SPDX-License-Identifier: GPL-3.0-only */
/* Bounded SPSC queues. USB/TRS producer and USB consumer run only outside audio;
 * main-loop RX consumer and audio TX producer publish indexes after slot writes. */
#pragma once
#define MQ 64u
static uint32_t midi_in_q[MQ],midi_in_stamp[MQ],midi_out_q[MQ],midi_out_due[MQ];
static volatile uint32_t mi_w,mi_r,mo_w,mo_r;
static volatile uint32_t midi_rx_drops,midi_tx_drops;
static int midi_rx_put(uint32_t packet,uint32_t stamp){
 if(mi_w-mi_r>=MQ){midi_rx_drops++;return 0;}
 unsigned slot=mi_w%MQ;midi_in_q[slot]=packet;midi_in_stamp[slot]=stamp;
 RING_PUBLISH();mi_w++;return 1;
}
static int midi_tx_put(uint32_t packet,uint32_t due){
 if(mo_w-mo_r>=MQ){midi_tx_drops++;return 0;}
 unsigned slot=mo_w%MQ;midi_out_q[slot]=packet;midi_out_due[slot]=due;
 RING_PUBLISH();mo_w++;return 1;
}
static int midi_tx_ready(uint32_t now){
 return mo_r!=mo_w&&(int32_t)(now-midi_out_due[mo_r%MQ])>=0;
}
