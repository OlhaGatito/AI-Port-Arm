# Real Racing 3 — 移植分析（阶段 A/B/C）

> 参考代码根目录（下称 `RR3:`）= `E:/Programs/NextOs-Ports/realracing3-arm-linux`。
> 本文所有 `RR3:` 行号取自该目录 HEAD（2026-09-23 快照），是**证据**，不是已获授权的实机结论。
> 凡参考仓库自述但未被本轮实测复现的条目，一律标注 ⏳待实测。

## 1. 游戏身份

| 项 | 值 | 证据 | 本轮状态 |
|----|----|------|---------|
| 游戏 | Real Racing 3（EA / Firemonkeys，2013/2014 移动赛车） | `RR3:README.md:1-6` | ✅ 无需复核 |
| 目标构建 | **v2.7.0 ROW**（`com.ea.games.r3_row`） | 所有者 `apk/Real_Racing_3_2.7.0.apk` 的 `AndroidManifest.xml`（AAPT1 串池解出 `versionName=2.7.0`、`package=com.ea.games.r3_row`） | ✅ 实测，与 `RR3:README.md:10-14` 记录一致 |
| 主客库 | `lib/armeabi-v7a/libRealRacing3.so`，11,150,536 B，SHA1 `615c9aa4a92faaf9a0f34750e344e5e8a6b9aedf` | 所有者 APK 实测哈希 == `RR3:README.md:12-14` 记录值 | ✅ 逐字节同一构建，不是"同版本的不同包" |
| 音频库 | `libfmodex.so`（FMOD Ex），1,045,372 B，SHA1 `3a5f4eabfb02b0e5e6800073b5c20617bf2d5432` | 所有者 APK 实测 | ✅ ELF32 LE EABI5，静态报告 `logs/fmodex-elf-armv7.json` |
| ABI | **仅 armeabi-v7a**（APK 内另有 `lib/x86`，**无 arm64-v8a**；解出的工作目录 `apk/lib/` 只有 `armeabi-v7a`） | APK zip 清单 `lib/*` 实测 | ✅ 已实测确认：AGENTS 的「含 arm64-v8a 则优先 AArch64」条件**不触发**，ARMv7 是唯一路线（见 §3） |
| 引擎 | 非 Unity / 非 Cocos2d-x：EA 自研 **Blast** 引擎 + 大量 `com/ea/*`、`com/cloudcell/*` JNI 门面 | 静态实测：`DT_NEEDED = libfmodex, libz, liblog, libEGL, libGLESv2, libstdc++, libm, libc, libdl`；PT_INTERP `/system/bin/linker`；36,750 个导出符号、387 个未定入（`Android platform (bionic/NDK)` 147、C++ mangled 34）；无 `Java_*` 导出名 | ✅ 定性由 ELF 实测支撑（`logs/rr3-elf-armv7.json`）；JNI 门面仍需按 `RR3:jni/classes/` 逐个取证 |
| 数据体积 | APK 内 native 库 ~12 MB；**app 数据目录实测 2.45 GB / 11,656 个文件**（赛道、车辆、音频、UI 为首次运行时在线拉取，现已不可下载） | 所有者 `apk/` 目录树实测：`vehicles` 1008.3 MB/4057、`Tracks` 971.1 MB/4235、`assets_2048x1536` 198.9 MB/396、`audio` 81.9 MB/355、`assets_960x640` 55.7 MB/378、`assets_480x320` 16.5 MB/381、`sponsorship` 10.1 MB/181、`Shaders` 5.7 MB/486、`gui` 5.1 MB/557、`gamemodes` 4.4 MB/230、`hud` 2.5 MB/44 | ✅ README 的 ~2.4 GB 自述被复现 |
| 关键校验 | 加载器只校验它要打补丁的**关键区域字节**，不校验整文件哈希 → 分区变体/重签名包可接受，真正不同的构建在崩溃前拒绝 | `RR3:README.md:16-20` | ✅ 机制可复用（`RR3:src/rr3_control.cpp`、`fix_path.h`） |

**结论：本游戏属于 `native-android` 路线**（`PORTING-GUIDE.zh-CN.md §3.4` 第八行），不是 Unity/Mono/Godot 路线。
参考检索用 `python3 tools/find_reference.py --runtime native-android --abi armeabi-v7a`，
同族公开案例见 `docs/zh/NATIVE-ANDROID.md`。

## 2. 参考仓库现状：可复用 / 需改造 / 仅参考

参考仓库是**已可玩**的独立 SDL/Makefile 移植（`RR3:README.md:22-33`：能启动、跑完整 Android 生命周期、编译 116 个着色器、渲染前端与 3D 场景、手柄驾驶、有声音；RK3326 级掌机 15–20 fps）。
它不属于 NextOS 合集树，本次按「迁入 + 契约化改造」路线把它搬进 `newports/realracing3/`。

| 类别 | 文件 | 处置 |
|------|------|------|
| 直接复用 | `RR3:loader/*`（ARM32/ARM64 ELF 加载、重定位）、`RR3:thunks/libc/*`（bionic 导出表，含 `generated/`）、`RR3:thunks/khronos/*`、`RR3:src/symtab_*.cpp`、`RR3:src/*_decompress.cpp`、`RR3:third_party/powervr`、`RR3:third_party/stb` | 迁入 `port/src/`、`port/thunks/`、`port/third_party/`，保留 `LICENSE.md`/`NOTICE.md` |
| 复用但必须改造 | `RR3:android/platform.cpp`、`RR3:src/symtab_glprobe.cpp`、`RR3:src/main.cpp`、`RR3:src/symtab_io.cpp`、`RR3:android/input_bridge.cpp`、`RR3:android/asset_manager.cpp` | 见 §4 的 R1–R5 |
| 仅参考（未被构建链接） | `RR3:jni/classes/ea_DisplayAndroidDelegate.cpp`、`RR3:jni/classes/app_NativeActivity.cpp`、`RR3:android/cursor_draw.cpp`、`RR3:android/emulator_control.cpp` | **不在 `RR3:Makefile:39-66` 的 SRCS 内**：它们存在≠已生效。delegate 是否真被引擎查询必须先取证 |
| 未接入门禁的测试 | `RR3:tests/test_display.cpp` | Makefile / `RR3:harness/verify.sh` / `RR3:package_portmaster.sh` 均不引用 → 不能作为验收依据 |
| 打包层 | `RR3:package/`、`RR3:ports/`、`RR3:package_portmaster.sh`、`RR3:tools/make_release.sh` | 保留 PortMaster 形态，另按 NextOS 补 NXExtract 配方（见 PLAN §6） |

