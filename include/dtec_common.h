#ifndef DTEC_COMMON_H
#define DTEC_COMMON_H

#include <nds.h>
#include <stdint.h>
#include <stdbool.h>

#define SCREEN_W 256
#define SCREEN_H 192

// Default Veepeak VP01 OBD-II WiFi connection parameters
#define DEFAULT_OBD_SSID "WiFi_OBDII"
#define DEFAULT_OBD_KEY  ""
#define DEFAULT_OBD_IP   "192.168.0.10"
#define DEFAULT_OBD_PORT 35000

// 16-bit Opaque Color Helper (aBBBBBGGGGGRRRRR with bit 15 set)
#define DTEC_RGB(r, g, b) ((uint16_t)(BIT(15) | ((r) & 31) | (((g) & 31) << 5) | (((b) & 31) << 10)))

// High-visibility Palette
#define DTEC_COLOR_BG          DTEC_RGB(1, 2, 4)       // #070f1d
#define DTEC_COLOR_PANEL       DTEC_RGB(2, 4, 7)       // #0e1a2f
#define DTEC_COLOR_BORDER      DTEC_RGB(4, 8, 14)      // #1e385c
#define DTEC_COLOR_BORDER_HL   DTEC_RGB(7, 14, 22)     // #366094
#define DTEC_COLOR_CYAN        DTEC_RGB(7, 24, 31)     // #38bdf8
#define DTEC_COLOR_WHITE       DTEC_RGB(31, 31, 31)    // #ffffff
#define DTEC_COLOR_GREEN       DTEC_RGB(2, 25, 12)     // #10b981
#define DTEC_COLOR_YELLOW      DTEC_RGB(30, 24, 3)     // #eab308
#define DTEC_COLOR_RED         DTEC_RGB(30, 4, 4)      // #ef4444
#define DTEC_COLOR_PURPLE      DTEC_RGB(24, 15, 31)    // #c084fc
#define DTEC_COLOR_ORANGE      DTEC_RGB(31, 15, 2)     // #f97316
#define DTEC_COLOR_GRAY        DTEC_RGB(12, 14, 17)    // #64748b
#define DTEC_COLOR_DARK_GRAY   DTEC_RGB(5, 6, 9)       // #172540
#define DTEC_COLOR_BLACK       DTEC_RGB(0, 0, 0)

typedef enum {
    NET_DISCONNECTED = 0,
    NET_WIFI_CONNECTING,
    NET_WIFI_CONNECTED,
    NET_TCP_CONNECTING,
    NET_OBD_CONNECTED,
    NET_ERROR
} DTEC_NetStatus;

typedef enum {
    DTEC_MODE_CAR_SELECT = 0,
    DTEC_MODE_MULTI_GAUGE,
    DTEC_MODE_DIGITAL_SPEED,
    DTEC_MODE_BIG_SPEED,
    DTEC_MODE_ARC_TACH,
    DTEC_MODE_SIM_DASH,
    DTEC_MODE_GT_STYLE,
    DTEC_MODE_FORZA_STYLE,
    DTEC_MODE_BEAMNG_STYLE,
    DTEC_MODE_ANALOG_DIALS,
    DTEC_MODE_REALTIME_GRAPH,
    DTEC_MODE_BOOST_SETUP,
    DTEC_MODE_CODES,
    DTEC_MODE_SENSORS,
    DTEC_MODE_ECU_INFO,
    DTEC_MODE_OBD_TERMINAL,
    DTEC_MODE_SETTINGS,
    DTEC_MODE_COUNT
} DTEC_ScreenMode;

typedef struct {
    const char *brand;
    const char *name;
    const char *desc;
    const char *protocol_cmd;
    const char *header_cmd;
} DTEC_CarProfile;

#define DTEC_PROFILE_COUNT 14
extern const DTEC_CarProfile g_dtec_profiles[DTEC_PROFILE_COUNT];

// Mode 01 sensor table: value = raw * scale + offset, where raw is the
// big-endian `bytes` after `skip` leading bytes (a sensor-present mask).
typedef struct {
    uint8_t pid;
    uint8_t bytes;
    uint8_t decimals;
    const char *name;
    const char *unit;
    float scale;
    float offset;
    uint8_t skip;
} DTEC_Pid;

