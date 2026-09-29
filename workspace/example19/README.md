# 龙芯2K500红外遥控实验复现

本目录对应教材第19章，目标环境为龙芯2K500实验箱、Linux 5.10 和
LoongArch64 交叉工具链。驱动从 GPIO121 接收 NEC 红外信号，并通过 Linux
input 子系统生成按键事件。

## 文件

- `irdrv.c`：GPIO中断、NEC解码和input设备驱动。
- `ir_keymap.h`：教材遥控器命令码到Linux按键码的映射。
- `ir_test.c`：用户空间测试程序，可自动定位input event节点。
- `Makefile`：内核模块和测试程序的交叉编译规则。

## 一、准备环境

1. 确认目标板运行内核版本：`uname -r`。
2. 准备与目标板完全匹配、已经配置并至少执行过一次构建的Linux 5.10源码树。
3. 准备教材所用的`loongarch64-linux-gnu-`交叉工具链。
4. 确认实验箱红外接收模块连接到GPIO121。板载模块通常不需要额外接线。

内核源码的配置和版本必须与目标板一致，否则`insmod`可能报
`Invalid module format`。

## 二、编译

在x86 Linux主机或教材虚拟机中进入本目录：

```sh
make KDIR=/absolute/path/to/linux-5.10 \
     CROSS_COMPILE=/absolute/path/to/toolchain/bin/loongarch64-linux-gnu-
```

成功后应得到`irdrv.ko`和`ir_test`。用`file`确认`ir_test`是LoongArch64
可执行文件。

## 三、部署与测试

将`irdrv.ko`和`ir_test`复制到目标板，然后以root执行：

```sh
insmod irdrv.ko debug=1
dmesg | tail -n 20
grep -A6 -B2 loongson-ir /proc/bus/input/devices
chmod +x ir_test
./ir_test
```

`ir_test`会自动查找名称为`loongson-ir-nec`的`/dev/input/eventX`。也可以手动
指定：

```sh
./ir_test /dev/input/event0
```

按下遥控器按键后，应同时看到press和release事件。长按按键时，NEC重复帧会
继续生成同一按键事件。

测试结束：

```sh
rmmod irdrv
```

## 四、验收标准

1. `insmod`成功，`dmesg`出现GPIO121、IRQ号和input设备注册信息。
2. `/proc/interrupts`中的`loongson-ir-nec`计数会随按键增加。
3. 21个教材按键均能映射到正确名称。
4. 单击不重复、不丢键；长按能持续产生事件。
5. `rmmod`后对应`eventX`消失，重新加载模块仍能正常工作。

## 五、故障排查

- **完全没有中断**：检查红外模块供电、GPIO121连接和引脚复用；执行
  `grep loongson-ir /proc/interrupts`。如果设备树已经配置GPIO复用，可尝试
  `insmod irdrv.ko configure_mux=0 debug=1`。
- **有中断但没有按键事件**：观察`dmesg`中的未知命令或校验错误。不同遥控器
  的命令字节不同，需要修改`ir_keymap.h`。
- **大量校验错误**：检查接收头是否输出已解调的数字信号，并用示波器确认
  下降沿周期；必要时小幅放宽`irdrv.c`中的时间窗口。
- **找不到event节点**：查看`/proc/bus/input/devices`；节点编号不是固定的，
  不要假定它一定是`event0`。
- **模块格式错误**：重新使用与目标板相同的内核源码、配置、编译器和
  `CONFIG_MODVERSIONS`设置编译。

## 六、与教材代码相比的修正

- 改为下降沿中断，直接测量相邻下降沿间隔，符合NEC周期定义。
- 校验地址、地址反码、命令和命令反码。
- 修复未知按键导致用户程序数组越界的问题。
- input按下和释放事件分别调用`input_sync()`。
- 完整处理初始化失败时的资源回收，并避免input设备重复释放。
- 测试程序按设备名称自动查找`eventX`，不再硬编码`event0`。

注意：教材第19章实验目的中出现的“ZigBee模块”应为红外接收模块的编辑错误。
