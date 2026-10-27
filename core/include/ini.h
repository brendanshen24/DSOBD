#ifndef DSOBD_INI_H
#define DSOBD_INI_H

#include <stdbool.h>

// Called once per "key=value" line, with both sides trimmed.
typedef void (*ini_kv_fn)(const char *key, const char *val, void *user);

// Parses a simple ini file ('#' and ';' comments, no sections).
// Returns false if the file could not be opened.
bool ini_parse_file(const char *path, ini_kv_fn on_kv, void *user);

// Accepts 1/0, true/false, yes/no, on/off.
bool ini_parse_bool(const char *val);

#endif // DSOBD_INI_H