## 3. ABI 决策（四个彼此独立的维度中唯一有强制约束的一个）

`AGENTS.md` 要求「所有者提供的 Android 输入里含 `arm64-v8a` 时优先 AArch64」。
本作的约束相反：参考仓库明确说明**游戏只发 `armeabi-v7a` 原生库**，而 bionic 加载器必须把客机映射进同字长的进程，因此加载器只能编成 32 位 ARM（`RR3:Makefile:3-6`，R36S 跑 aarch64 内核也仍需 armhf 二进制）。

因此：

1. **已实测**：所有者 APK 的 `lib/` 只有 `armeabi-v7a` 与 `x86`，无 `arm64-v8a`（见 §1 表）→ ARMv7 是唯一路线，AGENTS 的 AArch64 优先条款不触发。目标设备侧也已实测具备原生执行条件：`CONFIG_COMPAT=y`、`/usr/lib32` armhf glibc/GLES/EGL/mali 全栈（`docs/DEVICE.md` §3），不需要 box86。
2. **ARMv7 的 softfp/ARMHF 边界已两侧实测**（不再是"可能有"）：guest 的两个库都**不声明** `Tag_ABI_VFP_args`（`libRealRacing3.so` 17 条属性、`libfmodex.so` 16 条，均缺 28），而目标设备上五个 32 位提供方库（`ld-linux-armhf.so.3`、`libc.so.6`、`libSDL2-2.0.so`、`libmali-bifrost-g31-g24p0-wayland-gbm.so`、`libz.so.1`）**全部声明** `Tag_ABI_VFP_args: VFP registers` → guest = base AAPCS 传标量浮点，host = ARMHF 用 VFP 寄存器。凡是按值传/返 `float`/`double` 的跨界调用都要桥：本作的 `glOrtho`/`glFrustum`/`glViewport`/触摸浮点增量、以及 libm 全族都在此列；只传地址的（`glUniform4fv` 等）可直绑。框架侧对应物 `framework/nxloader/include/nxloader_softfp.h`（`nxloader_softfp_add_libm`，`pcs("aapcs")`），该头文件明确 GLES 浮点入口由端口适配器自行注册。编号本身由设备 `readelf -A` 双向钉死并可重跑自检：`tools/test_arm_attributes.py`（libz 18/18、ld-linux 20/20）。**错这一处不会崩溃，会静默变垃圾**（音频发飘、物理异常），是本项目风险 #1。
3. **`DT_NEEDED` 供给面已量化**：`libfmodex.so`（随包客库）、`libz`/`libm`/`libc`/`libdl`（armhf 有）、`libEGL`/`libGLESv2`（armhf 有，但浮点入口需桥）、`liblog`（Android 专有，须 shim 到 stderr/日志文件）、**`libstdc++.so`**（Android 早期 NDK 的小型 C++ 支持库，不是 GNU 完整 libstdc++；须按符号清单逐一决定由谁提供，不得靠"名字相同"直绑）。解析顺序按 `DT_NEEDED`，且 C++ 家族必须钉在对应提供方上，不能用不受限的 host `dlsym`——同一条在 `ports_nfsmw/research/abi-contract.md` 的 540 导入审计里也出现过。
4. 发布 ELF 上限 **GLIBC 2.30**（`PORTING-GUIDE.zh-CN.md` 附录 B），参考仓库自带的 `RR3:tools/check_glibc_floor.sh` 正是干这件事，迁入后接进 `port/build.sh`。注意：目标设备固件是 glibc 2.40（`docs/DEVICE.md` §1），上限低只会更安全，不构成阻断。

## 4. 分辨率自适应：缺陷根因（本轮主战场）

参考仓库**已经有的**：显示检测（`RR3:src/display_config.h:34-41` 读 `SDL_GetCurrentDisplayMode`）、手动覆盖（`<PREFIX>_RESOLUTION`，`display_config.h:11-25`，合法域 160..4096）、以及把逻辑画面等比搬上面板的缩放策略（`RR3:src/symtab_glprobe.cpp:1244-1308`：`fit`/`stretch`/`integer`）。

**但它不是分辨率自适应，而是「固定 640×480 渲染 + 上屏缩放」。五条根因，全部有代码证据：**

### R1 — `android_platform_init()` 是死代码，`ANativeWindow` 恒报 640×480

- `RR3:android/platform.cpp:98-99`：`static int g_width = 640; static int g_height = 480;`
- `RR3:android/platform.cpp:103-111` 定义 `android_platform_init(SDL_Window*, w, h)`，但**全仓 grep 只有定义、没有任何调用点**（只有 `android_platform_window/queue/pump/...` 被内部引用）。
- 后果：`ANativeWindow_getWidth/getHeight`（`platform.cpp:135-145`）永远返回 640×480；`ANativeWindow_setBuffersGeometry`（`platform.cpp:147-158`）被**显式忽略**，注释就写着表面「fixed at the panel's 640x480」；该文件里的触摸换算（`platform.cpp:810-818`）也按这个陈旧值缩放。

