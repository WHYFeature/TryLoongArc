// SPDX-License-Identifier: GPL-2.0

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>

#include "rc522_core.h"

/* MFRC522 commands. */
#define PCD_IDLE          0x00
#define PCD_AUTHENT       0x0e
#define PCD_TRANSCEIVE    0x0c
#define PCD_RESETPHASE    0x0f
#define PCD_CALCCRC       0x03

/* ISO14443A / MIFARE commands. */
#define PICC_REQALL       0x52
#define PICC_ANTICOLL1    0x93
#define PICC_AUTHENT1A    0x60
#define PICC_READ         0x30
#define PICC_WRITE        0xa0
#define PICC_HALT         0x50

/* MFRC522 registers. */
#define CommandReg        0x01
#define ComIEnReg         0x02
#define ComIrqReg         0x04
#define DivIrqReg         0x05
#define ErrorReg          0x06
#define Status2Reg        0x08
#define FIFODataReg       0x09
#define FIFOLevelReg      0x0a
#define ControlReg        0x0c
#define BitFramingReg     0x0d
#define CollReg           0x0e
#define ModeReg           0x11
#define TxControlReg      0x14
#define TxAutoReg         0x15
#define RxSelReg          0x17
#define CRCResultRegM     0x21
#define CRCResultRegL     0x22
#define RFCfgReg          0x26
#define TModeReg          0x2a
#define TPrescalerReg     0x2b
#define TReloadRegH       0x2c
#define TReloadRegL       0x2d
#define VersionReg        0x37

#define RC522_FIFO_SIZE   64
#define RC522_MAX_RX      18

extern bool rc522_debug_bus;
extern bool rc522_swap_data;

static unsigned int spi_delay_us = 10;
static bool sample_falling;

module_param(spi_delay_us, uint, 0444);
MODULE_PARM_DESC(spi_delay_us,
	"software SPI half-cycle delay in microseconds (default: 10)");
module_param(sample_falling, bool, 0444);
MODULE_PARM_DESC(sample_falling,
	"sample MISO after the falling edge for diagnostics (default: false)");

static void rc522_spi_delay(void)
{
	udelay(spi_delay_us ? spi_delay_us : 1);
}

static unsigned int rc522_mosi_gpio(void)
{
	return rc522_swap_data ? RC522_GPIO_MISO : RC522_GPIO_MOSI;
}

static unsigned int rc522_miso_gpio(void)
{
	return rc522_swap_data ? RC522_GPIO_MOSI : RC522_GPIO_MISO;
}

static u8 rc522_spi_transfer(u8 tx)
{
	u8 rx = 0;
	int bit;

	for (bit = 7; bit >= 0; bit--) {
		gpio_set_value(rc522_mosi_gpio(), !!(tx & BIT(bit)));
		rc522_spi_delay();
		gpio_set_value(RC522_GPIO_CLK, 1);
		rc522_spi_delay();
		if (!sample_falling) {
			rx <<= 1;
			if (gpio_get_value(rc522_miso_gpio()))
				rx |= 1;
		}
		gpio_set_value(RC522_GPIO_CLK, 0);
		if (sample_falling) {
			rc522_spi_delay();
			rx <<= 1;
			if (gpio_get_value(rc522_miso_gpio()))
				rx |= 1;
		}
	}

	return rx;
}

static void rc522_write_reg(u8 reg, u8 value)
{
	gpio_set_value(RC522_GPIO_CS, 0);
	rc522_spi_delay();
	rc522_spi_transfer((reg << 1) & 0x7e);
	rc522_spi_transfer(value);
	gpio_set_value(RC522_GPIO_CS, 1);
	rc522_spi_delay();
}

static u8 rc522_read_reg(u8 reg)
{
	u8 value;

	gpio_set_value(RC522_GPIO_CS, 0);
	rc522_spi_delay();
	rc522_spi_transfer(((reg << 1) & 0x7e) | 0x80);
	value = rc522_spi_transfer(0x00);
	gpio_set_value(RC522_GPIO_CS, 1);
	rc522_spi_delay();
	return value;
}

static void rc522_set_bits(u8 reg, u8 mask)
{
	rc522_write_reg(reg, rc522_read_reg(reg) | mask);
}

static void rc522_clear_bits(u8 reg, u8 mask)
{
	rc522_write_reg(reg, rc522_read_reg(reg) & ~mask);
}

