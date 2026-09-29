#ifndef IR_KEYMAP_H
#define IR_KEYMAP_H

#include <linux/input-event-codes.h>

/* NEC command byte, Linux input key code, printable name. */
#define IR_KEYMAP(X) \
	X(0x40, KEY_UP,    "UP")      \
	X(0x19, KEY_DOWN,  "DOWN")    \
	X(0x07, KEY_LEFT,  "LEFT")    \
	X(0x09, KEY_RIGHT, "RIGHT")   \
	X(0x15, KEY_ENTER, "ENTER")   \
	X(0x0c, KEY_1,     "Num:1")   \
	X(0x18, KEY_2,     "Num:2")   \
	X(0x5e, KEY_3,     "Num:3")   \
	X(0x08, KEY_4,     "Num:4")   \
	X(0x1c, KEY_5,     "Num:5")   \
	X(0x5a, KEY_6,     "Num:6")   \
	X(0x42, KEY_7,     "Num:7")   \
	X(0x52, KEY_8,     "Num:8")   \
	X(0x4a, KEY_9,     "Num:9")   \
	X(0x16, KEY_0,     "Num:0")   \
	X(0x45, KEY_A,     "A")       \
	X(0x46, KEY_B,     "B")       \
	X(0x47, KEY_C,     "C")       \
	X(0x44, KEY_D,     "D")       \
	X(0x43, KEY_E,     "E")       \
	X(0x0d, KEY_F,     "F")

#endif