**但本轮静态实测把这条从"第一因"降级为"端口自身的陈旧常量"**：客机既**不导入**也**不引用** `ANativeWindow_*` —— 387 个未定符号里 Android 家族只有 `eglGetDisplay`/`eglGetProcAddress`/`eglQueryString` + 142 个 `gl*`（`logs/rr3-elf-armv7.json` 的 `android_import_families`），全文件字符串扫描 `ANativeWindow|AInputQueue|AConfiguration|AAssetManager|AChoreographer|eglQuerySurface|__system_property` **零命中**（`android_strings_not_imported == []`，连"dlsym 按名字取"的路径都没有，客机也没有 `dlopen/dlsym` 导入）。所以 `platform.cpp` 的 640×480 不可能被引擎读到，它影响的是**端口自己**的覆盖层/光标/触摸换算（`platform.cpp:454-456, 509, 539-548, 810-818`）与那份没人调用provider 表（`RR3:src/symtab.cpp:71-72`）。

引擎真正的尺寸入口是 **`Java_com_firemint_realracing3_MainActivity_onViewChangedJNI(env, obj, width, height, landscape, rotation)`**（客机导出符号，端口在 `RR3:src/main.cpp:440-441, 463-464` 按名查找并调用），见 R3。客机同样**不创建 EGL 表面**（无 `eglCreateWindowSurface/SwapBuffers/MakeCurrent/Initialize/ChooseConfig` 导入或字符串），EGL 引导完全由端口代持 —— 这也解释了为什么 `RR3:android/egl_shim.cpp` 实现了二十多个 EGL 入口而客机只导入三个。

**这条修正有直接设计后果**：`PLAN` 里六个只读入口必须挂在 `onViewChangedJNI` 发布链与 GL 重定位别名上，把 `ANativeWindow` 口径当成"端口自身一致性"问题处理，而不是自适应的开关。留一个未闭环项：引擎理论可以通过 `eglGetProcAddress` 拿到 `eglQuerySurface`，但该字符串在客机里不存在，故仅在运行时 trace 里才能彻底排除（§9 第 3 项）。

### R1b — `ANativeWindow`/`Surface` 相关 shim 与 `egl_shim` 的其余入口当前无客机调用方

`RR3:android/egl_shim.cpp:202, 293, 345` 与 `RR3:src/symtab.cpp:71-72` 提供的这些入口，按上面的实测结果没有客机引用点。它们不是"错的"，但在改造时不能当作自适应通路来调参 —— 改了不会被引擎看见。同理，`RR3:jni/classes/ea_DisplayAndroidDelegate.cpp`（§2 已证不在 SRCS）即便接上，客机也没有查询 `DisplayMetrics` 的字符串或类描述符痕迹：客机里的 Java 类只有 `Landroid/app/Activity;`、`Landroid/webkit/WebView;`、`Lcom/firemint/realracing3/Font|TextField|TextureInfo;`、`Lcom/firemonkeys/cloudcellapi/…`、`Lcom/popcap/ea2/UltraDelegate;`（实测 `java_class_strings`），没有任何 Display/Metrics/Surface/Window 类。

### R2 — viewport 重映射只命中「恰好等于逻辑全屏的那一个」viewport

`RR3:src/symtab_glprobe.cpp:1310-1334` 的 `is_fullscreen_viewport()` 要求 `x==0 && y==0 && w==g_log_w && h==g_log_h` 才改写，且 `g_log_w/g_log_h` 默认 640/480（`symtab_glprobe.cpp:1249`，取自 `REALRACING3_SCREEN_W/H`）。设计意图很谨慎（只改那一个全屏调用，其余 render-target viewport 原样放过，避免盲改破坏 FBO），但它同时说明：**引擎侧的渲染分辨率从头到尾就是 640×480，端口只动上屏矩形**。文件头注释（`symtab_glprobe.cpp:1225-1242`）自述了这一点，并记录了 RG34XXSP（720×480）上「640×480 落在角落、其余留黑」的原始症状。

### R3 — 尺寸有两个口径并存，且各消费者取用不一

- `RR3:src/main.cpp:113-114`：`static int kWidth = 640; static int kHeight = 480;`
- 建窗**之前**用 `display_config::detect()`（`main.cpp:350`）覆盖成显示模式，建窗**之后**又用 `display_config::drawable()` 以 `SDL_GL_GetDrawableSize` 再覆盖一次（`main.cpp:400`），紧接着 `viewport_scale_init(drawable_w, drawable_h)`（`main.cpp:401`）。
- 然后三个消费者各自取用同一对 `kWidth/kHeight`：`android_input_init(mod, env, kWidth, kHeight)`（`main.cpp:424`）、`on_view_changed(..., kWidth, kHeight, kLandscape=2, kRotation=0)`（`main.cpp:115-116, 463-464`）、`rr3_control_after_draw(frames+1, kWidth, kHeight)`（`main.cpp:591`）。

`eglQuerySurface` 倒是返回真实 drawable（`RR3:android/egl_shim.cpp:299-305`），但按 R1 的实测，客机既不导入也不按名字索取它，所以这条口径实际上没有消费者；`ANativeWindow` 口径（640×480）同样没有客机引用点。**真正被引擎读走的只有 JNI `onViewChanged` 的 `kWidth/kHeight`**，而它又会被建窗后的 `drawable()` 覆盖一次 —— 一个变量被两个来源、三类消费者取用，且其中两类的取值彼此不等，这就是 `newports/bad2bad` 踩过的坑的 RR3 版本（`bad2bad/port/src/display_contract.h:1-8` 的注释原话：Unity 保持 Android 逻辑窗口而往 4:3 drawable 里 present，于是出现上下黑边）。契约化的第一步就是把它拆成三个各有唯一来源的口径（见 PLAN 的六个只读入口）。