static int rc522_calculate_crc(const u8 *data, size_t len, u8 result[2])
{
	unsigned int timeout = 5000;
	size_t i;

	rc522_clear_bits(DivIrqReg, 0x04);
	rc522_write_reg(CommandReg, PCD_IDLE);
	rc522_set_bits(FIFOLevelReg, 0x80);
	for (i = 0; i < len; i++)
		rc522_write_reg(FIFODataReg, data[i]);
	rc522_write_reg(CommandReg, PCD_CALCCRC);

	while (timeout--) {
		if (rc522_read_reg(DivIrqReg) & 0x04) {
			result[0] = rc522_read_reg(CRCResultRegL);
			result[1] = rc522_read_reg(CRCResultRegM);
			return 0;
		}
		udelay(2);
	}

	return -ETIMEDOUT;
}

static int rc522_transceive(u8 command, const u8 *tx, size_t tx_len,
			    u8 *rx, unsigned int *rx_bits)
{
	u8 irq_enable;
	u8 wait_irq;
	u8 irq;
	u8 count;
	u8 last_bits;
	unsigned int timeout = 5000;
	size_t i;

	if (tx_len > RC522_FIFO_SIZE)
		return -EINVAL;

	switch (command) {
	case PCD_AUTHENT:
		irq_enable = 0x12;
		wait_irq = 0x10;
		break;
	case PCD_TRANSCEIVE:
		irq_enable = 0x77;
		wait_irq = 0x30;
		break;
	default:
		return -EINVAL;
	}

	rc522_write_reg(ComIEnReg, irq_enable | 0x80);
	rc522_clear_bits(ComIrqReg, 0x80);
	rc522_write_reg(CommandReg, PCD_IDLE);
	rc522_set_bits(FIFOLevelReg, 0x80);
	for (i = 0; i < tx_len; i++)
		rc522_write_reg(FIFODataReg, tx[i]);

	rc522_write_reg(CommandReg, command);
	if (command == PCD_TRANSCEIVE)
		rc522_set_bits(BitFramingReg, 0x80);

	do {
		irq = rc522_read_reg(ComIrqReg);
		if ((irq & wait_irq) || (irq & 0x01))
			break;
		udelay(5);
	} while (--timeout);

	rc522_clear_bits(BitFramingReg, 0x80);
	rc522_set_bits(ControlReg, 0x80);
	rc522_write_reg(CommandReg, PCD_IDLE);

	if (!timeout)
		return -ETIMEDOUT;
	if (irq & 0x01)
		return -ENODEV;
	if (rc522_read_reg(ErrorReg) & 0x1b)
		return -EIO;

	if (command != PCD_TRANSCEIVE || !rx || !rx_bits)
		return 0;

	count = rc522_read_reg(FIFOLevelReg);
	last_bits = rc522_read_reg(ControlReg) & 0x07;
	*rx_bits = last_bits ? (count - 1) * 8 + last_bits : count * 8;
	if (!count)
		count = 1;
	if (count > RC522_MAX_RX)
		count = RC522_MAX_RX;
	for (i = 0; i < count; i++)
		rx[i] = rc522_read_reg(FIFODataReg);

	return 0;
}

static void rc522_antenna_on(void)
{
	if (!(rc522_read_reg(TxControlReg) & 0x03))
		rc522_set_bits(TxControlReg, 0x03);
}

static int rc522_request(u8 tag_type[2])
{
	u8 buffer[RC522_MAX_RX] = { PICC_REQALL };
	unsigned int bits = 0;
	int ret;

	rc522_clear_bits(Status2Reg, 0x08);
	rc522_write_reg(BitFramingReg, 0x07);
	rc522_set_bits(TxControlReg, 0x03);
	ret = rc522_transceive(PCD_TRANSCEIVE, buffer, 1, buffer, &bits);
	if (ret)
		return ret;
	if (bits != 16)
		return -EIO;
	tag_type[0] = buffer[0];
	tag_type[1] = buffer[1];
	return 0;
}

