#include "obd_diag.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
const ObdSensor obd_sensors[OBD_SENSOR_COUNT] = {
    { 0x04, 1, 0, "ENGINE LOAD",   "%",      100.0f / 255, 0, 0 },
    { 0x05, 1, 0, "COOLANT",       "C",      1, -40, 0 },
    { 0x06, 1, 1, "SHORT TRIM B1", "%",      100.0f / 128, -100, 0 },
    { 0x07, 1, 1, "LONG TRIM B1",  "%",      100.0f / 128, -100, 0 },
    { 0x08, 1, 1, "SHORT TRIM B2", "%",      100.0f / 128, -100, 0 },
    { 0x09, 1, 1, "LONG TRIM B2",  "%",      100.0f / 128, -100, 0 },
    { 0x0A, 1, 0, "FUEL PRESSURE", "kPa",    3, 0, 0 },
    { 0x0B, 1, 0, "INTAKE MAP",    "kPa",    1, 0, 0 },
    { 0x0C, 2, 0, "ENGINE RPM",    "rpm",    0.25f, 0, 0 },
    { 0x0D, 1, 0, "SPEED",         "km/h",   1, 0, 0 },
    { 0x0E, 1, 1, "TIMING ADV",    "deg",    0.5f, -64, 0 },
    { 0x0F, 1, 0, "INTAKE AIR",    "C",      1, -40, 0 },
    { 0x10, 2, 2, "MAF AIRFLOW",   "g/s",    0.01f, 0, 0 },
    { 0x11, 1, 0, "THROTTLE",      "%",      100.0f / 255, 0, 0 },
    { 0x14, 1, 3, "O2 B1 S1",      "V",      0.005f, 0, 0 },
    { 0x15, 1, 3, "O2 B1 S2",      "V",      0.005f, 0, 0 },
    { 0x1C, 1, 0, "OBD STANDARD",  "",       1, 0, 0 },
    { 0x1F, 2, 0, "RUN TIME",      "s",      1, 0, 0 },
    { 0x21, 2, 0, "DIST MIL ON",   "km",     1, 0, 0 },
    { 0x23, 2, 0, "RAIL PRESSURE", "kPa",    10, 0, 0 },
    { 0x2C, 1, 0, "COMMANDED EGR", "%",      100.0f / 255, 0, 0 },
    { 0x2F, 1, 0, "FUEL LEVEL",    "%",      100.0f / 255, 0, 0 },
    { 0x30, 1, 0, "WARM-UPS CLR",  "",       1, 0, 0 },
    { 0x31, 2, 0, "DIST CLEARED",  "km",     1, 0, 0 },
    { 0x33, 1, 0, "BAROMETRIC",    "kPa",    1, 0, 0 },
    { 0x3C, 2, 0, "CAT TEMP B1",   "C",      0.1f, -40, 0 },
    { 0x42, 2, 2, "ECU VOLTAGE",   "V",      0.001f, 0, 0 },
    { 0x43, 2, 0, "ABSOLUTE LOAD", "%",      100.0f / 255, 0, 0 },
    { 0x44, 2, 2, "COMMANDED AFR", "lambda", 2.0f / 65536, 0, 0 },
    { 0x45, 1, 0, "REL THROTTLE",  "%",      100.0f / 255, 0, 0 },
    { 0x46, 1, 0, "AMBIENT AIR",   "C",      1, -40, 0 },
    { 0x49, 1, 0, "ACCEL PEDAL",   "%",      100.0f / 255, 0, 0 },
    { 0x4E, 2, 0, "TIME CLEARED",  "min",    1, 0, 0 },
    { 0x51, 1, 0, "FUEL TYPE",     "",       1, 0, 0 },
    { 0x5C, 1, 0, "OIL TEMP",      "C",      1, -40, 0 },
    { 0x5E, 2, 1, "FUEL RATE",     "L/h",    0.05f, 0, 0 },
    { 0x61, 1, 0, "DEMAND TORQUE", "%",      1, -125, 0 },
    { 0x62, 1, 0, "ACTUAL TORQUE", "%",      1, -125, 0 },
    { 0x63, 2, 0, "REF TORQUE",    "Nm",     1, 0, 0 },
    { 0x77, 1, 0, "INTERCOOLER",   "C",      1, -40, 1 },
    { 0x78, 2, 0, "EGT B1 S1",     "C",      0.1f, -40, 1 },
    { 0x7C, 2, 0, "DPF INLET B1",  "C",      0.1f, -40, 1 },
    { 0xA6, 4, 0, "ODOMETER",      "km",     0.1f, 0, 0 },
};
const ObdSensor *obd_sensor(int i) { return i >= 0 && i < OBD_SENSOR_COUNT ? &obd_sensors[i] : NULL; }
double obd_value(const ObdDiagnostics *d, int i, bool f) { return obd_sensor(i) ? (f ? d->freeze[i] : d->values[i]) : 0; }
bool obd_has(const ObdDiagnostics *d, int i, bool f) { return obd_sensor(i) && (f ? d->freeze_has[i] : d->has[i]); }
const ObdCode *obd_code(const ObdDiagnostics *d, int i) { return i >= 0 && i < d->code_count ? &d->codes[i] : NULL; }
int obd_misfire(const ObdDiagnostics *d, int c, bool avg) {
    if (c < 0 || c >= 12 || !((avg ? d->average_valid : d->drive_valid) & (1u << c))) return -1;
    return avg ? d->average[c] : d->drive[c];
}
const char *obd_text(const ObdDiagnostics *d, int f) { return f == 0 ? d->vin : f == 1 ? d->calibration : f == 2 ? d->ecu : d->protocol; }
void obd_format_code(uint16_t v, char out[6]) { snprintf(out, 6, "%c%04X", "PCBU"[v >> 14], v & 0x3FFF); }
const char *obd_enum(uint8_t pid, int v) {
    static const char *const fuel[] = {"Unknown", "Gasoline", "Methanol", "Ethanol", "Diesel", "LPG", "CNG", "Propane", "Electric"};
    static const char *const standard[] = {"Unknown", "OBD-II CARB", "OBD EPA", "OBD + OBD-II", "OBD-I", "Non-OBD", "EOBD", "EOBD + OBD-II", "EOBD + OBD", "EOBD + OBD + II", "JOBD", "JOBD + OBD-II", "JOBD + EOBD", "JOBD + EOBD + II"};
    if (pid == 0x1C && v >= 0 && v < 14) return standard[v];
    if (pid == 0x51 && v >= 0 && v < 9) return fuel[v];
    if (pid == 0x51 && v >= 9 && v <= 23) return v >= 17 && v <= 22 ? "Hybrid" : "Bi-fuel";
    return NULL;
}
bool obd_supported(const ObdDiagnostics *d, uint8_t pid) {
    if (!pid || pid > 0xC0) return true;
    int r = (pid - 1) / 32;
    for (int i = 0; i < r; i++) if (d->known[i] && !(d->supported[i] & 1)) return false;
    return !d->known[r] || (d->supported[r] & (1u << (31 - (pid - 1) % 32)));
}
void obd_scan_reset(ObdDiagnostics *d) {
    d->code_count = 0; d->monitors = d->mil = d->clear_ok = d->scan_error = false;
    memset(d->codes_valid, 0, sizeof(d->codes_valid));
    d->ready_supported = d->incomplete = d->freeze_code = 0;
    memset(d->freeze_has, 0, sizeof(d->freeze_has));
    d->drive_valid = d->average_valid = 0; d->mids = 0;
    d->vin[0] = d->calibration[0] = d->ecu[0] = d->protocol[0] = 0;
}
const char *obd_scan_job(const ObdDiagnostics *d, int i) {
    static const char *const jobs[] = {"ATDP", "0101", "03", "07", "0A", "0902", "0904", "090A", "011C", "0151", "01A6", "0121", "0131", "0130", "020200", "020400", "020500", "020C00", "020D00", "06A0", "06A2", "06A3", "06A4", "06A5", "06A6", "06A7", "06A8", "06A9", "06AA", "06AB", "06AC", "06AD"};
    if (i < 0 || i >= (int)(sizeof(jobs)/sizeof(jobs[0]))) return NULL;
    const char *cmd = jobs[i]; unsigned pid = (unsigned)strtoul(cmd + 2, NULL, 16);
    if (!strncmp(cmd, "01", 2) && !obd_supported(d, pid)) return "";
    if (!strncmp(cmd, "02", 2) && strcmp(cmd, "020200") && !d->freeze_code) return "";
    if (!strncmp(cmd, "06", 2) && pid > 0xA0 && !(d->mids & (1u << (0xC0-pid)))) return "";
    return cmd;
}
static uint32_t number(const uint8_t *p, int n) { uint32_t v = 0; while (n--) v = v * 256 + *p++; return v; }
static void message(ObdDiagnostics *d, const char *cmd, const uint8_t *b, int n, bool can) {
    if (!n) return;
    unsigned mode = (unsigned)strtoul(cmd, NULL, 16);
    if (!strcmp(cmd, "04")) { if (b[0] == 0x44) d->clear_ok = true; return; }
    if (!strncmp(cmd, "01", 2) || !strncmp(cmd, "02", 2)) {
        bool ff = cmd[1] == '2'; int start = ff ? 3 : 2;
        if (n < start || b[0] != (ff ? 0x42 : 0x41) || b[1] != strtoul(cmd+2, NULL, 16) / (ff ? 256 : 1)) return;
        int pid = b[1];
        if (!ff && pid <= 0xA0 && pid % 32 == 0 && n >= 6) { d->known[pid/32] = true; d->supported[pid/32] |= number(b+2,4); }
        if (!ff && pid == 1 && n >= 6) { d->monitors = true; d->mil |= (b[2] & 128) != 0; d->ready_supported |= (b[3]&7) | (b[4]<<3); d->incomplete |= ((b[3]>>4)&7) | (b[5]<<3); }
        if (ff && pid == 2 && n >= 5) d->freeze_code = number(b+3,2);
        for (int i = 0; i < OBD_SENSOR_COUNT; i++) {
            const ObdSensor *p = &obd_sensors[i];
            if (p->pid != pid || n < start+p->skip+p->bytes) continue;
            if (p->skip && !(b[start] & 1)) continue;
            double v = number(b+start+p->skip,p->bytes) * (double)p->scale + p->offset;
            if (ff) { d->freeze[i] = v; d->freeze_has[i] = true; }
            else { d->values[i] = v; d->has[i] = d->fresh[i] = true; }
        }
    } else if (!strncmp(cmd, "09", 2)) {
        int pid = (int)strtoul(cmd+2,NULL,16);
        if (n < 3 || b[0] != 0x49 || b[1] != pid) return;
        char *out = pid == 2 ? d->vin : pid == 4 ? d->calibration : pid == 10 ? d->ecu : NULL;
        int cap = pid == 2 ? 17 : pid == 4 ? 16 : 20;
        if (!out || (can && out[0])) return;
        if (pid == 4 && can && n > 19) n = 19; // Keep the first 16-byte calibration ID.
        if (pid == 4 && !can && b[2] > 4) return;
        int len = (int)strlen(out);
        for (int i = 3; i < n && len < cap; i++) if (b[i] >= 32 && b[i] < 127) out[len++] = b[i];
        out[len] = 0;
    } else if (!strncmp(cmd, "06", 2) && b[0] == 0x46) {
        unsigned mid = (unsigned)strtoul(cmd+2,NULL,16);
        if (mid == 0xA0) { if (n == 6 && b[1] == mid) d->mids |= number(b+2,4); return; }
        if (mid < 0xA2 || mid > 0xAD) return;
        for (int i = 1; i+9 <= n; i+=9) {
            if (b[i] != mid || b[i+2] != 0x24) continue;
            int c = mid-0xA2;
            if (b[i+1] == 0x0B) { d->average[c] = number(b+i+3,2); d->average_valid |= 1u<<c; }
            if (b[i+1] == 0x0C) { d->drive[c] = number(b+i+3,2); d->drive_valid |= 1u<<c; }
        }
    } else if ((mode == 3 || mode == 7 || mode == 10) && b[0] == 0x40+mode) {
        int kind = mode == 3 ? 0 : mode == 7 ? 1 : 2;
        d->codes_valid[kind] = true;
        int start = can || n%2 == 0 ? 2 : 1;
        int count = start == 2 && n > 1 ? b[1] : (n-1)/2;
        for (int i=start; i+1<n && count--; i+=2) {
            uint16_t v=number(b+i,2); if (!v) continue;
            bool found=false;
            for(int j=0;j<d->code_count;j++) if(d->codes[j].value==v && d->codes[j].kind==kind) found=true;
            if(!found && d->code_count<OBD_CODE_MAX) d->codes[d->code_count++]=(ObdCode){v,kind};
        }
    }
}
void obd_response(ObdDiagnostics *d, const char *cmd, const char *raw) {
    if (!strcmp(cmd,"ATDP")) {
        char copy[2048]; snprintf(copy,sizeof(copy),"%s",raw);
        for(char *l=strtok(copy,"\r\n>");l;l=strtok(NULL,"\r\n>"))
            if(strcmp(l,"ATDP") && strcmp(l,"?") && strcmp(l,"NO DATA")) snprintf(d->protocol,sizeof(d->protocol),"%.63s",l);
        return;
    }
    uint8_t bytes[512]; int n=0, expected=0, seq=-1; bool framed=false, bad=false;
    char line[1024];
    while(*raw) {
        int k=0; bool overflow=false;
        while(*raw && *raw!='\r' && *raw!='\n' && *raw!='>') {
            unsigned char c=*raw++;
            if(!isspace(c)) { if(k+1<(int)sizeof(line)) line[k++]=toupper(c); else overflow=true; }
        }
        if(*raw) raw++;
        line[k]=0;
        if(!k) continue;
        if(overflow) { bad=true; continue; }
        if(k==3 && strspn(line,"0123456789ABCDEF")==3) {
            if(n && !bad && framed && expected>0 && n>=expected) message(d,cmd,bytes,expected,true);
            n=0; seq=-1; bad=false; framed=false;
            expected=(int)strtol(line,NULL,16); continue;
        }
        char *hex=line; bool numbered=k>2 && line[1]==':';
        if(numbered) {
            int index=(int)strtol(line,NULL,16); hex+=2;
            if(index==0 && seq<0) { framed=true; seq=0; }
            else if(index==0 && n>=expected) {
                if(!bad) message(d,cmd,bytes,expected?expected:n,framed);
                n=0; bad=false; framed=true; seq=0;
            } else if(seq<0 || index!=((seq+1)&15)) { bad=true; continue; }
            else seq=index;
        } else {
            if(n && !bad && (!framed || n>=expected)) message(d,cmd,bytes,expected?expected:n,framed);
            n=0; seq=-1; framed=false; bad=false; expected=0;
        }
        size_t len=strlen(hex);
        if(len%2 || strspn(hex,"0123456789ABCDEF")!=len) continue;
        for(size_t i=0;i<len;i+=2) {
            char pair[3]={hex[i],hex[i+1],0};
            if(n<(int)sizeof(bytes)) bytes[n++]=(uint8_t)strtol(pair,NULL,16); else bad=true;
        }
        if(!numbered && n) { message(d,cmd,bytes,n,false); n=0; }
    }
    if(n && !bad && (!framed || (expected>0 && n>=expected))) message(d,cmd,bytes,expected?expected:n,framed);
    if(bad || (framed && (expected<=0 || n<expected))) d->scan_error=true;
}
static bool csv_error(ObdCsv *l) { snprintf(l->error,sizeof(l->error),"CSV write failed: %s",strerror(errno)); if(l->file) fclose(l->file); l->file=NULL; return false; }
bool obd_csv_stop(ObdCsv *l) { if(!l->file) return true; int rc=fclose(l->file); l->file=NULL; if(rc) return csv_error(l); return true; }
bool obd_csv_start(ObdCsv *l, const char *dir, ObdDiagnostics *d, uint32_t now) {
    if(l->file) return false;
    memset(l,0,sizeof(*l));
    for(int i=1;i<=9999;i++) {
        if(snprintf(l->path,sizeof(l->path),"%s/dsobd-log-%04d.csv",dir,i)>=(int)sizeof(l->path)) { errno=ENAMETOOLONG; return csv_error(l); }
        int fd=open(l->path,O_WRONLY|O_CREAT|O_EXCL,0666);
        if(fd<0) { if(errno==EEXIST) continue; return csv_error(l); }
        l->file=fdopen(fd,"w"); if(!l->file) { close(fd); return csv_error(l); }
        break;
    }
    if(!l->file) { errno=EEXIST; return csv_error(l); }
    fprintf(l->file,"time_s");
    for(int i=0;i<OBD_SENSOR_COUNT;i++) fprintf(l->file,",%s (%s)",obd_sensors[i].name,obd_sensors[i].unit);
    fprintf(l->file,"\n"); if(fflush(l->file)) return csv_error(l);
    memset(d->fresh,0,sizeof(d->fresh)); l->start=l->last=l->flushed=now; return true;
}
bool obd_csv_tick(ObdCsv *l, ObdDiagnostics *d, uint32_t now) {
    if(!l->file || (uint32_t)(now-l->last)<100) return true;
    l->last=now; bool fresh=false;
    for(int i=0;i<OBD_SENSOR_COUNT;i++) fresh|=d->fresh[i];
    if(fresh) {
        fprintf(l->file,"%.3f",(uint32_t)(now-l->start)/1000.0);
        for(int i=0;i<OBD_SENSOR_COUNT;i++) {
            fputc(',',l->file);
            if(d->fresh[i]) fprintf(l->file,"%.*f",obd_sensors[i].decimals,d->values[i]);
        }
        fputc('\n',l->file); if(ferror(l->file)) return csv_error(l);
        memset(d->fresh,0,sizeof(d->fresh)); l->rows++;
    }
    if((uint32_t)(now-l->flushed)>=5000) { if(fflush(l->file)) return csv_error(l); l->flushed=now; }
    return true;
}
