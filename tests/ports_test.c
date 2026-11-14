#include "elm_core.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
static int sensor(int pid) { for(int i=0;i<OBD_SENSOR_COUNT;i++) if(obd_sensors[i].pid==pid) return i; abort(); }
static char sent[24];
static bool send_command(const char *cmd, void *user) { (void)user; snprintf(sent,sizeof(sent),"%s",cmd); return true; }
static void feed(ElmCore *e,const char *s,uint32_t now) { elm_feed(e,(const uint8_t *)s,strlen(s),now); }
int main(void) {
    ObdDiagnostics d={0};
    obd_response(&d,"0100","41 00 00 10 00 00\r>");
    assert(obd_supported(&d,0x0C) && !obd_supported(&d,0xA6));
    obd_response(&d,"0178","00B\r0: 41 78 01 17 0C 00\r1: 00 00 00 00 00\r>");
    assert(fabs(obd_value(&d,sensor(0x78),false)-550)<0.001);
    obd_response(&d,"017C","00B\r0: 41 7C 01 17 0C 00\r1: 00 00 00 00 00\r>");
    assert(fabs(obd_value(&d,sensor(0x7C),false)-550)<0.001);
    obd_response(&d,"0177","41 77 00 46 00 00 00\r>");
    assert(!obd_has(&d,sensor(0x77),false));
    obd_response(&d,"0177","41 77 01 46 00 00 00\r>");
    assert(obd_value(&d,sensor(0x77),false)==30);
    obd_response(&d,"01A6","41 A6 00 12 D6 87\r>");
    assert(fabs(obd_value(&d,sensor(0xA6),false)-123456.7)<0.05);
    obd_response(&d,"010C","41 0C GG FF\r>"); assert(!obd_has(&d,sensor(0x0C),false));
    obd_response(&d,"03","43 02 01 33 04 20\r>");
    obd_response(&d,"07","47 01 01 71\r>");
    obd_response(&d,"0A","4A 01 04 20\r>");
    assert(d.code_count==4 && d.codes[3].kind==2 && d.codes[3].value==0x0420);
    obd_scan_reset(&d); obd_response(&d,"03","43 01 33 02 44 00 00\r43 03 00 00 00 00 00\r>");
    assert(d.code_count==3 && d.codes[0].value==0x0133);
    obd_response(&d,"0902","014\r0: 49 02 01 31 48 47\r1: 43 4D 38 32 36 33 33\r2: 41 30 30 34 33 35 32\r>");
    assert(!strcmp(d.vin,"1HGCM82633A004352"));
    obd_response(&d,"0904","013\r0: 49 04 01 33 37 38\r1: 30 35 52 42 42 41 35\r2: 33 30 00 00 00 00\r>");
    assert(!strcmp(d.calibration,"37805RBBA530"));
    d.calibration[0]=0;
    obd_response(&d,"0904","023\r0: 49 04 02 41 42 43\r1: 00 00 00 00 00 00 00\r2: 00 00 00 00 00 00 44\r3: 45 46 00 00 00 00 00\r4: 00 00 00 00 00 00 00\r5: 00\r>");
    assert(!strcmp(d.calibration,"ABC"));
    obd_response(&d,"090A","017\r0: 49 0A 01 45 43 4D\r1: 00 2D 45 6E 67 69 6E\r2: 65 43 6F 6E 74 72 6F\r3: 6C 00 00\r>");
    assert(!strcmp(d.ecu,"ECM-EngineControl"));
    obd_response(&d,"0101","41 01 82 07 65 04\r>");
    assert(d.monitors && d.mil && __builtin_popcount(d.ready_supported & ~d.incomplete)==6);
    obd_response(&d,"020200","42 02 00 01 33\r>");
    obd_response(&d,"020C00","42 0C 00 1F 40\r>");
    assert(d.freeze_code==0x0133 && obd_value(&d,sensor(0x0C),true)==2000);
    obd_response(&d,"06A0","46 A0 F8 00 00 00\r>"); assert(d.mids==0xF8000000u);
    obd_response(&d,"06A4","013\r0: 46 A4 0B 24 00 06\r1: 00 00 FF FF A4 0C 24\r2: 00 0E 00 00 FF FF\r>");
    assert(obd_misfire(&d,2,false)==14 && obd_misfire(&d,2,true)==6 && obd_misfire(&d,1,false)==-1);
    obd_response(&d,"06A5","013\r0: 46 A5 0B 24 00 06\r2: 00 0E 00 00 FF FF\r>");
    assert(obd_misfire(&d,3,false)==-1);
    obd_response(&d,"04","NO DATA\r>"); assert(!d.clear_ok);
    obd_response(&d,"04","44\r>"); assert(d.clear_ok);
    char code[6]; obd_format_code(0xC123,code); assert(!strcmp(code,"U0123"));
    // CSV exclusivity and blanks: stale RPM must not be logged again.
    char directory[]="/tmp/ports-csv-XXXXXX"; assert(mkdtemp(directory));
    ObdCsv csv={0}; assert(obd_csv_start(&csv,directory,&d,0));
    obd_response(&d,"010C","41 0C 1F 40\r>"); assert(obd_csv_tick(&csv,&d,100));
    assert(obd_csv_tick(&csv,&d,200) && csv.rows==1);
    char path[512]; snprintf(path,sizeof(path),"%s",csv.path); assert(obd_csv_stop(&csv));
    FILE *f=fopen(path,"r"); assert(f); char line[2048]; assert(fgets(line,sizeof(line),f)); assert(strstr(line,"ODOMETER (km)"));
    assert(fgets(line,sizeof(line),f) && strstr(line,",2000,")); assert(!fgets(line,sizeof(line),f)); fclose(f);
    assert(obd_csv_start(&csv,directory,&d,0) && strcmp(path,csv.path)); assert(obd_csv_stop(&csv));
    unlink(path); unlink(csv.path); rmdir(directory);
    assert(!obd_csv_start(&csv,"/does-not-exist/ports-test",&d,0));
    // Core sends one request per prompt; unsupported range queries are skipped.
    ElmCore e; ElmCallbacks cb={.send=send_command}; elm_init(&e,&cb,NULL,0); elm_on_connected(&e,0); feed(&e,">",0);
    for(int i=0;i<7;i++) { elm_tick(&e,i*10); assert(e.busy); feed(&e,i==6?"41 00 00 10 00 00\r>":"OK\r>",i*10); }
    elm_tick(&e,100); assert(!e.busy);
    assert(elm_scan(&e,true)); elm_tick(&e,110); assert(!strcmp(sent,"04"));
    feed(&e,"NO DATA\r>",120); assert(!e.scanning && e.diagnostics.scan_error);
    assert(elm_scan(&e,false)); elm_tick(&e,130); assert(!strcmp(sent,"ATDP"));
    elm_tick(&e,140); assert(!strcmp(sent,"ATDP")); feed(&e,"ISO 15765-4 (CAN)\r>",150);
    elm_on_disconnected(&e); assert(!e.scanning);
    puts("Shared diagnostics, frame validation, scan gating and CSV tests passed");
}
