# 龙芯2K500 RC522 NFC/RFID实验复现

该代码对应教材第15章。龙芯2K500通过GPIO125—129模拟SPI访问MFRC522，驱动
注册字符设备`/dev/rfid_dev`，支持读取4字节UID、选择MIFARE Classic块、设置
Key A、读取16字节数据块和写入16字节数据块。

## 硬件映射

| 信号 | GPIO |
|---|---:|
| CLK | 125 |
| MISO | 126 |
| MOSI | 127 |
| NSS/CS | 128 |
| RST | 129 |

确认扩展板排线已插紧、RC522刷卡区域供电正常。代码针对教材的Linux 5.10环境。

## 编译

必须使用与目标板正在运行的内核完全匹配的源码树、配置和交叉编译器：

默认Makefile环境已经配置为：

```make
KDIR := /home/user/loonarch/linux-5.10-2k500-cbd-src
ARCH := loongarch
CROSS_COMPILE := loongarch64-linux-gnu-
CC := $(CROSS_COMPILE)gcc
```

直接编译：

```sh
make
```

生成`rc522.ko`与`rc522_test`。

## 加载和硬件检测

```sh
insmod rc522.ko
dmesg | tail -n 20
ls -l /dev/rfid_dev
./rc522_test version
```

常见MFRC522 VersionReg为`0x91`或`0x92`，兼容芯片也可能返回其他非`0x00`、
非`0xff`的值。如果设备树或其他驱动已经设置好引脚复用，可以使用：

```sh
insmod rc522.ko configure_mux=0
```

## 读取UID

把卡放在刷卡区域并保持不动：

```sh
./rc522_test uid
```

## 读取数据块

默认Key A为`FFFFFFFFFFFF`。读取第1块：

```sh
./rc522_test read 1
```

指定Key A：

```sh
./rc522_test read 1 A0A1A2A3A4A5
```

## 写入与回读验证

将`Hello world`写入第1块并立即回读比较：

```sh
./rc522_test hello 1
```

写入自定义文本，超过16字节的部分会被截断：

```sh
./rc522_test write 1 "Loongson 2K500"
```

为防止卡片损坏，驱动和测试程序均禁止写入厂商块0以及扇区尾块
`3、7、11……63`。这些尾块保存密钥和访问控制位。请先使用可擦写测试卡，
不要使用门禁卡、校园卡或其他真实业务卡做写入实验。

## 卸载

```sh
rmmod rc522
```

## 验收标准

1. 模块加载后`dmesg`打印有效的VersionReg，并出现`/dev/rfid_dev`。
2. 连续读取同一卡片，UID稳定一致。
3. 第1块能读取16字节数据。
4. `hello 1`写入后，回读内容前11字节为`Hello world`，剩余字节为0。
5. 写块0或扇区尾块被拒绝。
6. 模块反复加载、卸载不会残留设备节点或占用GPIO。

## 常见问题

- `VersionReg=00/ff`：通常是排线、供电、GPIO复用、MISO/MOSI或CS连接问题。
- 遇到`VersionReg=00/ff`时，先加载诊断模式：

  ```sh
  insmod rc522.ko strict_probe=0 debug_bus=1 spi_delay_us=10
  dmesg | tail -n 40
  ./rc522_test version
  ```

  如果连续8次读取始终为`00`且日志中的`MISO=0`，重点检查RC522供电、排线、
  MISO(GPIO126)、CS(GPIO128)和RST(GPIO129)。若MISO会变化但寄存器仍错误，可
  尝试备用采样边沿：

  ```sh
  rmmod rc522
  insmod rc522.ko strict_probe=0 debug_bus=1 spi_delay_us=10 sample_falling=1
  ./rc522_test version
  ```

  正常确认版本后应恢复严格模式，不要长期使用`strict_probe=0`。

  排除硬件故障后，可验证SoC的GPIO输入路径：

  ```sh
  rmmod rc522 2>/dev/null
  insmod rc522.ko strict_probe=0 debug_bus=1 gpio_selftest=1 spi_delay_us=10
  dmesg | tail -n 40
  ```

  正常结果必须是`input-path self-test low=0 high=1 restore_input=0`。如果不是，
  问题位于GPIO编号、GPIO方向切换或该内核的GPIO实现，而不是RC522协议代码。

  若自检正常但寄存器仍全0，测试教材中MISO/MOSI编号在当前内核下是否相反：

  ```sh
  rmmod rc522
  insmod rc522.ko strict_probe=0 debug_bus=1 swap_data=1 spi_delay_us=10
  dmesg | tail -n 40
  ./rc522_test version
  ```
- `No such device`：未检测到卡；将卡贴近刷卡区域并保持不动。
- `Permission denied`：密钥错误，或者尝试写受保护块。
- `Input/output error`：通信校验或卡片ACK失败，检查供电和软件SPI时序。
- `Invalid module format`：模块与目标板内核版本或配置不一致。

## 相比教材的修正

- 补全教材展示中缺失的SPI读取采样逻辑。
- 认证帧只复制4字节UID，避免教材代码从UID数组越界读取6字节。
- ioctl使用带类型和长度的标准命令，避免裸数字命令冲突。
- 每个打开文件独立保存块号和密钥，并使用互斥锁串行化硬件访问。
- 对用户指针、长度、块号和所有错误返回进行检查。
- 完整回收GPIO、映射区和misc设备，并在卸载时恢复原引脚复用寄存器。
- 默认禁止写厂商块和扇区尾块，降低测试中锁卡的风险。

说明：本实现面向教材使用的4字节UID MIFARE Classic卡，不包含7/10字节UID级联
防冲突，也不支持手机NFC的NDEF协议。
