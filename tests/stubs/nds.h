#ifndef DSOBD_TEST_NDS_H
#define DSOBD_TEST_NDS_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#define BIT(n) (1u << (n))
#define KEY_A BIT(0)
#define KEY_B BIT(1)
#define KEY_SELECT BIT(2)
#define KEY_START BIT(3)
#define KEY_UP BIT(6)
#define KEY_DOWN BIT(7)
#define KEY_R BIT(8)
#define KEY_L BIT(9)
#define KEY_X BIT(10)
#define KEY_Y BIT(11)
typedef enum { SoundFormat_16Bit = 1 } SoundFormat;
void DC_FlushRange(const void *, unsigned);
void soundEnable(void);
void soundKill(int);
int soundPlaySampleChannel(int, const void *, SoundFormat, unsigned, unsigned, unsigned, unsigned, bool, unsigned);
#endif
