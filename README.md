# 龙芯 2K500 嵌入式系统实践项目

本仓库收录了基于龙芯 2K500 教学实验平台完成的驱动、用户态测试程序和神经网络训练示例。实验程序在 Linux 主机上交叉编译，然后通过 U 盘等方式部署到开发板；驱动实验需要先加载对应的 `.ko` 模块，再运行用户态测试程序。

本文为根据《嵌入式系统原理实践教材 以龙芯2K500处理器为例》整理复现，并以当前仓库源码定义的参数和行为为准。

## 已实现实验一览

| 实验 | 教材章节 | 主要硬件或接口 | 仓库位置 | 运行入口 | 验收时可观察的效果 |
| --- | --- | --- | --- | --- | --- |
| 基础程序与目标平台运行 | 第 4 章 | 龙芯 2K500 处理器 | [`workspace/example2`](workspace/example2) | `./example2_1` | 终端输出 `3`，验证 LoongArch 目标程序能够装载和运行 |
| 七段数码管 | 第 5 章 | CH422G；GPIO141/SCL、GPIO142/SDA | [`workspace/example5`](workspace/example5) | `./test_ch422g` | 四位数码管从 `0000` 每秒递增到 `0030` |
| LED 输出控制 | 第 8 章 | GPIO136/绿、GPIO138/黄、GPIO137/红 | [`workspace/example8`](workspace/example8) | `./test_led 100` 等 | 按参数分别点亮绿、黄、红灯，可验证全亮和全灭 |
| 高速电机控制 | 第 9 章 | TC118S；GPIO84/INA、GPIO85/INB | [`workspace/example9`](workspace/example9) | `./motor_ctl 0` 至 `3` | 电机呈现待机、正转、反转、制动四种状态 |
| 蜂鸣器控制 | 第 10 章 | GPIO33，低电平有效 | [`workspace/example10`](workspace/example10) | `./test_beep 1` | 蜂鸣器发声；执行参数 `0` 后停止 |
| 三轴加速度采集 | 第 11 章 | STK8BA58；软件 I²C 地址 `0x18` | [`workspace/example11`](workspace/example11) | `./test_stk8ba` | 终端输出 20 组 X、Y、Z 原始值，改变板件姿态时读数变化 |
| 温湿度采集 | 第 16 章 | AHT20；软件 I²C 地址 `0x38` | [`workspace/example16`](workspace/example16) | `./test_aht20` | 每约 2 秒输出原始字节、温度和相对湿度 |
| 4×4 矩阵键盘 | 第 18 章 | GPIO103-106 行输入、GPIO107-110 列输出 | [`workspace/example18`](workspace/example18) | `./test_key 1000` 或 `./test_all` | 按键使对应行电平变化；完整扫描程序显示 4×4 按键位置 |
| 龙芯神经网络训练 | 第 21 章 | 处理器、内存、文件系统、LibTorch | [`workspace/train_af`](workspace/train_af) | `./train` | 输出训练轮次、损失、准确率和耗时，结束后生成 `model.pt` |

## 运行环境

### 编译主机

- Linux 主机或虚拟机；
- LoongArch64 交叉编译器，仓库 Makefile 默认前缀为 `loongarch64-linux-gnu-`；
- 与开发板内核匹配的 Linux 内核源码。多个 Makefile 当前将 `KDIR` 写为 `/home/user/loonarch/linux-5.10-2k500-cbd-src`，若本机路径不同，需要先修改；
- 编译神经网络程序时还需要适用于 LoongArch64 的 LibTorch。

### 开发板

- 龙芯 2K500 教学实验箱及对应扩展板；
- 能够以 root 权限执行 `insmod`、`rmmod` 和访问 `/dev/*` 设备节点；
- 可通过串口、SSH 或 MobaXterm 操作终端。

### 挂载 U 盘

先确认实际设备名，再进行挂载。以下命令沿用验收流程中的 `/dev/sda1` 示例，设备名不同时应替换，不能直接照抄。

