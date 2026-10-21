# 目标设备画像（实测，2026-10-08）

探测方式：SSH 只读命令（`tools/device_probe.py`，逐条列出设备上的读取命令，不写入、不删除、不安装）。
原始输出保存在 `logs/device-probe-*.txt`（gitignored，含私有地址，禁止入库）。
本文只记录与移植决策相关的**测量事实**，不含设备地址、凭据与个人归属。

## 1. 硬件与固件

| 项 | 实测值 |
| --- | --- |
| 系统 | ROCKNIX `OS_VERSION=20260809`，`BUILD_BRANCH=next`，`BUILD_ID=08b301df…` |
| 机型标识 | `HW_DEVICE=RK3326`，`HW_CPU=Rockchip RK3326`，hostname `XF35H` |
| 内核 | `6.12.79 SMP`，`uname -m=aarch64`，`getconf LONG_BIT=64` |
| CPU | 4× Cortex-A35（`CPU part 0xd04`，`CPU architecture: 8`，Features `fp asimd evtstrm aes pmull sha1 sha2 crc32`） |
| 内存 | `MemTotal 1005280 kB`，`MemAvailable 684156 kB`（≈1 GB RAM） |
| glibc | `ldd (GNU libc) 2.40` |
| GPU 用户态 | `/usr/lib/libmali.so → libmali.so.1.9.0`（56 MB，`libmali-bifrost-g31-g24p0-wayland-gbm` 家族），`renderD128` 存在 |

**1 GB RAM 是硬约束**：RR3 是 32 位进程 + 主机侧呈现器（若拆进程）+ 正在运行的 `emulationstation`（实测占用 ≈124 MB）。任何"再启一个 EGL/GLES 呈现子进程"的方案都要按这个预算评估。

## 2. 显示几何（分辨率自适应的直接输入）

| 来源 | 实测 |
| --- | --- |
| DRM 首选模式 | `kmsprint`：`Connector 44 DSI-1 (connected)` → `Crtc 36 640x480@60.00 31.080 640/150/60/150/- 480/20/6/12/- 60 (60.00) D` |
| 当前平面 | `Plane 32 fb-id 51 … 0,0 640x480 -> 0,0 640x480`，`FB 51 640x480 XR24`（无缩放，1:1） |
| `/sys/class/drm/card0-DSI-1/modes` | 12 行，去重后唯一值 `640x480` |
| `/sys/class/graphics/fb0/virtual_size` | `640,960`（**fbcon 双页高度，非面板真值**，不可用于像素判定） |
| card1 | 无 `card1-*` 连接器目录（无第二输出） |

**面板 = 640×480，4:3，唯一模式。** 这带来一个必须在规划里承认的后果：

- 参考移植硬编码的 640×480 恰好等于本机面板 → 参考移植在本机上"看起来正常"，**自适应缺失的缺陷（R1–R5）无法用本机直接呈现**；
- 本机可证明的是：几何来自实测（DRM 首选模式）而非硬编码、内容=drawable=面板、offset=0+0、资产桶选择随面板变化；
- 本机**不能**证明的（须留作未验证）：非 4:3 面板上的 letterbox/pillarbox 正确性、大于 640×480 的原生渲染收益。若要主张这些，需要另一台不同分辨率面板的设备或等比放大的软件验证路径，二者目前都没有，所以先不主张。

## 3. 32 位（ARMv7）执行能力 — 决定性利好

| 检查 | 实测 |
| --- | --- |
| `CONFIG_COMPAT` | `=y` |
| `/lib/ld-linux-armhf.so.3` | **存在**（`/usr/lib32/` 下亦有一份，163112 字节） |
| `/usr/lib32` | **743 个条目**，含 `libc.so.6`(1.54 MB armhf)、`libm.so.6`、`libz.so`、`libpng16.so`、`libopenal.so`、`libvorbis*`、`libSDL2-2.0.so.0`、`libEGL.so → libEGL.so.1`、`libGLESv2.so → libGLESv2.so.2`、`libmali.so → libmali.so.1` |
| `ldconfig -p` | 对 `libmali / libGLESv1_CM / libGLESv2 / libEGL / libEGL_mesa / libSDL2*` 同时给出 `(libc6,AArch64)` 与 `(libc6,hard-float)` 两条路径 |
| box86 / box64 | `/usr/bin/box86`、`/usr/bin/box64` 存在（本项目不需要：原生执行可用） |

