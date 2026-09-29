// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "ir_keymap.h"

#define EXPECTED_DEVICE_NAME "loongson-ir-nec"

struct user_key_entry {
	unsigned int keycode;
	const char *name;
};

#define MAKE_USER_KEY(command, keycode, name) { keycode, name },
static const struct user_key_entry keys[] = {
	IR_KEYMAP(MAKE_USER_KEY)
};
#undef MAKE_USER_KEY

static const char *key_name(unsigned int keycode)
{
	size_t i;

	for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
		if (keys[i].keycode == keycode)
			return keys[i].name;
	return "UNKNOWN";
}

static int open_named_input(char *selected, size_t selected_size)
{
	glob_t paths;
	size_t i;
	int fd = -1;

	if (glob("/dev/input/event*", 0, NULL, &paths) != 0)
		return -1;

	for (i = 0; i < paths.gl_pathc; i++) {
		char name[128] = { 0 };

		fd = open(paths.gl_pathv[i], O_RDONLY);
		if (fd < 0)
			continue;
		if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0 &&
		    strcmp(name, EXPECTED_DEVICE_NAME) == 0) {
			snprintf(selected, selected_size, "%s", paths.gl_pathv[i]);
			globfree(&paths);
			return fd;
		}
		close(fd);
		fd = -1;
	}

	globfree(&paths);
	return -1;
}

int main(int argc, char **argv)
{
	struct input_event event;
	char selected[256] = { 0 };
	const char *path = NULL;
	ssize_t count;
	int fd;

	if (argc > 2) {
		fprintf(stderr, "Usage: %s [/dev/input/eventX]\n", argv[0]);
		return EXIT_FAILURE;
	}

	if (argc == 2) {
		path = argv[1];
		fd = open(path, O_RDONLY);
	} else {
		fd = open_named_input(selected, sizeof(selected));
		path = selected;
	}

	if (fd < 0) {
		fprintf(stderr, "Cannot open IR input device: %s\n", strerror(errno));
		fprintf(stderr, "Try: grep -A6 -B2 loongson-ir /proc/bus/input/devices\n");
		return EXIT_FAILURE;
	}

	printf("Reading %s; press Ctrl-C to stop.\n", path);
	for (;;) {
		count = read(fd, &event, sizeof(event));
		if (count < 0) {
			if (errno == EINTR)
				continue;
			perror("read");
			break;
		}
		if (count != sizeof(event))
			continue;
		if (event.type != EV_KEY)
			continue;

		printf("key=%-7s code=%u value=%s\n",
		       key_name(event.code), event.code,
		       event.value == 1 ? "press" :
		       event.value == 0 ? "release" : "repeat");
		fflush(stdout);
	}

	close(fd);
	return EXIT_FAILURE;
}
