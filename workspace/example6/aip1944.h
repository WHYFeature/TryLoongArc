#ifndef __AIP1944_H__
#define __AIP1944_H__

/*
 * 教材明确使用 GPIO50、GPIO51、GPIO115。
 *
 * 教材正文没有进一步显示三者与 CLK/DIO/STB
 * 的逐一映射，这里继续采用当前实验使用的映射。
 */
#define AIP_CLK_GPIO    50
#define AIP_DIO_GPIO    51
#define AIP_STB_GPIO    115

#define AIP_NAME        "aip"

#endif