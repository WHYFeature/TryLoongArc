// SPDX-License-Identifier: MIT

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "rc522_uapi.h"

#define DEVICE_PATH "/dev/rfid_dev"

static void usage(const char *program)
{
	fprintf(stderr,
		"Usage:\n"
		"  %s version\n"
		"  %s uid\n"
		"  %s read BLOCK [KEY_A_HEX]\n"
		"  %s write BLOCK TEXT [KEY_A_HEX]\n"
		"  %s hello BLOCK [KEY_A_HEX]\n"
		"KEY_A_HEX is 12 hex digits; default is FFFFFFFFFFFF.\n",
		program, program, program, program, program);
}

static int parse_block(const char *text, unsigned char *block)
{
	char *end;
	long value = strtol(text, &end, 0);

	if (*text == '\0' || *end != '\0' || value < 0 || value > 63)
		return -1;
	*block = (unsigned char)value;
	return 0;
}

static int hex_value(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	c = (char)tolower((unsigned char)c);
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return -1;
}

static int parse_key(const char *text, struct rc522_key *key)
{
	int high;
	int low;
	int i;

	if (!text) {
		memset(key->bytes, 0xff, sizeof(key->bytes));
		return 0;
	}
	if (strlen(text) != RC522_KEY_SIZE * 2)
		return -1;
	for (i = 0; i < RC522_KEY_SIZE; i++) {
		high = hex_value(text[i * 2]);
		low = hex_value(text[i * 2 + 1]);
		if (high < 0 || low < 0)
			return -1;
		key->bytes[i] = (unsigned char)((high << 4) | low);
	}
	return 0;
}

static void dump_block(const unsigned char data[RC522_BLOCK_SIZE])
{
	int i;

	printf("hex  :");
	for (i = 0; i < RC522_BLOCK_SIZE; i++)
		printf(" %02x", data[i]);
	printf("\nascii: ");
	for (i = 0; i < RC522_BLOCK_SIZE; i++)
		putchar(isprint(data[i]) ? data[i] : '.');
	putchar('\n');
}

static int configure_access(int fd, unsigned char block,
			    const struct rc522_key *key)
{
	if (ioctl(fd, RC522_IOC_SET_BLOCK, &block) < 0) {
		perror("RC522_IOC_SET_BLOCK");
		return -1;
	}
	if (ioctl(fd, RC522_IOC_SET_KEY_A, key) < 0) {
		perror("RC522_IOC_SET_KEY_A");
		return -1;
	}
	return 0;
}

int main(int argc, char **argv)
{
	struct rc522_key key;
	struct rc522_uid uid;
	unsigned char data[RC522_BLOCK_SIZE] = { 0 };
	unsigned char verify[RC522_BLOCK_SIZE];
	unsigned char block;
	unsigned char version;
	const char *key_text = NULL;
	ssize_t count;
	int fd;
	int i;

	if (argc < 2) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	fd = open(DEVICE_PATH, O_RDWR);
	if (fd < 0) {
		perror("open " DEVICE_PATH);
		return EXIT_FAILURE;
	}

	if (strcmp(argv[1], "version") == 0) {
		if (ioctl(fd, RC522_IOC_GET_VERSION, &version) < 0) {
			perror("get version");
			goto fail;
		}
		printf("MFRC522 VersionReg: 0x%02x\n", version);
		goto success;
	}

	if (strcmp(argv[1], "uid") == 0) {
		printf("Hold a card over the reader...\n");
		if (ioctl(fd, RC522_IOC_GET_UID, &uid) < 0) {
			perror("get uid");
			goto fail;
		}
		printf("UID:");
		for (i = 0; i < RC522_UID_SIZE; i++)
			printf(" %02x", uid.bytes[i]);
		putchar('\n');
		goto success;
	}

	if (argc < 3 || parse_block(argv[2], &block) < 0) {
		usage(argv[0]);
		goto fail;
	}

	if (strcmp(argv[1], "read") == 0)
		key_text = argc >= 4 ? argv[3] : NULL;
	else if (strcmp(argv[1], "write") == 0) {
		if (argc < 4) {
			usage(argv[0]);
			goto fail;
		}
		key_text = argc >= 5 ? argv[4] : NULL;
	} else if (strcmp(argv[1], "hello") == 0)
		key_text = argc >= 4 ? argv[3] : NULL;
	else {
		usage(argv[0]);
		goto fail;
	}

	if (parse_key(key_text, &key) < 0) {
		fprintf(stderr, "Invalid Key A; use exactly 12 hex digits.\n");
		goto fail;
	}
	if (configure_access(fd, block, &key) < 0)
		goto fail;

	if (strcmp(argv[1], "read") == 0) {
		printf("Hold the card still while reading block %u...\n", block);
		count = read(fd, data, sizeof(data));
		if (count != sizeof(data)) {
			if (count < 0)
				perror("read block");
			else
				fprintf(stderr, "short read: %zd\n", count);
			goto fail;
		}
		dump_block(data);
		goto success;
	}

	if (block == 0 || block % 4 == 3) {
		fprintf(stderr,
			"Refusing to write manufacturer block 0 or a sector trailer.\n");
		goto fail;
	}
	if (strcmp(argv[1], "hello") == 0)
		memcpy(data, "Hello world", strlen("Hello world"));
	else
		memcpy(data, argv[3],
		       strlen(argv[3]) < sizeof(data) ? strlen(argv[3]) : sizeof(data));

	printf("Hold the card still while writing block %u...\n", block);
	count = write(fd, data, sizeof(data));
	if (count != sizeof(data)) {
		if (count < 0)
			perror("write block");
		else
			fprintf(stderr, "short write: %zd\n", count);
		goto fail;
	}
	usleep(100000);
	count = read(fd, verify, sizeof(verify));
	if (count != sizeof(verify)) {
		perror("verify read");
		goto fail;
	}
	dump_block(verify);
	if (memcmp(data, verify, sizeof(data)) != 0) {
		fprintf(stderr, "Verification failed.\n");
		goto fail;
	}
	printf("Write and verification succeeded.\n");

success:
	close(fd);
	return EXIT_SUCCESS;
fail:
	close(fd);
	return EXIT_FAILURE;
}
