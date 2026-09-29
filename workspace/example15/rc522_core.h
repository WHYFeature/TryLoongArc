#ifndef RC522_CORE_H
#define RC522_CORE_H

#include <linux/types.h>

#include "rc522_uapi.h"

#define RC522_GPIO_CLK   125
#define RC522_GPIO_MISO  126
#define RC522_GPIO_MOSI  127
#define RC522_GPIO_CS    128
#define RC522_GPIO_RST   129

int rc522_chip_init(void);
u8 rc522_chip_version(void);
void rc522_bus_diagnose(void);
void rc522_gpio_selftest(void);
int rc522_get_uid(u8 uid[RC522_UID_SIZE]);
int rc522_read_block(u8 block, const u8 key[RC522_KEY_SIZE],
		     u8 data[RC522_BLOCK_SIZE]);
int rc522_write_block(u8 block, const u8 key[RC522_KEY_SIZE],
		      const u8 data[RC522_BLOCK_SIZE]);

#endif
