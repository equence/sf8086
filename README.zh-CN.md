# sf8086

8086tiny 在 SiFli SF32LB525（黄山派）上的移植。FreeDOS 运行于板上 Cortex-M33，
通过 UART1 使用。BIOS、启动软盘和 C 编译器存放在固件中，不使用外部存储或文件系统。

[English](README.md)

## 内容

- A: 盘上的 FreeDOS。A: 为 RAM 盘，每次启动从 flash 重建。
- Turbo C 2.01 与小模式运行库，命令形式为 `CC hello.c`。
- 启动盘自带的 FreeDOS 工具集：EDIT、XCOPY、MOVE、DEBUG、FDISK、FORMAT、ZIP 等。
- A: 上约 144 KB 可用空间。
- RT-Thread shell，与 DOS 控制台共用 UART1。

控制台仅支持文本模式，不显示图形模式。

## 需求

- SF32LB525 开发板，板名 `sf32lb52-lchspi-ulp`。
- 含 Arm 工具链的 SiFli SDK。
- 接在 UART1 上的串口终端（PA18 收，PA19 发）。

## 构建

```sh
source /path/to/SiFli-SDK/export.sh
cd sdk_probe/project
scons --board=sf32lb52-lchspi-ulp -j8
```

产物为 `build_sf32lb52-lchspi-ulp_hcpu/main.bin` 和 `sftool_param.json`，供 sftool
或 CodeKit 扩展使用。镜像占用 8 MB 代码区中的约 1.8 MB、523 KB 内部 SRAM 中的 193 KB。

## Shell 命令

串口默认进入 RT-Thread shell。

| 命令 | 作用 |
|------|------|
| `sf8086` | 启动 DOS。在暂停状态下再次执行，则从干净软盘重新引导。 |
| `Ctrl-]` | 暂停 DOS，将 UART1 交回 shell。 |
| `sf8086run` | 继续被暂停的会话。 |
| `psram_smoke` | 检查访客内存与控制台。 |
| `resetinfo` | 报告上次复位的原因。 |
| `help` | 列出其余命令。 |

DOS 运行期间 UART1 由仿真器独占，shell 不读取该端口。`Ctrl-]` 由固件消费，不传递给访客。

## DOS 环境

| 路径 | 内容 |
|------|------|
| `A:\CC.BAT` | `CC hello.c` 编译链接为 `HELLO.EXE`。 |
| `A:\TC` | 编译器、链接器、头文件、小模式运行库。 |
| `A:\TC\SAMPLES` | HELLO.C、SIEVE.C、FILEIO.C、KEYTEST.C。 |
| `A:\FREEDOS\BIN` | FreeDOS 工具集。 |

对 A: 的写入保存在 RAM 中，重启后丢失。DOS 中的操作不会修改 flash 中的镜像。

固件里的镜像默认是 FreeDOS。若改用 MS-DOS，需要用你自己的启动软盘重建镜像，做法见
[tools/floppy/README.md](tools/floppy/README.md)。

Alt 组合键：仿真的 BIOS 以 `Ctrl+A` 作为 Alt 前缀，固件转换终端发出的 `ESC`+键序列。
`KEYTEST` 打印每次按键所接收到的扫描码与 ASCII 值。

## 性能

启动至 DOS 提示符约执行 2.5×10⁸ 条访客指令。在台式机上约需 2 秒；板上 Cortex-M33
主频 240 MHz。访客首次显示提示符时，固件打印实测值：

```text
[8086] DOS prompt reached after <ms> ms, <n> guest instructions
```

## 文档

- 移植笔记、控制台归属与设计约束：
  [third_party/8086tiny/PORTING.md](third_party/8086tiny/PORTING.md)
- 软盘内容与重建方式：[tools/floppy/README.md](tools/floppy/README.md)
- 目录结构与提交约定：[docs/CONVENTIONS.md](docs/CONVENTIONS.md)

## 致谢

- **8086tiny**（MIT，Adrian Cable）：仿真器本体。核心、BIOS 源码与启动软盘存放在
  `third_party/8086tiny/`。对上游文件的两处修改均位于宏之后：终端 I/O 交由移植层处理，
  `mem` 改为指针以便访客内存从 PSRAM 分配。
- **FreeDOS**（GPL-2.0-or-later）：软盘上的 kernel、FreeCOM 与工具集，未作修改。
- **Alley Cat**：随 8086tiny 分发的游戏，保留在软盘中。
- **Turbo C 2.01**（Borland，1989）：C 工具链。官方允许免费使用，再分发权未明确授予。
  构建不含它的镜像的方法见
  [tools/floppy/payload/README.md](tools/floppy/payload/README.md)。
- **SiFli SDK** 与 **RT-Thread**（Apache-2.0）：RTOS、驱动与板级支持，未随仓库分发。
- **Arm GNU Toolchain**（GPL-3.0 + Arm exception）：构建工具链，未随仓库分发。

Turbo C 2.01 收录在此只是为了方便，并不代表其条款允许再分发：Borland 发布它时允许
免费使用，但没有明确授予再分发权。若权利人认为这里的收录构成侵权，请在本仓库提交
issue，我会将其移除。