#define DTEC_PID_COUNT 43
extern const DTEC_Pid g_dtec_pids[DTEC_PID_COUNT];
#define DTEC_SENSOR_ROWS 8

#define DTEC_MAX_DTCS 16
#define DTEC_MAX_CYLINDERS 12
typedef enum { DTEC_DTC_STORED = 0, DTEC_DTC_PENDING, DTEC_DTC_PERMANENT } DTEC_DtcKind;

#define GRAPH_HISTORY_LEN 64
#define OBD_LOG_LINES 10

typedef struct {
    // Wi-Fi & OBD Adapter State
    DTEC_NetStatus net_status;
    char status_msg[48];
    char wifi_ssid[33];
    char local_ip[20];
    char target_ip[20];
    uint16_t target_port;

    bool auto_poll;
    uint32_t poll_tick;
    uint8_t poll_phase;
    uint32_t bytes_rx;
    uint32_t bytes_tx;
    uint32_t lines_rx;
    uint32_t cmds_sent;

    char elm_version[32];
    bool elm_inited;
    char last_cmd[32];
    char last_raw_line[64];

    // Selected Vehicle Profile (Car Scanner style)
    int selected_profile;
    bool profile_confirmed;
    char car_brand[32];
    char car_model[32];

    // Scrolling OBD Terminal Log
    char log_lines[OBD_LOG_LINES][40];
    int log_count;

    // Real Vehicle Telemetry (Parsed strictly from live OBD PIDs)
    float rpm;
    bool has_rpm;
    float peak_rpm;

    float boost_kpa;
    int map_kpa;
    bool has_map;
    bool has_boost; // Positive boost observed this connection; latched through idle.
    float peak_boost;

    float tps_pct;
    bool has_tps;

    uint32_t speed_kmh;
    bool has_speed;

    int coolant_c;
    bool has_coolant;

    float engine_load_pct;
    bool has_load;

    char battery_voltage[16];
    bool has_voltage;

    uint32_t knock_count;
    int knock_active_ticks;

    // Sensor table values (mode 01) and the freeze frame snapshot (mode 02)
    float pid_val[DTEC_PID_COUNT];
    bool pid_has[DTEC_PID_COUNT];
    bool pid_fresh[DTEC_PID_COUNT];  // read since the last CSV log row
    float ff_val[DTEC_PID_COUNT];
    bool ff_has[DTEC_PID_COUNT];
    uint16_t ff_dtc;
    int sensor_page;

    // Diagnostics: 0101 MIL/readiness, 03/07/0A codes, 04 clear, 0902 VIN, ATDP
    bool scanning;
    bool codes_read;
    bool clear_armed;
    bool has_monitors;
    bool mil_on;
    int ecu_dtc_count;
    uint16_t mon_supported;
    uint16_t mon_incomplete;
    uint16_t dtc[DTEC_MAX_DTCS];
    uint8_t dtc_kind[DTEC_MAX_DTCS];
    int dtc_count;
    char vin[18];
    char protocol[40];
    char cal_id[17];
    char ecu_name[21];

    // Mode 06 misfire counts per cylinder: this drive cycle and 10-drive average
    uint16_t misfire_drive[DTEC_MAX_CYLINDERS];
    uint16_t misfire_avg[DTEC_MAX_CYLINDERS];
    uint16_t misfire_cyls;  // bit n = cylinder n+1 reported

    // CSV logging to the SD card
    bool logging;
    char log_file[20];
    uint32_t log_rows;

    // UI & Navigation
    DTEC_ScreenMode screen_mode;
    bool sound_enabled;
    bool settings_saved;
    bool settings_session_only;
    float target_boost_setting;
    int gain_setting;
    bool closed_loop;

    // Real Telemetry Rolling History
    float hist_boost[GRAPH_HISTORY_LEN];
    float hist_rpm[GRAPH_HISTORY_LEN];
    float hist_tps[GRAPH_HISTORY_LEN];
    int hist_count;
} DTEC_State;

extern DTEC_State g_dtec;

void dtec_log(const char *msg);

#endif // DTEC_COMMON_H