### R4 — 资产与 DPI 把尺寸烧进了内容侧

- 资产包**恒定重定向到最小桶**：`RR3:src/symtab_io.cpp:447-482` 的 `downscale_asset_pack()`，默认目标 `assets_480x320`（`:452`），仅 `REALRACING3_ASSET_PACK` 可覆盖（`:443-445`）；可用桶为 480x320 / 960x640 / 2048x1536（`:428-429`，注意是 **3:2** 不是 4:3）。
- **三个桶在所有者数据里都实测存在**（不再是"参考仓库说存在"）：`apk/assets_480x320` 16.5 MB/381 文件、`apk/assets_960x640` 55.7 MB/378、`apk/assets_2048x1536` 198.9 MB/396 → 合计 271 MB，占 2.45 GB 数据的一成出头；也就是说"按面板选桶"在本轮**不需要额外下载**，缺的只是选择逻辑。桶内文件数近乎相等（381/378/396）说明它们是同一资产集的三套分辨率版本，而非内容差异 → 高桶可用性须逐个资产再证（§9 取证项）。
- 打包链只搬运 480×320，但**抽取配方其实认三个桶**：`RR3:package/realracing3/realracing3.eapx.json:102-104` 列出全部三桶供 NXExtract 解包，而 `:209` 的 staging 只有 `data/assets_480x320`；`RR3:package_portmaster.sh:127` 的匹配模式同样只含 `assets_480x320`，启动器 `RR3:package/Real Racing 3.sh:317` 更是把该桶目录当成硬性前置条件。→ 「真原生」要动的正是这条链：staging 与前置检查得允许按面板选中 960×640 / 2048×1536，或明确记录为何不。
- 开屏画面是 1280×720 而对面板 640×480，端口用**文件替换**而非缩放来救：`RR3:android/asset_manager.cpp:114-121`（`REALRACING3_ASSET_OVERRIDE`）。
- DPI 烧成常数：`RR3:jni/classes/ea_DisplayAndroidDelegate.cpp:33-42` 的 `kDefaultWidth 640`/`kDefaultHeight 480`/`kDefaultDpi = 229.0f`（按 3.5″ 640×480 面板算出），只有 `REALRACING3_SCREEN_DPI` 能覆盖；方向被硬锁（`:93-114`）。**该文件不在 SRCS 里**（§2），所以它当前根本未参与运行 —— 是先接线还是先删除，取决于取证结果。

### R5 — 显示测试没有接入任何门禁

`RR3:tests/test_display.cpp` 存在（`:8` 覆盖多种尺寸），但没有任何构建/CI 目标引用它。按 `AGENTS.md`「未知的必需导入必须产出诊断，绝不能编造成功」的精神，未经门禁的测试等于没有测试。

### R6 — 上报尺寸取自 SDL 显示模式，而本设备的通行后端不给面板真值

- `display_config::detect()`（`RR3:src/display_config.h:34-41`）：`SDL_GetCurrentDisplayMode(0,&mode)` 成功才用 `mode.w/h`，**失败即静默保留传进来的初值**，而初值就是 `main.cpp:113-114` 的 640×480（`int pw = w, ph = h`，`:36`）。整条失败路径不打印任何解释。
- 目标设备上 32 位端口的通行形态是 `SDL_VIDEODRIVER=offscreen`（游戏进程离屏渲染）+ 另一进程 present（`docs/DEVICE.md` §5 的实测样本）。offscreen/dummy 后端的 `SDL_GetCurrentDisplayMode` **不是面板真值**，这与"检测失败"同样危险：它会返回一个看起来合法但无意义的尺寸。
- 同一设备上的同类 armhf 端口正是因此不信任 SDL：`/storage/roms/ports/Shadowgun.sh` 的 `detect_fb_resolution()` 顺序是 DRM connector 首选模式 → `fbset geometry` → `fb0/virtual_size` → `DISPLAY_WIDTH/HEIGHT` → 兜底 1280×720。
- 判据：RR3 的 `screen` 口径必须由 **DRM 实测**提供，SDL 的返回值降为"仅参考"，且每次取值得落进收据日志。这一条要先于 A/B 落地 —— 否则 §9 第 6 项里"只改上报尺寸"这一维会被后端假值污染，得出的否定性结论不可信。

### R7 — 引擎自带一整套分辨率机制，端口现在的做法是在跟它对着干

`logs/guest-resolution-strings.txt`（对 `apk/lib/armeabi-v7a/libRealRacing3.so` 的字符串实测，偏移均为 `.rodata` 内相对偏移）：

| 实测符号 / 格式串 | 位置 | 含义 |
|-------------------|------|------|
| `/assets_%dx%d/` | `0xa06128` | **引擎自己按宽高拼资产桶目录名** —— `assets_480x320` / `assets_960x640` / `assets_2048x1536` 是它选的，不是端口该硬指定的 |
| `asset_packs.cfg` | `0xa06470` | 引擎会打开该文件（所有者数据里实测存在，内容 1 字节 `T`；语义待运行时取证） |
| `dataoffsets.txt` + `"Current working directory doesn't contain dataoffsets.txt"` | `0xa14df0,0xa14e2c` | 资产目录表由 CWD 读取（与 `RR3:src/main.cpp:336-337` 的 `chdir(game_dir)` 一致） |
| `DOff::ResolutionCountEv` / `DOff::GetResolutionDimEii` | `0x0b6c55,0x0b6c71` | **引擎内建"受支持分辨率表"**：条数 + 按索引取宽高 |
| `DOff::GetImageIdOffsetEiNS_27e_supportedImageResolutionsE`、`Asset::GetImageWidth/Height/GetSourceImageWidth/Height/GetSpecificResImage…e_supportedImageResolutionsE` | `0x0b6c8f`、`0x216d65-0x216ede` | 每个资源可按 `e_supportedImageResolutions` 取不同分辨率版本 —— 即 R4 的 `*2X` 家族与三桶的取用逻辑 |
| `mtResolution::setResolutionEjj`、`mtResolution::setClipEiijj`、`mtResolution::screenToResPointTLEiiRiS0_`、`…screenToResVecEiiRiS0_`、`IsClipValidEv` | `0x0c1e26,0x095609,0x0cf6a9,0x0cf6d7,0x1c0929` | **屏幕坐标 ↔ 资源坐标**的双向换算层（自适应命中测试的原生实现） |
| `FrontEnd2::MainMenu/MailScreen/UpgradesScreen/…::OnSwitchResolutionEii`（实测 24 个界面类）+ `GuiImage::OnSwitchResolutionEii` | `0x11153b-0x1bf5dd` | UI 有**分辨率切换回调**，会随尺寸重建布局 |
| `CGlobal::game_getSplitScreenAspectRatioEv` | `0x0c1db1` | 宽高比在引擎内有独立概念 |

