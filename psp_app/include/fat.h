#ifndef DSOBD_PSP_FAT_H
#define DSOBD_PSP_FAT_H

#include <stdbool.h>
#include <pspiofilemgr.h>

/* Platform shim mapping DS libfat root calls to PSP Memory Stick ms0:/ storage. */
static inline bool fatInitDefault(void) {
    SceUID dfd = sceIoDopen("ms0:/");
    if (dfd >= 0) {
        sceIoDclose(dfd);
        return true;
    }
    dfd = sceIoDopen("ms0:/PSP");
    if (dfd >= 0) {
        sceIoDclose(dfd);
        return true;
    }
    return false;
}

static inline const char *fatGetDefaultDrive(void) {
    return "ms0:/";
}

#endif // DSOBD_PSP_FAT_H