```sh
fdisk -l
mkdir -p /mnt/usb
mount /dev/sda1 /mnt/usb
cd /mnt/usb
```

驱动模块和测试程序可以位于不同目录。执行时先进入驱动目录加载 `.ko`，再进入测试程序目录运行可执行文件。模块已经加载时不要重复执行 `insmod`。

## 编译方法

多数实验目录中的 Makefile 会同时生成驱动模块和用户态程序：

```sh
cd workspace/example8
make
```

编译成功后会得到类似 `led.ko` 和 `test_led` 的文件。`.ko` 是需要部署到开发板的最终驱动模块，不是应删除的中间文件。

数码管和电机实验将驱动与测试程序分开放置：

```sh
# 数码管
(cd workspace/example5/driver && make)
(cd workspace/example5/test && make)

# 电机驱动和自动四状态测试程序
(cd workspace/example9/motor && make)
(cd workspace/example9/test && make)

# motor_ctl 未包含在该目录 Makefile 的目标中，需要单独编译
loongarch64-linux-gnu-gcc -O2 \
  workspace/example9/test/motor_ctl.c \
  -o workspace/example9/test/motor_ctl
```

基础程序和完整键盘扫描程序分别执行：

```sh
make -C workspace/example2
make -C workspace/example18/key_scan
```

## 各实验执行方法

以下命令均在开发板上执行。示例假定 `.ko` 与测试程序已经复制到当前实验目录；如果文件分开放置，请先切换到对应目录。

### 1 基础程序与目标平台运行

代码位置：[`workspace/example2/example2_1.c`](workspace/example2/example2_1.c)，编译规则位于 [`workspace/example2/Makefile`](workspace/example2/Makefile)。

```sh
uname -m
uname -r
chmod +x ./example2_1
./example2_1
```

预期终端输出：

```text
3
```

验收流程正文中的 `./test` 是泛称；仓库和教材中的实际目标文件名为 `example2_1`。

### 2 七段数码管

驱动位于 [`workspace/example5/driver`](workspace/example5/driver)，测试程序位于 [`workspace/example5/test`](workspace/example5/test)。驱动通过 GPIO141 提供时钟，通过 GPIO142 发送数据，并创建 `/dev/ch422g`。

```sh
insmod ./ch422g.ko
ls -l /dev/ch422g
chmod +x ./test_ch422g
./test_ch422g
rmmod ch422g
```

`test_ch422g` 将数字转换为七段段码，四位数码管从 `0000` 开始，每隔约 1 秒加 1，到 `0030` 后自动退出，全程约 31 秒。卸载驱动不等于发送清屏命令，因此 `rmmod` 后不保证数码管立即熄灭。

### 3 LED 输出控制

驱动与测试程序位于 [`workspace/example8`](workspace/example8)。三位参数按“绿、黄、红”排列，`1` 表示点亮，`0` 表示熄灭。

```sh
insmod ./led.ko
ls -l /dev/led

./test_led 000   # 全灭
./test_led 100   # 绿灯亮
./test_led 010   # 黄灯亮
./test_led 001   # 红灯亮
./test_led 111   # 全亮
./test_led 000   # 恢复全灭

rmmod led
```

对应关系为 GPIO136/绿灯、GPIO138/黄灯、GPIO137/红灯。每条命令可停留约 2 秒，便于观察参数、GPIO 与实物颜色的对应关系。

### 4 蜂鸣器

驱动与测试程序位于 [`workspace/example10`](workspace/example10)。蜂鸣器连接 GPIO33，硬件为低电平有效，但用户程序仍使用直观的 `1=开启`、`0=关闭` 参数；驱动负责完成逻辑转换。

```sh
insmod ./beep.ko
ls -l /dev/beep

./test_beep 1    # 发声，驱动将 GPIO33 置 0
./test_beep 0    # 关闭，驱动将 GPIO33 置 1

rmmod beep
```

卸载模块前必须先执行 `./test_beep 0`，确认蜂鸣器已经关闭。

