#ifndef RC522_UAPI_H
#define RC522_UAPI_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define RC522_BLOCK_SIZE 16
#define RC522_UID_SIZE    4
#define RC522_KEY_SIZE    6

struct rc522_uid {
	__u8 bytes[RC522_UID_SIZE];
};

struct rc522_key {
	__u8 bytes[RC522_KEY_SIZE];
};

#define RC522_IOC_MAGIC       'r'
#define RC522_IOC_SET_BLOCK   _IOW(RC522_IOC_MAGIC, 1, __u8)
#define RC522_IOC_SET_KEY_A   _IOW(RC522_IOC_MAGIC, 2, struct rc522_key)
#define RC522_IOC_GET_UID     _IOR(RC522_IOC_MAGIC, 3, struct rc522_uid)
#define RC522_IOC_GET_VERSION _IOR(RC522_IOC_MAGIC, 4, __u8)

#endif
