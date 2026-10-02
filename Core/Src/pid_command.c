#include "pid_command.h"
#include "nRF24L01P.h"
#include "debug.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
extern osMessageQueueId_t DebugQueueHandle;
static uint8_t ring[256];
static volatile uint16_t head,tail;
static volatile uint8_t overflow;
static char line[100];
static unsigned used;
static uint8_t discard,active,control_next,report_pending;
static uint32_t started,last_send;
static PidMessage command,report;
static const char *report_kind;
static int publish(const char *kind,const PidMessage *m) {
    DebugMessage_t msg={0};
    snprintf(msg.text,sizeof(msg.text),"%s_%s,%lu,%u,%u,%lu,%lu,%lu\r\n",m->operation==3?"AXIS":"PID",kind,
        (unsigned long)m->sequence,(unsigned)(m->operation==3 && m->id>=128?m->id-128:m->id),(unsigned)m->status,
        (unsigned long)m->gain[0],(unsigned long)m->gain[1],(unsigned long)m->gain[2]);
    return DebugQueueHandle && osMessageQueuePut(DebugQueueHandle,&msg,0,0)==osOK;
}
void PidCommand_USB(const uint8_t *p,uint32_t n) {
    for(uint32_t i=0;i<n;i++) {
        uint16_t next=(head+1U)&255U;
        if(next==tail) { overflow=1;return; }
        ring[head]=p[i];__DMB();head=next;
    }
}
static void parse(void) {
    PidMessage m={0};uint32_t v[5]={0};char *p;unsigned count;
    if(strncmp(line,"PID,",4)==0) { m.operation=1;p=line+4;count=5; }
    else if(strncmp(line,"AXIS,",5)==0) { m.operation=3;p=line+5;count=2; }
    else return;
    for(unsigned i=0;i<count;i++) {
        char *end;
        if(*p<'0'||*p>'9') return;
        errno=0;unsigned long value=strtoul(p,&end,10);
        if(errno || value>UINT32_MAX || (i+1<count?*end!=',':*end!='\0')) return;
        v[i]=(uint32_t)value;p=end+1;
    }
    m.sequence=v[0];m.id=(uint8_t)v[1];
    for(unsigned i=0;i<3;i++) m.gain[i]=v[i+2];
    if(m.operation==3 && v[1]<=2) m.id=(uint8_t)(128+v[1]);
    if(m.operation==3 ? v[1]>2 : (v[1]>255 || !PidValid(&m))) { m.status=1;(void)publish("REJECTED",&m);return; }
    if(active || report_pending) { (void)publish("BUSY",&m);return; }
    command=m;active=1;started=HAL_GetTick();last_send=started-100U;
}
void PidCommand_Poll(void) {
    /* Atomic overflow recovery: discard through next newline, never execute a truncated command. */
    uint32_t mask=__get_PRIMASK();__disable_irq();
    if(overflow) { tail=head;overflow=0;discard=1;used=0; }
    __set_PRIMASK(mask);
    unsigned budget=256;
    while(tail!=head && budget--) {
        uint8_t c=ring[tail];__DMB();tail=(tail+1U)&255U;
        if(c=='\n') {
            if(!discard) { line[used]=0;parse(); }
            used=0;discard=0;
        } else if(c!='\r' && !discard) {
            if(c<32 || c>126 || used>=sizeof(line)-1) { discard=1;used=0; }
            else line[used++]=(char)c;
        }
    }
    if(active && (uint32_t)(HAL_GetTick()-started)>=2000U) {
        report=command;report_kind="TIMEOUT";report_pending=1;active=0;
    }
    if(report_pending && publish(report_kind,&report)) report_pending=0;
}
uint8_t PidCommand_Send(uint8_t *tx_status) {
    if(control_next) { control_next=0;return 0; }
    if(!active || (uint32_t)(HAL_GetTick()-last_send)<100U) return 0;
    uint8_t packet[PID_WIRE_SIZE];PidEncode(packet,&command,command.operation);
    last_send=HAL_GetTick();control_next=1;
    *tx_status=L01_TransmitPacket(packet,sizeof(packet),L01_TX_TIMEOUT_MS);
    return 1;
}
void PidCommand_Ack(const uint8_t *p,uint8_t length) {
    PidMessage m;
    if(!active || !PidDecode(p,length,&m,(uint8_t)(command.operation+1))) return;
    if(m.sequence!=command.sequence || m.id!=command.id || memcmp(m.gain,command.gain,sizeof(m.gain))) return;
    m.operation=command.operation;
    report=m;report_kind=m.status==0?"APPLIED":"REJECTED";
    report_pending=1;active=0;
}