结论：RR3 的 ARMv7 guest 可在这台 aarch64 固件上**原生执行**（内核 AArch32 EL0 + armhf glibc multilib + armhf GLES/EGL/mali 全栈），无需动态翻译层。这与 `elf_static_report.py` 测得的 guest 侧 0 个 V5 预检阻断项相互印证。

**但 armhf 侧 SDL2 有缺口**：`grep -ac` 计数显示固件 `/usr/lib32/libSDL2-2.0.so.0` 含 `wayland/opengles/dummy` 字符串而 **`kmsdrm=0`、`video_kmsdrm=0`**（无 kmsdrm 视频后端）。设备上另一个 armhf 端口因此自带一份 `host-libs/libSDL2-2.0.so.0`（Debian 版）并把固件版仅作兜底。→ RR3 主机的 SDL2 供给必须留"随包带 armhf SDL2"的路径（对应 `port-runtime-lib-bundling` skill 的手法）。
注：这是 `grep -ac` 字符串计数（存在性证据），不是后端清单的完整解析；正式判定要在真机上让 SDL 报告 `SDL_GetCurrentVideoDriver` 与 `SDL_GetNumRenderDrivers`。

## 4. 浮点 ABI 边界 — 设备侧实测（softfp 桥的依据）

同一台设备上，用 `dd`（按 `readelf -S` 给出的段偏移/长度）取出每个库 `.ARM.attributes` 的原始字节，并与同一库的 `readelf -A` 渲染逐条对照，得到以下**同一编号体系**的证据：

| 被测库（设备自带） | 段 | 属性条数 | `Tag_ABI_VFP_args` |
| --- | --- | --- | --- |
| `/usr/lib32/ld-linux-armhf.so.3` | 59 B @ `0x27946` | 20 | **VFP registers** |
| `/usr/lib32/libc.so.6` | 59 B @ `0x177c6a` | 20 | **VFP registers** |
| `/usr/lib32/libSDL2-2.0.so` | 57 B @ `0x1eec12` | 19 | **VFP registers** |
| `/usr/lib32/libmali-bifrost-g31-g24p0-wayland-gbm.so` | 55 B @ `0x243819d` | 18 | **VFP registers** |
| `/usr/lib32/libz.so.1` | 55 B @ `0x1710a` | 18 | **VFP registers** |

RR3 侧（主机静态解析，设备上没有 guest `.so`，见 §8）：

| guest | 段 | 属性条数 | `Tag_ABI_VFP_args` |
| --- | --- | --- | --- |
| `libRealRacing3.so` | 56 B | 17 | **缺失** |
| `libfmodex.so` | 51 B | 16 | **缺失** |

结论（测得，不是推断）：guest 走 base AAPCS 的标量浮点约定（Android `armeabi-v7a` = `-mfloat-abi=softfp`，但仍发 VFP 指令），而本机所有 32 位提供方库都是 Linux ARMHF。**凡是按值传/返 float/double 的跨界调用都必须经过 softfp 桥**；只传地址的（如 `glUniform4fv`）可直接绑。框架侧对应物：`framework/nxloader/include/nxloader_softfp.h` 的 `nxloader_softfp_add_libm`（libm 边界，`__attribute__((pcs("aapcs")))`），该头文件同时声明 GLES 的浮点入口由端口适配器自己显式注册。同一缺失在其他 Android-guest ARMv7 案例中是已记录契约（`framework/catalog/abi-variants-v1.json:54`，"no Tag_ABI_VFP_args; never inferred"），NFSMW 亦独立测得（`ports_nfsmw/research/abi-contract.md:65`）。

