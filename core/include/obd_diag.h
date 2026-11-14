#ifndef OBD_DIAG_H
#define OBD_DIAG_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define OBD_SENSOR_COUNT 43
#define OBD_CODE_MAX 32
typedef struct { uint8_t pid, bytes, decimals; const char *name, *unit; float scale, offset; uint8_t skip; } ObdSensor;
typedef struct { uint16_t value; uint8_t kind; } ObdCode;
typedef struct {
    uint32_t supported[6]; bool known[6];
    double values[OBD_SENSOR_COUNT], freeze[OBD_SENSOR_COUNT];
    bool has[OBD_SENSOR_COUNT], fresh[OBD_SENSOR_COUNT], freeze_has[OBD_SENSOR_COUNT];
    ObdCode codes[OBD_CODE_MAX]; int code_count;
    bool monitors, mil, clear_ok, scan_error, codes_valid[3];
    uint16_t ready_supported, incomplete, freeze_code;
    char vin[18], calibration[17], ecu[21], protocol[64];
    uint32_t mids; uint16_t drive[12], average[12], drive_valid, average_valid;
} ObdDiagnostics;
extern const ObdSensor obd_sensors[OBD_SENSOR_COUNT];
const ObdSensor *obd_sensor(int index);
double obd_value(const ObdDiagnostics *d, int index, bool freeze);
bool obd_has(const ObdDiagnostics *d, int index, bool freeze);
const ObdCode *obd_code(const ObdDiagnostics *d, int index);
int obd_misfire(const ObdDiagnostics *d, int cylinder, bool average);
const char *obd_text(const ObdDiagnostics *d, int field);
bool obd_supported(const ObdDiagnostics *d, uint8_t pid);
void obd_scan_reset(ObdDiagnostics *d);
const char *obd_scan_job(const ObdDiagnostics *d, int index);
void obd_response(ObdDiagnostics *d, const char *cmd, const char *response);
void obd_format_code(uint16_t value, char out[6]);
const char *obd_enum(uint8_t pid, int value);
typedef struct { FILE *file; uint32_t start, last, flushed, rows; char path[512], error[80]; } ObdCsv;
bool obd_csv_start(ObdCsv *log, const char *directory, ObdDiagnostics *d, uint32_t now);
bool obd_csv_tick(ObdCsv *log, ObdDiagnostics *d, uint32_t now);
bool obd_csv_stop(ObdCsv *log);
#endif