### 5 高速电机

驱动位于 [`workspace/example9/motor`](workspace/example9/motor)，用户程序位于 [`workspace/example9/test`](workspace/example9/test)。GPIO84、GPIO85 分别连接电机驱动器的 INA、INB；GPIO 只提供控制信号，电机电流由 TC118S 驱动电路提供。

```sh
insmod ./motor.ko
ls -l /dev/motor

./motor_ctl 0    # INA=0, INB=0：待机
./motor_ctl 1    # INA=1, INB=0：正转
./motor_ctl 2    # INA=0, INB=1：反转
./motor_ctl 3    # INA=1, INB=1：制动

rmmod motor
```

也可以运行自动测试程序：

```sh
./test_motor
```

`test_motor` 按“正转、待机、反转、制动”的顺序运行，每个阶段约 3 秒。正反转方向以现场统一的观察方向为准；GPIO 回读只能验证控制端电平，不能当作电机转速反馈。

> **电机命令差异：**验收流程末尾速查表写有“`motor_ctl 2` 开、`motor_ctl 1` 关”，但当前 [`motor_ctl.c`](workspace/example9/test/motor_ctl.c) 并未定义简单的开关语义，而是采用上面的四状态参数。实际验收应以重新编译后的当前源码行为为准，并在上板前完成一次彩排。

### 6 4×4 矩阵键盘

驱动和单列测试程序位于 [`workspace/example18`](workspace/example18)。GPIO103-106 是四条行输入，GPIO107-110 是四条列输出。

```sh
insmod ./key.ko
ls -l /dev/key

./test_key 1000  # 选通 GPIO107
# Ctrl+C 退出后再测试下一列
./test_key 0100  # 选通 GPIO108
```

`test_key` 按 GPIO103 到 GPIO110 的顺序持续打印八个电平值。前四个数是行输入，后四个数是列输出；按键位置由“被选中的列”和“发生变化的行”的交点确定。还可使用 `0010`、`0001` 测试其余两列。

完整 4×4 自动扫描程序位于 [`workspace/example18/key_scan`](workspace/example18/key_scan)，第一次使用时需要校准实际键位：

```sh
cd key_scan
./test_all --calibrate
./test_all
```

校准过程会在当前目录生成 `keymap.conf`。之后可用 `./test_all 1000` 将显示间隔改为 1000 毫秒；允许范围为 100-60000 毫秒。使用 `Ctrl+C` 正常退出后再卸载驱动：

```sh
rmmod key
```

更完整的校准说明见 [`workspace/example18/key_scan/README.md`](workspace/example18/key_scan/README.md)。

### 7 三轴加速度采集

驱动与测试程序位于 [`workspace/example11`](workspace/example11)。教材章节标题使用“陀螺仪”，实际器件和当前程序采集的是 STK8BA58 三轴加速度原始数据。

```sh
# AHT20 与本实验共用 GPIO68/69，必须先确认 aht20 未加载
rmmod aht20 2>/dev/null || true

insmod ./stk8ba.ko
ls -l /dev/stk8ba
./test_stk8ba
rmmod stk8ba
```

STK8BA58 使用 GPIO68/SDA、GPIO69/SCL，七位 I²C 地址为 `0x18`，GPIO100 用于事件通知。程序每 0.5 秒读取一次 0x02-0x07 六个寄存器，共输出 20 组 X、Y、Z 的 12 位有符号原始值，约 10 秒后结束。

静置时仍会测到重力分量，因此三个轴不应被要求全部为 0；安全地改变传感器所在板件姿态后，三个轴的读数分配应发生变化。

### 8 AHT20 温湿度采集

驱动与测试程序位于 [`workspace/example16`](workspace/example16)。AHT20 与 STK8BA58 共用 GPIO68/SDA、GPIO69/SCL，但地址为 `0x38`，两个独立驱动必须按顺序使用，不能同时占用 GPIO。

