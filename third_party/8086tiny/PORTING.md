# 8086tiny 串口后端验证

本目录保留 Revision 1.25 的最小上游输入（`8086tiny.c`、`bios`、`fd.img` 和 BIOS 源码）。
`port/emu_console.h` 是唯一的终端边界：

- `port/emu_console_posix.c` 用于主机 smoke test，标准输入/输出模拟串口字节流。
- `port/emu_console_rtthread.c` 用于 SF32LB525，绑定 `uart1`，对应黄山派板级默认 UART1（PA18 RX、PA19 TX）。

8086tiny 的 BIOS 文本输出通过 `0F 00` 扩展指令进入 `emu_console_tx()`；键盘路径通过 `emu_console_rx()` 注入 BIOS 的键盘中断。因此目标配置不需要 LCD、SDL 或板载按键。

磁盘 I/O 已改为 RAM 盘后端（`port/emu_disk.c`）：`bios` 与 `fd.img` 由
`sdk_probe/src/disk_image.S` 用 `.incbin` 编进 flash，启动时拷进 PSRAM，
仿真核心里的 `open/lseek/read/write` 在编译期重定向到该后端（见
`sdk_probe/src/sf8086_core.c`）。板上不需要挂载任何文件系统。
注意写入只落在 PSRAM 副本上，掉电即失。

## 目标侧构建状态（已实测）

固件工程在 `sdk_probe/`，构建命令：

```sh
cd /Users/wangmian/Fixes/SiFli-SDK && source ./export.sh
cd /Users/wangmian/Fixes/sf8086/sdk_probe/project && scons --board=sf32lb52-lchspi-ulp -j8
```

结果：ROM 335,927 B / 8 MB，RAM 192,340 B / 523,264 B，`main.bin` 309,288 B。

移植时踩到并已解决的四个点：

1. 访客内存（1 MiB）不能是静态数组。SDK 的 PSRAM 段是 PROGBITS 且 LMA=0x60000000，
   静态数组会被写进镜像（1 MiB 数组 → 1.3 GB 的 `main.bin`）。改用运行时从 PSRAM
   分配，`mem` 变成指针（`SF8086_PSRAM_MEM`）。
2. 必须用 `-fsigned-char` 编译仿真核心。SDK 默认 `-funsigned-char`，实测该设置下
   仿真器完全无输出。用 SDK 的 `LOCAL_CCFLAGS` 只对该文件生效。
3. 仿真主循环不返回，必须跑在独立 RT-Thread 线程里（`sdk_probe/src/emulator.c`）。
4. newlib 没有 `ftime()`，由 `port/emu_libc_shim.c` 补齐。

磁盘路径不需要改上游源码：仿真核心通过宏重定向改走 RAM 盘后端，所以
`third_party/8086tiny/8086tiny.c` 里仍然是普通的 `open/read/lseek/write` 调用。
`sf8086` 命令直接以 `bios` / `fd.img` 两个名字打开 RAM 盘，不需要文件系统。

## UART1 控制台归属（FreeDOS 与 RT-Thread shell 抢输入的问题）

现象：运行 DOS 后，给 FreeDOS 的按键会被 RT-Thread 的 msh 吃掉（或反过来）。

原因：`RT_USING_POSIX` 打开时，msh 走的是 newlib `getchar()` → fd 0 → 启动时
`libc_system_init()` 已经把 fd 0 绑到 `/dev/uart1`；而仿真器是直接读同一个 uart1
设备。两个读者争同一个接收缓冲，谁先读谁拿到。

解决（`port/emu_console_rtthread.c` + `port/emu_idle_dev.c`）：保证同一时刻只有一个读者。

- `sf8086` 启动时（在 shell 线程内、刚执行完本命令、尚未回到 getchar）把 stdio 切到
  `/dev/emu_idle`：一个永远没有输入的字符设备，shell 的阻塞读就停在那里（0 CPU）。
  UART1 归仿真器独占。
- 仿真线程里检测到 `Ctrl-]`（0x1D）时**不切换 stdio**，只把内部命令 `sf8086_paused`
  排进 emu_idle 设备并唤醒 shell，然后在信号量上停住自己。
- shell 执行 `sf8086_paused` 时（它自己的线程）再 `libc_stdio_set_console("uart1")` 交还
  UART1，于是 `msh` 提示符与输入恢复正常。`sf8086run` 继续、`sf8086` 重新引导。

关键约束：stdio 的切换必须发生在 shell 线程里（切换会 `fclose` shell 正在读的 FILE）；
不能用 `rt_thread_suspend` 从外部挂起 shell——这版 RT-Thread 的 `rt_thread_suspend`
只接受处于 READY 状态的线程。