static int rc522_anticollision(u8 uid[RC522_UID_SIZE])
{
	u8 buffer[RC522_MAX_RX] = { PICC_ANTICOLL1, 0x20 };
	u8 check = 0;
	unsigned int bits = 0;
	int ret;
	int i;

	rc522_clear_bits(Status2Reg, 0x08);
	rc522_write_reg(BitFramingReg, 0x00);
	rc522_clear_bits(CollReg, 0x80);
	ret = rc522_transceive(PCD_TRANSCEIVE, buffer, 2, buffer, &bits);
	rc522_set_bits(CollReg, 0x80);
	if (ret)
		return ret;
	if (bits < 40)
		return -EIO;

	for (i = 0; i < RC522_UID_SIZE; i++) {
		uid[i] = buffer[i];
		check ^= buffer[i];
	}
	return check == buffer[4] ? 0 : -EIO;
}

static int rc522_select(const u8 uid[RC522_UID_SIZE])
{
	u8 buffer[9] = { PICC_ANTICOLL1, 0x70 };
	unsigned int bits = 0;
	int ret;
	int i;

	for (i = 0; i < RC522_UID_SIZE; i++) {
		buffer[i + 2] = uid[i];
		buffer[6] ^= uid[i];
	}
	ret = rc522_calculate_crc(buffer, 7, &buffer[7]);
	if (ret)
		return ret;
	rc522_clear_bits(Status2Reg, 0x08);
	ret = rc522_transceive(PCD_TRANSCEIVE, buffer, sizeof(buffer),
			       buffer, &bits);
	return !ret && bits == 24 ? 0 : (ret ? ret : -EIO);
}

static int rc522_authenticate(u8 block, const u8 key[RC522_KEY_SIZE],
			      const u8 uid[RC522_UID_SIZE])
{
	u8 buffer[12];
	unsigned int unused = 0;
	int ret;

	buffer[0] = PICC_AUTHENT1A;
	buffer[1] = block;
	memcpy(&buffer[2], key, RC522_KEY_SIZE);
	memcpy(&buffer[8], uid, RC522_UID_SIZE);
	ret = rc522_transceive(PCD_AUTHENT, buffer, sizeof(buffer), NULL, &unused);
	if (ret)
		return ret;
	return rc522_read_reg(Status2Reg) & 0x08 ? 0 : -EACCES;
}

static void rc522_stop_crypto(void)
{
	rc522_clear_bits(Status2Reg, 0x08);
}

static void rc522_halt(void)
{
	u8 buffer[4] = { PICC_HALT, 0x00 };
	unsigned int bits = 0;

	if (!rc522_calculate_crc(buffer, 2, &buffer[2]))
		rc522_transceive(PCD_TRANSCEIVE, buffer, sizeof(buffer),
				 buffer, &bits);
}

static int rc522_prepare_card(u8 uid[RC522_UID_SIZE])
{
	u8 tag_type[2];
	int ret;

	ret = rc522_request(tag_type);
	if (ret)
		return ret;
	ret = rc522_anticollision(uid);
	if (ret)
		return ret;
	return rc522_select(uid);
}

int rc522_chip_init(void)
{
	unsigned int timeout = 100;

	gpio_set_value(RC522_GPIO_CS, 1);
	gpio_set_value(RC522_GPIO_CLK, 0);
	gpio_set_value(RC522_GPIO_RST, 1);
	mdelay(10);
	gpio_set_value(RC522_GPIO_RST, 0);
	mdelay(10);
	gpio_set_value(RC522_GPIO_RST, 1);
	mdelay(100);

	rc522_write_reg(CommandReg, PCD_RESETPHASE);
	while ((rc522_read_reg(CommandReg) & BIT(4)) && --timeout)
		mdelay(1);
	if (!timeout)
		return -ETIMEDOUT;

	rc522_write_reg(ModeReg, 0x3d);
	rc522_write_reg(TReloadRegL, 30);
	rc522_write_reg(TReloadRegH, 0);
	rc522_write_reg(TModeReg, 0x8d);
	rc522_write_reg(TPrescalerReg, 0x3e);
	rc522_write_reg(TxAutoReg, 0x40);
	rc522_write_reg(RxSelReg, 0x86);
	rc522_write_reg(RFCfgReg, 0x7f);
	rc522_antenna_on();
	mdelay(5);
	return 0;
}

u8 rc522_chip_version(void)
{
	return rc522_read_reg(VersionReg);
}

