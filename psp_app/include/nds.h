#ifndef DSOBD_PSP_NDS_H
#define DSOBD_PSP_NDS_H

/* Platform shim allowing shared DTEC modules to compile on PSP SDK. */
#include <stdint.h>
#include <stdbool.h>

#ifndef BIT
#define BIT(n) (1U << (n))
#endif

#endif // DSOBD_PSP_NDS_H
