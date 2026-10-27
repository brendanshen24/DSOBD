#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

void ui_init(void);
void ui_set_status(const char *msg);
void ui_render(void);
void ui_log_rx_bytes(const uint8_t *data, size_t len);
void ui_log_tx(const char *cmd);
void ui_log_system(const char *msg);
void ui_clear_log(void);

#endif // UI_H