所有者数据侧的对应物也已实测：`apk/dataoffsets.txt` 的 `#SEC:res:4` 列出 **`480,320` / `768,576` / `960,640` / `1024,768`** 四档（`python` 读文件核对，非推测），且 `FFT_FONT_*` / `MISC_FONT_*` / `IMG_FONT_CHARS_*` 三族都带 `2X` 变体（`*150` 变体也存在）。四档全是 3:2 或 4:3，**没有一档是 16:9** —— 这是 R4 桶选择与 §9 第 7 项「已验证宽高比区间」的直接证据，比参考仓库 `symtab_io.cpp:428-429` 的桶名清单更硬。

**结论与后果（改写 R4 的做法）**：
- 端口现在把资产恒定重定向到 `assets_480x320`（`RR3:src/symtab_io.cpp:447-482`）是在**覆盖引擎自己的选择**。真原生路线下，这件事应当交还给引擎：喂给它真实尺寸，让它用 `/assets_%dx%d/` 拼目录，端口只在桶缺失时给诊断。
- 「交还给引擎」这条路已有一次**同机制的运行时实证**（换的是驱动、不是尺寸）：同一产物在 Mesa 下收到 `ETC1 2651 + DXT1 2202 + DXT5 29` 次压缩上传，在 vendor Mali 下变成 `ETC1 4775 / S3TC 0`，两边 `dropped=0`，端口自己的 DXT 兜底通路在后者一次都没被进入（`PLAN §11` 结论四）。⇒ 引擎确实按驱动 advertised 的能力换资产家族，且换得干净；尺寸桶由 `/assets_%dx%d/` 承担，属同一类"引擎自己会选"的机制，因此把真实尺寸喂回去比硬指定桶更可能成立。
- `OnSwitchResolution` 的存在，把 PLAN 风险项 3（「引擎可能在 init 时缓存尺寸」）从"未知"降为"有专用切换通路"；但**谁调用它**（是 `onViewChangedJNI` 触发，还是只在游戏内选项改变时触发）仍是运行时问题，须由 §9 第 3 项的 `LOADER_TRACE` 证实。
- 仍未证明：`DOff::ResolutionCount/GetResolutionDim` 的尺寸入参来自哪一路（JNI DisplayMetrics / `onViewChanged` 实参 / 自建 GL 查询）。R1b 已排除 DisplayMetrics 与 `ANativeWindow`，所以剩下的候选就是 `onViewChangedJNI` 与运行期 `eglGetProcAddress`。

### R8 — GL 派发层与当前上下文不同源（非分辨率类，但它曾独占「出不了画面」这一条）

实机首次崩溃（`SIGSEGV pc=0 / lr=libRealRacing3.so+0x00892084`）的根因与显示契约无关，是**派发型**缺陷：`symtable_gles1` 的 provider 以「导出 `glMatrixMode`」为采纳判据，在本机采纳了 `libmali.so.1`，而窗口上下文由 Mesa/Panfrost 拥有 ⇒ 该 blob 的 `glGetString(GL_VERSION)` 返回 NULL ⇒ 引擎 `mtGLWrapper::Init` 版本解析为 0 ⇒ `InitPointers` 不填 `ptr_gl*` ⇒ `blx 0`。完整因果链与逐条偏移见 **PLAN §10.1**；修复在 `port/thunks/khronos/gles1.cpp:135-172`（GLES1 表先问当前驱动，`dlsym` 只作兜底），修复后 `dispatch audit: 0 of 18`、实机出画面。

对自适应主线的意义有两条：
1. **「符号解析成功」与「这个符号能为当前上下文干活」是两件事**——R5 的门禁思想在派发层同样成立，`dispatch audit` 与「GLES1 表来源计数」就是为此加的常驻判据。
2. 参考仓库在 R36S 上能出画面、本机不能，差别就在**谁的上下文**：这类差异不能靠写死 provider 顺序解决，只能靠「问当前驱动」。任何后续按设备改 GL 供给面的冲动，都应先复跑这条审计。

### 附：输入侧的尺寸耦合

`RR3:android/input_bridge.cpp:187-188` 默认 640×480，但 `android_input_init`（`:1426-1430`）会装入活的 `kWidth/kHeight` —— 这一条本身是自适应的。仍固定的是按特定面板调出来的比例常量：`kRegionSplit = 0.55f`（`:508`）、以 Vita 960×544 分数写的虚拟摇杆基准（`:1352-1360`）、点击 `tap.x * g_width`（`:1799-1800`）。`:465-475` 那段「引擎区域缩放常数 1.5f」的注释引用的是 Mass Effect 符号，**属继承脚手架文档， RR3 采信前须对着反汇编复核**。`RR3:ports/realracing3/realracing3.gptk` 所有键位刻意留空，不含坐标映射。