另一条附带实测：guest 声明 `Tag_CPU_arch = 10`（v7），而固件 32 位用户态声明 `14`（v8，按 Cortex-A35 构建）。A35 实现 ARMv8-A 且保留 AArch32 EL0，因此 v7 guest 在其上原生执行没有架构代际障碍——但反向（用 v8 编译的提供方库）不可假设 guest 能利用。

`tools/test_arm_attributes.py` 把上表 libz 与 ld-linux 两条流的"原始字节 → readelf 名称/值"对照固化为可重跑自检（18/18 与 20/20），并验证截断流必须报错而非静默误读；`tools/elf_static_report.py` 的 `parse_arm_attributes()` 即被测对象。工具里仍标为 `tag_27`/`tag_44` 的两条**故意不命名**：设备五个库里没有一条声明这两个编号，binutils 对未知标签只打印数字，所以不能从规范表借名字；它们与浮点约定无关（承载判断的是 28）。

## 5. 显示栈与合成器

- ROCKNIX 桌面侧：实测仅 `emulationstation --log-path /var/log` 在跑；`/tmp/*` 是 overlay 挂载，`/storage/roms` 是 exfat。
- `grep -hoE SDL_VIDEODRIVER /storage/roms/ports/*.sh` → 53 个脚本里 80 处出现，说明**视频驱动由启动脚本显式指定**，不靠默认。
- 设备上已成功的 armhf Android 端口（`Shadowgun.sh`，其头注自称"aarch64 掌机固件，不再依赖 TrimUI 专有路径"）记录了本机显示链条：DRM connector 首选模式 → `fbset geometry` → `fb0/virtual_size` → `DISPLAY_WIDTH/HEIGHT` → 兜底 `1280 720`；present 进程用 `SDL_VIDEODRIVER=wayland`，并**必须保留真实 `XDG_RUNTIME_DIR`**（注释：强行设为 `/tmp` 会让 SDL 找不到 wayland socket → `SDL_Init(VIDEO)` 失败）。同一脚本对 32 位运行时又设 `SDL_VIDEODRIVER=offscreen` + `SDL_VIDEO_GL_DRIVER=libGLESv2.so.2` + `SDL_VIDEO_EGL_DRIVER=libEGL.so.1` + `SDL_OPENGL_ES_DRIVER=1`，即用"离屏渲染 + 另一进程呈现"的两进程结构（`glbridge` + unix socket）。

### 5.1 32 位显示栈实测（本轮直接测量，纠正上一版的推断）

用端口自己的二进制做只加载不初始化的自测（`--sdl-info`），并对 `/usr/lib32` 逐项点名：

- 设备 SDL 运行时 `2.32.10`；我们的二进制按 focal 的 `2.0.10` 头编译，运行期回显 `built against 2.0.10` —— 只前向兼容，不反向。
- **视频驱动实测 6 个：`x11`、`wayland`、`KMSDRM`、`offscreen`、`dummy`、`evdev`**。上一版写"没有 kmsdrm 的 32 位 SDL 路径"是错的，KMSDRM 在 32 位 SDL 里是编译进去的。
- `/usr/lib32` 内 32 位 GL 全家都在：`libEGL.so.1.1.0`、`libEGL_mesa.so.0.0.0`、`libGLESv1_CM.so.1.2.0`、`libGLESv2.so.2.1.0`、`libGLdispatch`、`libGLX_mesa`、`libdrm.so.2.128.0`、`libgbm.so.1.0.0`、`libwayland-client.so.0.23.1`，以及 `libmali.so.1.9.0` 和 `libmali-bifrost-g31-g24p0-wayland-gbm.so`；`/usr/lib32/dri/` 有一批 KMSDRM 驱动 `.so`。
- `/dev/dri` 有 `card0`、`card1`、`renderD128`。
- 32 位 soname 解析默认即可用：`/etc/ld.so.conf.d/arm-lib32.conf` 内容就是 `/usr/lib32`。实测只给 `LD_LIBRARY_PATH=<端口>/libs.armhf` 时 `realracing3 --version` 与 `--sdl-info` 都正常返回。