```sh
# 先结束三轴程序并释放共用 GPIO
rmmod stk8ba 2>/dev/null || true

insmod ./aht20.ko
ls -l /dev/aht20
./test_aht20

# 观察若干组数据后按 Ctrl+C
rmmod aht20
```

驱动发送 `AC 33 00` 触发测量。程序约每 2 秒输出一组 `RAW` 原始字节、`Temp` 摄氏温度和 `Hum` 相对湿度，结果保留一位小数。当前测试程序忽略第 7 字节 CRC，显示精度也不代表已经完成计量校准。

### 9 龙芯神经网络训练

训练源码位于 [`workspace/train_af`](workspace/train_af)，使用 LibTorch 实现一维信号分类。主要文件如下：

- [`main.cpp`](workspace/train_af/main.cpp)：设置超参数、设备、数据集和 30 轮训练，结束后保存 `model.pt`；
- [`dataset.cpp`](workspace/train_af/dataset.cpp)：读取 CSV 索引和 TXT 序列数据；
- [`model.cpp`](workspace/train_af/model.cpp)：定义一维卷积网络；
- [`trainer.cpp`](workspace/train_af/trainer.cpp)：执行训练、测试并输出损失、准确率和耗时；
- [`CMakeLists.txt`](workspace/train_af/CMakeLists.txt)：查找 `/tmp/loongTensor` 下的 LibTorch 并生成 `train`。

运行前必须保证以下内容同时存在：

1. `/tmp/loongTensor` 是可用的 LoongArch64 LibTorch 运行环境；
2. `train` 的当前工作目录下有 `data/indice/test_indice.csv` 和 `data/training_dataset/`；
3. `train` 具有执行权限。

验收流程使用 U 盘中的 LibTorch 建立软链接：

```sh
rm -rf /tmp/loongTensor
ln -s /mnt/usb/loongArc/train/loongTensor /tmp/loongTensor
ls -ld /tmp/loongTensor
```

然后进入同时包含 `train` 和 `data` 的目录运行：

```sh
chmod +x ./train
./train
```

当前代码默认训练 30 轮。终端会输出训练损失、测试平均损失、准确率和批次耗时，完整结束后在当前目录生成 `model.pt`。教材示例截图中还出现过 F1 字段，但当前仓库的 `trainer.cpp` 没有计算或输出 F1，验收时不应宣称该指标。LibTorch 内置 `cpuinfo` 可能提示 LoongArch 未受支持；教材和验收流程均说明该提示可能影响 CPU 拓扑、缓存或 SIMD 能力探测，但不等同于本次训练失败，应继续根据实际训练输出和最终模型文件判断结果。

## 推荐验收顺序

1. 基础程序，确认目标程序能够执行；
2. 数码管、LED、蜂鸣器和电机，依次验证输出设备；
3. 矩阵键盘，验证 GPIO 行列输入；
4. STK8BA58 三轴采集，结束并卸载驱动；
5. AHT20 温湿度采集，避免与 STK8BA58 同时占用 GPIO68/69；
6. 神经网络训练，区分现场运行片段和此前完整训练记录。

结束演示前，应确认电机和蜂鸣器已关闭、持续采集程序已退出，并保留训练日志和 `model.pt` 等结果文件。

## 常见问题

### `insmod` 后没有出现设备节点

先查看内核日志和模块状态：

```sh
dmesg | tail -n 50
lsmod
ls -l /dev/ch422g /dev/led /dev/motor /dev/beep /dev/key /dev/stk8ba /dev/aht20
```

重点检查模块是否与当前运行内核匹配，以及 GPIO 是否已经被其他模块占用。

### 执行文件提示 `Permission denied`

```sh
chmod +x ./可执行文件名
```

如果 U 盘以 `noexec` 方式挂载，需要把程序复制到开发板可执行目录，或调整挂载参数。

### 执行文件提示格式错误

使用 `file` 检查程序架构。测试程序必须由 LoongArch64 交叉编译器生成，不能把 x86 主机程序直接复制到开发板运行。

```sh
file ./test_led
uname -m
```