## 5. 已知限制（参考仓库自述，逐条待本轮复现）

| 项 | 原文/数值 | 出处 | 本轮判定 |
|----|-----------|------|---------|
| 帧率 | RK3326 级 15–20 fps；每帧 ~50 ms 花在 CPU 模拟（4×Cortex-A35），GPU 侧 128–250 draw call / ~43k 三角形压力不大 | `RR3:README.md:26-33`、`gameinfo.xml` 描述 | 移植无法消除；**提高渲染分辨率会进一步加重 GPU/带宽**，见 PLAN §8 风险 |
| 显示适配复核 | 「These source changes need handheld verification.」 | `RR3:README.md:203` | 本轮即以实机认证为目标 |
| 已试分辨率 | 640×480、720×480、720×720、1024×768、1280×720；auto 与手动覆盖 | `RR3:testing_thread.txt:5,7` | 同行明确写着「**No new handheld tests have been performed for this adaptation**」 |
| 采集侧尺寸怪癖 | R36S 的 fb 报 1280×1440（两个 720 半幅） | `RR3:tools/grab_screen.sh:8-10` | 直接影响截图判据，PLAN §7 要先处理 |
| 打包层环境覆盖 | 启动器导出 `REALRACING3_SCALE=fit`（`:35-42`）、`REALRACING3_RESOLUTION=auto` + `resolution.txt` 覆盖（`:50-53`），并打印 PortMaster 的 `DISPLAY_WIDTH/HEIGHT`（`:916`） | `RR3:package/Real Racing 3.sh` | 迁入后与 `screen` 策略的默认值要对齐，否则启动器把新默认改回 `fit` |

## 6. 路线决定（以及为什么）

1. **迁入 + 契约化改造**，不做框架原生 V5 重构。理由：参考实现已经过可玩性证明，其价值在加载器/bionic thunks/JNI 门面；把这些丢掉重写等于把已证明的行为重新变成未证明的。
2. **自适应目标定为「真原生」**：引擎在其**已验证可正确布局的宽高比区间**内按实测面板尺寸 1:1 渲染，present 不走任何放大拉伸；偏离参考比例时收缩 content 并加黑边。等比缩放（现有 `fit/stretch/integer`）废弃为回退项。
   分层口径与判据以 **`E:/Programs/ports_nfsmw`（同族：EA Android ARMv7 原生客库 + bionic/JNI 兼容层，已实机验证，见 §8）** 为主先例；`newports/bad2bad/port/src/display_contract.c` 与 `PORTING-GUIDE.zh-CN.md 附录 D.3` 的 `screen/fill/native` 是 Unity/present-copy-scale 模型，**只作对照**，不直接套用：RR3 的客机是自己发 GL 调用的原生 ELF，不需要在 present 层做 blit 缩放，正确做法是重定位别名 + 默认帧缓冲偏移 + FBO 直通。
3. **一次只改一处边界，保持原生顺序**（`PORTING-GUIDE.zh-CN.md §7.2`：map → 重定位 → 导入 → 保护 → 构造函数 → JNI_OnLoad → create → resume → 帧/音频/输入 → pause → save → destroy）。显示契约属于 create/resume 之前的上下文发布环节，不能靠直接调某个渲染函数绕过。
4. **不进 catalog、不走发布文档对**（本轮决定）。`publication/verify.py` 冻结在 40 仓库 / 44 标题，加条目必须同步计数；newports 工作树不在该门禁与 `languages.json` 语言对之内。
5. 目录用 `newports/realracing3/`（与 60parsecs / ragingloop / beastiebay / bad2bad / doubledragongaiden 一致的已跟踪事实约定：`docs/` + `port/` + `tools/`，payload 子路径由 `.gitignore` 忽略）。注意 `AGENTS.md:10` 的成文规定是 `work/ports/<port-id>/` —— 这条冲突需要维护者显式裁决，我在 PLAN 里把它列为待办而不是自行改写 `AGENTS.md`。

## 8. 先例：`ports_nfsmw` 已实机验证的自适应分辨率方案

`E:/Programs/ports_nfsmw`（NFS Most Wanted，`com.ea.games.nfs13_row` 1.3.128，ARMv7 客库 + Bionic/JNI 兼容层 + softfp 转译 + SDL 输出）与本作**同族**：EA 的 Android ARMv7 原生库、自研加载器、假 JNI。它的自适应分辨率不是理论，是实测：1024×768 的 EmuELEC 4.7-Nexus 上输出 `screen resolution 1024x768`，窗口与 JNI 上报均按原生值，58–72 FPS、`exit_code=0`（`ports_nfsmw/PORTING_STATUS.md`「已完成并通过实机验证」节）。

### 8.1 三层口径（这才是「自适应」的正确分层）

| 口径 | 含义 | 来源 | 消费者 |
|------|------|------|--------|
| `screen` | 面板原生尺寸 | `SDL_GetDisplayBounds(0,…)`，失败保留 640×480 兜底 | 窗口创建（FULLSCREEN）：`platform_probe.c:388-392, 638-650` |
| `content` / view | 引擎**已验证可正确布局**的宽高比区间内的绘制区 | `nfsmw_update_view_geometry()`：区间内 == screen；<4:3 锁 4:3（`view_h = w*3/4`）上下信箱；>16:9 锁 16:9（`view_w = h*16/9`）左右信箱；offset 居中（`platform_probe.c:89-107`） | JNI 上报 + GL 偏移 + 触摸 |
| drawable | SDL/EGL 真实可绘表面 | `gl_get_drawable_size` | 在 `runtime_start` 里二次校正 content（`platform_probe.c:712-714`） |