含义修正：单进程直呈的**库条件已具备**（wayland 与 KMSDRM 两条都在，GL/EGL/blob 齐），剩下的只是"armhf EGL 能否真的对 DSI 面板建出可见 surface"这一条运行时事实，仍未证。

## 6. 端口目录布局与容量

- 合并布局：`/storage/roms/ports` 下 53 个 `.sh` 与同名游戏目录同级（无 `ports_scripts` 分离目录），与用户说明一致。
- PortMaster 在 `/storage/roms/ports/PortMaster`（86 项），`control.txt` 存在；`grep PORT_32BIT /storage/roms/ports/PortMaster/*.txt` 无命中 → `PORT_32BIT=Y` 是端口脚本自用变量，不是 PortMaster 注入的环境。
- 容量：`/storage/roms` 在 `/dev/mmcblk1p3` exfat，`Size 48.9G`，`Available 13.3G` → 2.4 GB 游戏数据放得下（`/storage` 本身 ext4 7.3G，剩 6.3G，不要往那里堆）。
- exfat 挂载带 `fmask=0022,dmask=0022,iocharset=utf8`：目录里确有 GBK 命名的中文启动器（经 `iconv` 会失败：设备 `iconv` 缺 `libiconv.so.2`；改名/新建请用 ASCII）。**执行位实测可用**：在 `/storage/roms/ports/` 下 `chmod +x` 一个脚本后其模式为 `-rwxr-xr-x` 且能被 `sh` 正常执行；已在用的 `ports/nfsmw/nfsmw_runtime` 也是 `-rwxr-xr-x`。所以 exfat 不是"必须把二进制放在 ext4 上"的障碍。
- PortMaster 侧两处与脚本假设不一致的实测：`$controlfolder/tasksetter` **在本机安装里不存在**（端口脚本是 `[ -f ... ] && source` 的软依赖，因此 `TASKSET` 为空、直接 exec）；`control.txt` 里 `clibs="/usr/lib/compat/"`，且 `get_controls()` 用 `export LD_LIBRARY_PATH=${clibs}` **整体覆盖**（不是追加），端口脚本随后才能把 `libs.armhf` 前置。
- `/storage/roms` 是 exfat（`mmcblk1p3`），`/storage` 是 ext4（`mmcblk1p2`），同一张卡两个分区：跨者搬运是真实复制，且 `find /storage -maxdepth 4` 取不到 `/storage/roms/ports/<port>/文件`（层级已是第 5 层），别用浅 maxdepth 判定"数据不在设备上"。
- 设备上没有 `Real Racing 3` 的任何目录或脚本（`ls -d …/*[Rr]eal* …` → `no RR3 directory on device`；逐脚本头注扫描未见 racing/RR3 字样）。

## 7. 设备上已在用的 nxbootstrap 版本

`grep -hoE 'nxbootstrap [0-9.]+' /storage/roms/ports/*.sh` → 0.6.8 / 0.6.21 / 0.6.29 / 0.6.30 / 0.7.7 / **0.7.8** 并存。这些脚本自带完整的失败日志路径（`<game>-launcher-error.$$.log`）、CFW 识别链、PortMaster 根目录探测表、信号 trap 与 `pm_finish` 收尾——是 native-android 端口在本机的现行落地形态，RR3 的启动脚本要与之同构。

## 8. 设备工具链

`readelf` 有（`readelf -A` 已在 §4 用于钉死标签编号），`objdump` 无，`gcc`/`aarch64-linux-gnu-gcc`/`arm-linux-gnueabihf-gcc`/`cmake`/`git` 均无，`python3` 有。→ **编译必须在主机交叉进行，设备只做部署与验证**。guest 自己的 `.so` 目前只在主机侧静态解析（`tools/elf_static_report.py`）；把它部署到设备再跑一次 `readelf -A` 是最终交叉确认，属 BYO 数据流，不入库。

## 9. 尚未测量（不得当作已知）