void rc522_bus_diagnose(void)
{
	int i;

	if (!rc522_debug_bus)
		return;

	pr_info("rc522: GPIO levels CLK=%d MISO=%d MOSI=%d CS=%d RST=%d\n",
		gpio_get_value(RC522_GPIO_CLK),
		gpio_get_value(rc522_miso_gpio()),
		gpio_get_value(rc522_mosi_gpio()),
		gpio_get_value(RC522_GPIO_CS),
		gpio_get_value(RC522_GPIO_RST));
	for (i = 0; i < 8; i++)
		pr_info("rc522: raw[%d] Version=0x%02x Command=0x%02x TReloadL=0x%02x MISO=%d\n",
			i, rc522_read_reg(VersionReg), rc522_read_reg(CommandReg),
			rc522_read_reg(TReloadRegL),
			gpio_get_value(rc522_miso_gpio()));
}

void rc522_gpio_selftest(void)
{
	unsigned int gpio = rc522_miso_gpio();
	int low;
	int high;
	int ret;

	/* NSS is high, so a correctly configured MFRC522 releases MISO here. */
	gpio_set_value(RC522_GPIO_CS, 1);
	ret = gpio_direction_output(gpio, 0);
	if (ret) {
		pr_err("rc522: GPIO%u self-test cannot select output: %d\n", gpio, ret);
		return;
	}
	rc522_spi_delay();
	low = gpio_get_value(gpio);
	gpio_set_value(gpio, 1);
	rc522_spi_delay();
	high = gpio_get_value(gpio);
	ret = gpio_direction_input(gpio);
	pr_info("rc522: GPIO%u input-path self-test low=%d high=%d restore_input=%d\n",
		gpio, low, high, ret);
}

int rc522_get_uid(u8 uid[RC522_UID_SIZE])
{
	int ret = rc522_prepare_card(uid);

	if (!ret)
		rc522_halt();
	return ret;
}

int rc522_read_block(u8 block, const u8 key[RC522_KEY_SIZE],
		     u8 data[RC522_BLOCK_SIZE])
{
	u8 uid[RC522_UID_SIZE];
	u8 buffer[RC522_MAX_RX] = { PICC_READ, block };
	unsigned int bits = 0;
	int ret;

	ret = rc522_prepare_card(uid);
	if (ret)
		return ret;
	ret = rc522_authenticate(block, key, uid);
	if (ret)
		goto out;
	ret = rc522_calculate_crc(buffer, 2, &buffer[2]);
	if (ret)
		goto out;
	ret = rc522_transceive(PCD_TRANSCEIVE, buffer, 4, buffer, &bits);
	if (!ret && bits != 144)
		ret = -EIO;
	if (!ret)
		memcpy(data, buffer, RC522_BLOCK_SIZE);
out:
	rc522_stop_crypto();
	rc522_halt();
	return ret;
}

int rc522_write_block(u8 block, const u8 key[RC522_KEY_SIZE],
		      const u8 data[RC522_BLOCK_SIZE])
{
	u8 uid[RC522_UID_SIZE];
	u8 buffer[RC522_MAX_RX] = { PICC_WRITE, block };
	unsigned int bits = 0;
	int ret;

	if (block == 0 || block >= 64 || block % 4 == 3)
		return -EPERM;

	ret = rc522_prepare_card(uid);
	if (ret)
		return ret;
	ret = rc522_authenticate(block, key, uid);
	if (ret)
		goto out;
	ret = rc522_calculate_crc(buffer, 2, &buffer[2]);
	if (ret)
		goto out;
	ret = rc522_transceive(PCD_TRANSCEIVE, buffer, 4, buffer, &bits);
	if (ret || bits != 4 || (buffer[0] & 0x0f) != 0x0a) {
		ret = ret ? ret : -EIO;
		goto out;
	}

	memcpy(buffer, data, RC522_BLOCK_SIZE);
	ret = rc522_calculate_crc(buffer, RC522_BLOCK_SIZE,
				  &buffer[RC522_BLOCK_SIZE]);
	if (ret)
		goto out;
	ret = rc522_transceive(PCD_TRANSCEIVE, buffer, RC522_BLOCK_SIZE + 2,
			       buffer, &bits);
	if (!ret && (bits != 4 || (buffer[0] & 0x0f) != 0x0a))
		ret = -EIO;
out:
	rc522_stop_crypto();
	rc522_halt();
	return ret;
}