关键点：**content 在正常比例面板上就等于 drawable，所以是 1:1 原生渲染，全链路没有一次放大拉伸**；偏离参考比例时收缩 content 并加黑边，而不是拉伸画面。这与我原先设想的「缩放 640×480 上屏」是两个方向。

### 8.2 机制（可直接搬到 RR3）

1. **单一读取入口**：`nfsmw_screen_width/height()`、`nfsmw_view_width/height()`、`nfsmw_view_offset_x/y()`（`platform_probe.h:15-20`）——全局只有这里能拿到尺寸。
2. **JNI 上报 content**：`getWidth`/`getHeight` 两处 dispatch（`jni_bridge.c:978-979, 1027-1028`）与 `DisplayMetrics.widthPixels/heightPixels`（`:1560-1561`）全部返回 `nfsmw_view_*`；`densityDpi` 固定 160、`density` 1.0（`:1562`）。注意口径是 **content 而非裸 screen**。
3. **GL 走重定位别名，不做尺寸匹配改写**：`relocation_probe.c:155-160` 把 `glViewport`/`glScissor`/`glBindFramebuffer` 别名到 `nfsmw_gl_*`；`platform_probe.c:584-609` 的 viewport/scissor **只在 `guest_framebuffer == 0`（默认帧缓冲）时叠加 offset，FBO 直通**；`glBindFramebuffer` 仅在 `target == GL_FRAMEBUFFER` 时更新跟踪值（`:604-611`）。比 RR3 现有的「viewport 宽高恰好等于逻辑全屏才改写」（`RR3:src/symtab_glprobe.cpp:1310-1334`）更稳：不依赖猜中引擎发的是哪个尺寸，且天然不误伤 render target。
4. **黑边由运行时绘制**：`runtime_draw_letterbox()`（`platform_probe.c:779-828`）`offset==0` 直接返回（对既有 4:3/16:9 零影响），否则严格保存/恢复帧缓冲绑定、scissor box、clear color、color mask、scissor 使能后 `glClear`。
5. **触摸与绘制同一口径**：备用光标用 guest 逻辑像素 `nfsmw_view_width/height`（`jni_bridge.c:2128-2132`，注释明写「platform renderer adds any letterbox offset when drawing it」），绘制侧再加 offset（`platform_probe.c:844-845`）。RR3 的对应缺陷见 R1（`platform.cpp:810-818` 按陈旧 640×480 换算）。
6. **启动判据日志两行**：`G5-RUNTIME screen resolution %dx%d (from SDL_GetDisplayBounds)`（`:641-644`）与 `G5-RUNTIME PASS persistent GLES drawable=%dx%d content=%dx%d+%d+%d`（`:732-736`）。RR3 的 receipt 应产出等价两行。
7. **启动器**：`SDL_VIDEODRIVER` 默认 `kmsdrm`、`SDL_VIDEO_GL_DRIVER=libGLESv2.so`、`SDL_VIDEO_EGL_DRIVER=libEGL.so`，Mali blob 按**三个候选名 pattern 探测**而非认单个 SoC（`portmaster/Need for Speed Most Wanted.sh:47-66`）。

### 8.3 必须带进 RR3 的否定性结论（比机制更重要）

`ports_nfsmw/KNOWN_ISSUES.md`「手柄导航高亮框在非 16:9 屏幕上整体偏移」一节记录了**上报口径失效**的实测：

- 1024×768 上高亮框垂直整体上移 127–128 px（≈ 768 − 640），水平 3 px 内对齐 ⇒ 是缺一段垂直偏移，不是缩放问题。
- 运行时上报值**逐项排除**：`G5-DISPLAY widthPixels=1024 heightPixels=768 densityDpi=160 density=1.000`，客端也自报读到 `ScreenWidthPixels=1024 / ScreenHeightPixels=768 / Aspect=1333 / ContentScale(derived)=1`，同屏其余元素逐像素正常。
- **决定性实验**：把上报屏高改成 640，客端确实读到 `ScreenHeightPixels=640 / Aspect=1600`，但界面布局**逐像素不变**（帧间差分只有 302 px，且集中在车身动态高光区）。⇒ 「客端布局并不取用我方上报的尺寸口径，**唯一杠杆不存在**」。
- 诊断旋钮 `NFSMW_VIEW_SIZE` / `NFSMW_DENSITY_DPI` 因此被回退移除。
- 剩下的两条路：改客端布局数据（`layouts.sb` 结构化二进制，成本高），或**让客端真的跑在设计尺寸上（画面留黑边）**。

对 RR3 的直接含义：D2 选定的「引擎按面板分辨率渲染」**不能靠上报口径自动成立**。RR3 的引擎可能同样把设计尺寸/参考宽高比烧在自己的 UI 布局里（`RR3:` 参考仓库的资产桶就是 480x320 / 960x640 / 2048x1536 这几个离散桶，见 R4），改上报值也许只换来 3D 视口变化而 UI 层错位。**必须先做 A/B 帧差取证，再决定 content 取哪个值。**

另一条可复用的取证手段：`ports_nfsmw/tools/diff_frames.py`（两帧差分 → 差异像素数 + 包围盒 + 16×16 热点网格，专用于回答「改了某个上报参数画面到底有没有动、动了哪一块」），配 `measure_panel.py`、`crop_zoom.py`、`measure_pair.py`、`annotate_offset.py`。RR3 的 `tools/` 应带同一类判据脚本，而不是靠肉眼看截图。

## 9. 取证动作（写代码之前必须落地的观测）

