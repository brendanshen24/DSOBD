#include "elm_core.h"
#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
static int fd;
static ElmCore engine;
static uint32_t now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return (uint32_t)(t.tv_sec*1000u+t.tv_nsec/1000000); }
static bool send_cmd(const char *cmd,void *u) { (void)u; char wire[32]; int n=snprintf(wire,sizeof(wire),"%s\r",cmd); return write(fd,wire,n)==n; }
static void tick(void) {
    uint8_t buffer[1024]; ssize_t n=read(fd,buffer,sizeof(buffer));
    if(n>0) elm_feed(&engine,buffer,n,now());
    elm_tick(&engine,now()); struct timespec t={0,1000000}; nanosleep(&t,NULL);
}
static void idle(void) { uint32_t start=now(); do { tick(); assert((uint32_t)(now()-start)<60000); } while(engine.busy || engine.scanning || engine.q_count); }
int main(int argc,char **argv) {
    assert(argc==2); fd=socket(AF_INET,SOCK_STREAM,0); assert(fd>=0);
    struct sockaddr_in addr={.sin_family=AF_INET,.sin_port=htons((unsigned short)atoi(argv[1]))}; inet_pton(AF_INET,"127.0.0.1",&addr.sin_addr);
    assert(connect(fd,(void*)&addr,sizeof(addr))==0); fcntl(fd,F_SETFL,O_NONBLOCK);
    ElmCallbacks cb={.send=send_cmd}; elm_init(&engine,&cb,NULL,0); elm_on_connected(&engine,now()); idle();
    assert(elm_scan(&engine,false)); idle();
    ObdDiagnostics *d=&engine.diagnostics;
    assert(!strcmp(d->vin,"1HGCM82633A004352")); assert(!strcmp(d->calibration,"37805RBBA530"));
    assert(!strcmp(d->ecu,"ECM-EngineControl")); assert(d->code_count==4 && d->mil && d->freeze_code==0x0133);
    assert(obd_misfire(d,2,false)==14 && obd_misfire(d,2,true)==6);
    int count=0; for(int i=0;i<OBD_SENSOR_COUNT;i++) if(obd_supported(d,obd_sensors[i].pid)) {
        char cmd[8]; snprintf(cmd,sizeof(cmd),"01%02X",obd_sensors[i].pid); assert(elm_request(&engine,cmd)); idle(); assert(d->has[i]); count++;
    }
    assert(count==30);
    assert(elm_scan(&engine,true)); idle();
    assert(d->clear_ok && d->code_count==1 && d->codes[0].kind==2 && !d->mil && !d->freeze_code);
    assert(obd_misfire(d,2,false)==0 && engine.d.stopped==0 && engine.d.timeouts==0);
    close(fd); puts("Mock TCP: scan, 30 sensors, clear/rescan passed with zero STOPPED/timeouts");
}
