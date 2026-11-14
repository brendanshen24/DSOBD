#ifndef DSOBD_PSP_UI_INTERNAL_H
#define DSOBD_PSP_UI_INTERNAL_H

#include "dtec_common.h"
#include "platform_psp.h"
#include "gfx_psp.h"

extern const char *const g_psp_mode_names[PSP_MODE_COUNT];
extern float g_psp_rpm_ease;
extern uint32_t g_psp_hud_frame;

float psp_clamp01(float v);
uint32_t psp_zone_color(float f);
float psp_display_speed(uint32_t kmh);
const char *psp_speed_unit(bool upper);
void psp_format_temp(int c, char *out, size_t n);
void psp_format_boost(float kpa, char *out, size_t n);
float psp_boost_max_display(void);
float psp_boost_display_val(float kpa);
const char *psp_boost_unit(void);

void psp_draw_header(const char *mode_name);
void psp_draw_footer(const char *hint_mode);
void psp_draw_link_dot(float x, float y);
void psp_draw_side_dock(float x, float y0, float w, float card_h, float gap);
void psp_draw_metric_card(float x, float y, float w, float h,
                          const char *label, const char *val, uint32_t val_col,
                          bool has_bar, float frac, uint32_t bar_col);
void psp_draw_shift_lights(float y, float h);
void psp_draw_dial_labels(float cx, float cy, float r, float start_deg, float sweep_deg,
                          int every, float max_val, float red_val, float scale);

void psp_pid_text(int idx, bool freeze, char *out, size_t n);

/* Gauge & scope screens (implemented in dtec_ui_gauges_psp.c) */
void psp_render_multi_gauge(void);
void psp_render_digital_speed(void);
void psp_render_big_speed(void);
void psp_render_arc_tach(void);
void psp_render_sim_dash(void);
void psp_render_gt_style(void);
void psp_render_forza_style(void);
void psp_render_beamng_style(void);
void psp_render_analog_dials(void);
void psp_render_realtime_graph(int scope_cursor, bool scope_scrubbing);
void psp_render_stats(void);

/* Diagnostic & PSP system screens (implemented in dtec_ui_screens_psp.c) */
void psp_render_car_select(void);
void psp_render_codes(int dtc_scroll);
void psp_render_sensors(void);
void psp_render_ecu_info(void);
void psp_render_terminal(int cmd_cursor);
void psp_render_wifi(bool editing_ip, int ip_field, const int ip_octets[4], int port_val);
void psp_render_settings(int setting_row);

#endif // DSOBD_PSP_UI_INTERNAL_H
