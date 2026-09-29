#ifndef __STK8BA_H__
#define __STK8BA_H__

#define STK8BA_CHIP_ID       0x00

#define STK8BA_XOUT1         0x02
#define STK8BA_XOUT2         0x03

#define STK8BA_YOUT1         0x04
#define STK8BA_YOUT2         0x05

#define STK8BA_ZOUT1         0x06
#define STK8BA_ZOUT2         0x07

#define STK8BA_INTSTS1       0x09
#define STK8BA_INTSTS2       0x0A
#define STK8BA_EVENTINFO1    0x0B

#define STK8BA_DATASETUP     0x13
#define STK8BA_SWRST         0x14

#define STK8BA_INTEN1        0x16
#define STK8BA_INTEN2        0x17

#define STK8BA_INTMAP1       0x19
#define STK8BA_INTMAP2       0x1A

#define STK8BA_INTCFG1       0x20
#define STK8BA_INTCFG2       0x21

#define STK8BA_INTFCFG       0x34

#define STK8BA_OFSTX         0x38
#define STK8BA_OFSTY         0x39
#define STK8BA_OFSTZ         0x3A


/*
 * 教材给出的 GPIO
 */
#define SDA_GPIO             68
#define SCL_GPIO             69
#define INT_GPIO             100


/*
 * STK8BA58 I2C 地址
 */
#define DEVICE_ADDR          0x18

#define DEVICE_WRITE_ADDR    ((DEVICE_ADDR << 1) | 0)
#define DEVICE_READ_ADDR     ((DEVICE_ADDR << 1) | 1)


/*
 * GPIO68/69 位于 GPIO64~71 的复用寄存器
 * GPIO100 位于 GPIO96~103 的复用寄存器
 */
#define GPCFG1               0x1fe104c0
#define GPCFG2               0x1fe104b0


#endif