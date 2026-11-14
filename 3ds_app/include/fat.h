#ifndef DSOBD_3DS_FAT_H
#define DSOBD_3DS_FAT_H

#include <stdbool.h>

/* Platform shim mapping DS libfat root calls to 3DS sdmc:/ storage. */
static inline bool fatInitDefault(void) { return true; }
static inline const char *fatGetDefaultDrive(void) { return "sdmc:/"; }

#endif // DSOBD_3DS_FAT_H