- 面板物理尺寸/毫米（`width-mm` 未取到），因此"1 逻辑像素 = 多少物理像素"未定；
- `densityDpi`：ROCKNIX 无 Android 显示服务，该值只能由我们按几何契约显式提供给 guest（nfsmw 先例是 160），本机实测值未取；
- 输入设备拓扑（`/dev/input` 事件节点、`SDL_GameControllerDB` 映射）——已实测 SDL 能开到手柄（`retrogame_joypad`），但事件节点级拓扑与逐项按键未做；
- 面板**第二档模式**：本机 DRM 只有 640×480 一档（§2），所以"上报尺寸改变后引擎是否重布局"在本机上不可证。

## 10. 运行时实测（2026-10-08，首次出画面）

> 本节全部测量取自 **Mesa/Panfrost 驱动时期**。同日下午所有者把 32 位图形栈换成 vendor Mali，本节里"活的 GL 上下文提供方 = Mesa""不是 Mali blob"这类结论随之只对那一时期成立；换驱动后的复测见 §11。

| 项 | 实测值 | 出处 |
| --- | --- | --- |
| 32 位 EGL/GLES 经 wayland 实际建面并出帧 | **可行**，已证到像素级 | `grim` 截图：Firemint splash、RR3 标题页、Laguna Seca 3D 赛道（PLAN §10 表） |
| 活的 GL 上下文提供方 | **Mesa/Panfrost，不是 Mali blob**：`VENDOR="Mesa"`、`RENDERER="Mali-G31 MC1 (Panfrost)"`、`VERSION="OpenGL ES 3.1 Mesa 26.0.4"` | `logs/run-4.txt:39-41`、`run-5.txt` |
| 启动器自身的驱动判定 | `GL: Wayland session + Mesa EGL in /usr/lib32; blob tiers skipped` → `no Mali blob; using the device's 32-bit EGL/GLES set` | `logs/run-4.txt:11-13` |
| `/usr/lib32/libmali.so.1` 的真实身份 | **导出** `glMatrixMode`（因此曾被 provider 采纳），但对当前上下文 `glGetString(GL_VERSION)` 返回 NULL ⇒ 它不是这台机器的派发层。教训见 ANALYSIS R8 | `port/thunks/khronos/gles1.cpp:135-172` 注释、PLAN §10.1 |
| GLES1 表 190 个导入的来源 | `190 from the live driver, 0 from libmali.so.1, 0 answered by neither`；`dispatch audit: 0 of 18` | `logs/run-5.txt` |
| 合成器截图通路 | `grim` 可用（`/usr/bin/grim`），需从会话环境取 `WAYLAND_DISPLAY`/`XDG_RUNTIME_DIR`；已封装为 `tools/device_capture.py` | 实测；`fbgrab`/`wf-recorder`/`weston-screenshooter` 不在位 |
| sway 全屏尝试 | **不稳定**：同一二进制不同次运行分别得 `No matching node`（反复重试）与 `Successful, exiting`。**与截图判据的耦合已观察到一次相关**：全屏失败的那次 `grim` 抓到画面（`logs/run-6-console.txt:49-57` ↔ `logs/attach-*.png`），全屏成功的那次 `grim` 抓到整屏黑（`logs/default-cap-*.png` 全 972 B；该次设备端日志未回传本地，故只算一次观察，机制未证，疑为 direct-scanout 绕过合成）。**截图判据须在已知可抓的窗口态下取，整屏黑不得直接当作"无画面"** | 本次实测对照 |
| 内存 | 崩溃前后实测 `MemTotal 981 MB`，退出时 718–721 MB 可用 | `logs/run-4.txt:8`、`run-6.txt` |

## 11. 驱动更换后复测（2026-10-08 下午，vendor libmali）

所有者为便于适配其他系统，把本机 32 位图形栈从 Mesa/Panfrost 换成 vendor Mali blob，并换了地址（地址不入库）。本节只记设备侧事实；对移植主线的结论在 PLAN §11。