1. ~~`tools/inventory_apk.py` 定 ABI、库清单~~ → **已完成（主机静态实测，见 §1 表）**：APK 内 `lib/` 只有 `armeabi-v7a` + `x86`，无 `arm64-v8a`；两客库 SHA1 实测并与 `RR3:README.md` 记录逐字节相同；数据目录 2.45 GB/11656 文件。
2. ~~`readelf -Ws/-d/-r` 三个客库~~ → **已完成**：设备与主机都没有可用 binutils，改用 `tools/elf_static_report.py`（纯 Python ELF32 解析，报告在 `logs/rr3-elf-armv7.json`、`logs/fmodex-elf-armv7.json`）。实测面：387 个未定符号（Android 家族仅 `eglGetDisplay`/`eglGetProcAddress`/`eglQueryString` + 142 `gl*`；无 `ANativeWindow_*`/`AInputQueue_*`/`AAsset*`/`__system_property*`/`dlopen`/`dlsym`/`Java_*`）；47165 条重定位以 `R_ARM_TARGET2`（46773）为主；606 个 `.init_array` 条目；0 个 V5 预检阻断项。~~仍未闭环的是运行时部分~~ → **运行时已闭环**（`REALRACING3_EGLPROC_TRACE=1`，`logs/run-3.txt`/`run-4.txt`）：客机一共向 `eglGetProcAddress` 索取 **251 个名字，全部是扩展/供应商名**（`glDebugMessageControlKHR…`、`glGenQueriesARB…`、`eglClientWaitSyncKHR`…），**没有一个核心 `gl*` 名**；核心名走的是导入表。所以 310 个 `gl*` 扩展字符串里未被请求的那批不是缺口，而 `Missing:` 导入告警全程 **0 条**。
3. ~~抓 `LOADER_TRACE` 确认引擎实际发了几种 viewport~~ → **已实测（下界）**：`logs/run-4.txt` 的 `GL viewport:` 行显示至少 **5 种**——`640x480@0,0`（全屏）与 `512x512 / 256x256 / 128x128 / 64x64`（方形，引擎自己的渲染目标）。该 trace 上限 24 行（`port/src/symtab_glprobe.cpp:1340`），所以「5 种」是**下界不是清单**。**对 R2 的判定**：非全屏尺寸确实存在，但它们是 FBO 渲染目标，**本来就不该重映射**（`symtab_glprobe.cpp:1310-1316` 的 `is_fullscreen_viewport` 只命中 `x=y=0 且 == 逻辑全屏` 的那一个，行为正确）；R2 的真正缺口不在"漏了别的 viewport"，而在**偏移模型**（present 层没有把 content 摆到面板中央的机制），见 PLAN §2.2。要拿完整清单需把 24 行上限临时放开。
4. ~~在设备日志里 grep `DisplayAndroidDelegate` 是否被查询~~ → **已定性，改为"确认删除"**：R1b 实测客机没有任何 Display/Metrics/Surface 类描述符与 `ANativeWindow` 引用点，delegate 即使接线也无查询方。仍要在首次真机日志里复核引擎是否通过 `onViewChangedJNI` 之外的途径索取尺寸（与第 3、6 项同批取证）。
5. ~~核对所有者 2.4 GB 数据里是否真含 `assets_960x640` / `assets_2048x1536` 桶~~ → **已完成**：三桶全在（16.5/55.7/198.9 MB，381/378/396 文件，见 R4）。**新增待办**：桶内文件是否一一对应（同相对路径集）以及 960×640 桶在 640×480 面板上的实际清晰度收益，须逐项比对后再决定"按面板选桶"能否默认开启。
6. **A/B 帧差判定「上报口径是不是杠杆」**（nfsmw 的否定性结论要求这一步不能省，见 §8.3）：在非 640×480 面板上，只改上报给客机的尺寸（其余不动），抓两帧用 `diff_frames.py` 比对。
   - 差分集中在 UI 层且随上报值移动 ⇒ 客端确实按上报尺寸布局，content 可以取面板值；
   - 差分仅数百像素/无结构变化 ⇒ 复现 nfsmw 的「布局不取用上报口径」，此时**唯一正确做法是把 content 锁在引擎设计尺寸并加黑边**，D2 的「按面板渲染」只能落在 3D 视口上，UI 层不得宣称已自适应。
   - **本轮的额外约束**：唯一被实测确认的引擎尺寸入口是 `onViewChangedJNI(w,h,landscape,rotation)`（`RR3:src/main.cpp:463-464`），所以 A/B 的"只改上报值"必须改这一处的实参；同时要按 R6 先把 `screen` 口径改成 DRM 实测，否则改动会被 SDL 假值淹没。**当前设备面板就是 640×480（`docs/DEVICE.md` §2），此项在本机上无法产生可辨差分** ⇒ 需要另一面板，或改用"上报值 vs 截图内容尺寸"的间接判据，二者都还没做，先记为未验证。
7. 确认 RR3 的**已验证宽高比区间**：**资产侧已实测闭环**（R7）——`dataoffsets.txt` 的 `#SEC:res` 表给出引擎自己的四档 `480x320 / 768x576 / 960x640 / 1024x768`，全部是 3:2 或 4:3，无 16:9。**渲染侧仍待定**：参考仓库试过 640×480(4:3)、720×480(3:2)、720×720(1:1)、1024×768(4:3)、1280×720(16:9)（`RR3:testing_thread.txt:5`）但明确未做掌机复测。区间上下界必须由第 3、6 项实测定出，**不能沿用 nfsmw 的 4:3~16:9**——那是 NFSMW 自己的已验证区间。
8. **新增（本轮实测引出）**：Shaders/`gui`/`hud`/`MenuBannerData.xml`/`MenuTransitions.xml`/`RaceGridBanims.txt` 里是否烧死了分辨率或 UV 常量（486 个着色器文件、557 个 gui 文件、44 个 hud 文件已在本地，可静态扫描），以及 `dataoffsets.txt` 的 `*2X` 字体族是否与资产桶联动 —— 这决定 R4 的"内容侧烧尺寸"到底能吃掉多大收益。
