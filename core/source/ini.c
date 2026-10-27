#include "ini.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void trim(char *s) {
    char *p = s;
    while (isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);

    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

bool ini_parse_file(const char *path, ini_kv_fn on_kv, void *user) {
    FILE *f = fopen(path, "r");
    if (!f) return false;

    char line[160];
    while (fgets(line, sizeof(line), f)) {
        trim(line);
        if (line[0] == '\0' || line[0] == '#' || line[0] == ';') continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        trim(key);
        trim(val);
        if (on_kv) on_kv(key, val, user);
    }

    fclose(f);
    return true;
}

bool ini_parse_bool(const char *val) {
    return atoi(val) != 0 || strcasecmp(val, "true") == 0 ||
           strcasecmp(val, "yes") == 0 || strcasecmp(val, "on") == 0;
}