| 项 | 实测值 | 出处 |
| --- | --- | --- |
| 32 位 GL 用户态现状 | `/usr/lib32` 里 **同时**存在 `libEGL_mesa.so.0`、`libmali.so.1`、`libmali-bifrost-g31-g24p0-wayland-gbm.so`、`libmali-hook.so`；另有 `/usr/lib32/mali/` 一套分裂 wrapper（未版本化 `libEGL.so` + `libGLESv2.so` 成对，附 `libGLESv1_CM.so`） | `logs/mali-run-3.txt:5-6` |
| 启动器实际选中的文件 | 仍是 tier 0：`Wayland session + Mesa EGL in /usr/lib32; blob tiers skipped` → `/usr/lib32/libEGL.so.1`。**"mesa" 只是那一档的档位名，不是驱动身份** —— 同一个文件名现在答出的是 Mali | `logs/mali-run-3.txt:12-13` |
| 活的 GL 身份 | `VENDOR="ARM"`、`RENDERER="Mali-G31"`、`VERSION="OpenGL ES 3.2 v1.g24p0-00eac0.49796431…"`；blob 自报 `arm_release_ver: g24p0-00eac0, rk_so_ver: 10` | `logs/mali-run-3.txt:22,39-41` |
| GLES 版本变化 | Panfrost 的 **ES 3.1** → vendor blob 的 **ES 3.2**。端口未因此改动任何代码：引擎索取的 190 个 GLES1 导入两边都是 `190/190 from the live driver`，`dispatch audit: 0 of 18` 两边相同 | `logs/mali-run-3.txt:34,38`、`run-5.txt` |
| provider 链在新驱动下的表现 | `GL provider: libmali.so.1 (fixed function present)`，而 GLES1 表 **0 个**指针取自它 —— 因为 190 个已全部由活驱动答出。R8 的修复顺序（先问当前驱动，`dlsym` 只兜底）在"blob 就是驱动"的机器上同样正确，**证实不需要按设备开关** | `logs/mali-run-3.txt:33-34` |
| `grim`（合成器判据） | **在新驱动下不可用**：同一窗口态连抓 4 张全为 972 B 整屏黑，而同期端口自己的回读给出完整画面。**窗口态不是变量**：Mesa 时期能抓到图的 `run-6` 与本次的 `mali-run-3/4` 一样，sway 全屏尝试都是 5 次 `No matching node`、0 次 `Successful` ⇒ 变的只有驱动。§10 里那条"疑为 direct-scanout 绕过合成"的一次性观察，这次成了稳定复现 | `logs/mali-cap-1..4.png` ↔ `logs/mali-cap-b-*.png`、`run-6.txt` ↔ `mali-run-3.txt`（各 5 次失败尝试） |
| 端口自带回读（`glReadPixels` fbo 0） | **两个驱动下都能出图**，从此是本机主判据。启用条件是 `REALRACING3_CONTROL_DIR` —— 此前没有任何一次运行设过它，本轮起由 `tools/device_capture.py --source game` 自动导出；首次实测 `RR3 MCP control ready at …` | `logs/mali-run-3.txt:1764` |
| 整屏黑不等于无画面 | 同一条回读通路在过渡帧给出 `nonblack=0/307200 alpha=307200/307200 max_rgb_sum=0`，而它前后两张分别是亮 splash（`max_rgb_sum=765`）与半暗 splash（`325`）⇒ 全黑是**引擎自己的淡入淡出**，alpha 全 255 也说明缓冲区有效。判黑必须先确认 alpha，再对照前后帧 | `logs/mali-run-3.txt:5898,10866,14630`、`logs/mali-cap-b-1..3.png`、`mali-cap-c-2.png` |
| 内存 | `MemTotal 981 MB`，本次 `MemAvailable 613 MB` | `logs/mali-run-3.txt:8` |
| 启动器措辞修正 | 旧文案 `no Mali blob; using the device's 32-bit EGL/GLES set` 在新驱动下是**假话**（选中的正是 Mali wrapper）。已改为 `no blob tier chosen; …`，并注明驱动身份由加载器打印、不由文件名断定；已在设备上复验 | `port/ports/Real Racing 3.sh:800-810`（`port/package/` 同步）、`logs/mali-run-4.txt:13` |
