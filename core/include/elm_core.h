#ifndef ELM_CORE_H
#define ELM_CORE_H

// Shared, platform-independent ELM327 protocol engine used by every DSOBD app
// (DS terminal, 3DS, PSP and the turboxs-dtec gauge cluster).
//
// The ELM327 only handles one command at a time: if it receives a character
// while still working on a command it aborts and replies "STOPPED". This core
// therefore never sends a command until the previous one has been answered with
// the '>' prompt (or has timed out).

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ELM_QUEUE_LEN 8
#define ELM_CMD_MAX   24

typedef struct {
    // Sends cmd followed by '\r'. Returns false if the bytes could not be sent.
    bool (*send)(const char *cmd, void *user);
    // Optional: called when a command actually goes out on the wire.
    void (*on_tx)(const char *cmd, void *user);
    // Optional: status messages from the core (timeouts, send failures).
    void (*on_log)(const char *msg, void *user);
    // Optional: every complete response line from the adapter.
    void (*on_line)(const char *line, void *user);
    void *user;
} ElmCallbacks;

typedef struct {
    bool has_rpm;
    uint32_t rpm;

    bool has_speed;
    uint32_t speed_kmh;

    bool has_coolant;
    int coolant_c;

    bool has_map;
    int map_kpa;

    bool has_tps;
    uint8_t tps_raw;    // percent = raw * 100 / 255

    bool has_load;
    uint8_t load_raw;   // percent = raw * 100 / 255

    bool has_voltage;
    char voltage[16];

    bool elm_ready;     // ELM327 banner seen
    char elm_version[32];
    char last_line[64];

    bool has_supported;
    uint32_t supported_01_20;  // bitmap from 0100, MSB = PID 01

    uint32_t lines_rx;
    uint32_t cmds_sent;
    uint32_t timeouts;
    uint32_t stopped;
    uint32_t send_failures;
} ElmData;

typedef struct {
    ElmCallbacks cb;
    ElmData d;

    char line[128];
    size_t line_pos;

    bool connected;
    bool busy;
    char inflight[ELM_CMD_MAX];
    uint32_t sent_ms;
    uint32_t timeout_ms;

    int init_idx;

    char queue[ELM_QUEUE_LEN][ELM_CMD_MAX];
    int q_head;
    int q_count;

    const char *const *poll_cmds;
    int poll_count;
    int poll_idx;
    bool auto_poll;
    uint32_t poll_interval_ms;
    uint32_t last_poll_ms;
} ElmCore;

void elm_init(ElmCore *e, const ElmCallbacks *cb,
              const char *const *poll_cmds, int poll_count);

// Call once the TCP socket to the adapter is open. Starts the init sequence.
void elm_on_connected(ElmCore *e, uint32_t now_ms);
// Call when the socket is closed. Drops anything queued or in flight.
void elm_on_disconnected(ElmCore *e);

// Feed raw bytes received from the adapter.
void elm_feed(ElmCore *e, const uint8_t *data, size_t len, uint32_t now_ms);

// Call every frame. Sends the next queued, init or poll command when the
// adapter is idle, and handles reply timeouts.
void elm_tick(ElmCore *e, uint32_t now_ms);

// Queue a manual command. Returns false if not connected or the queue is full.
bool elm_request(ElmCore *e, const char *cmd);

// True if the vehicle reported the given mode 01 PID as supported, or if
// support is unknown (no 0100 answer yet, or a PID outside 01-20).
bool elm_pid_supported(const ElmCore *e, uint8_t pid);

#endif // ELM_CORE_H
