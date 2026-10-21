# 60 Parsecs! — 移植分析（阶段 A→F，持续更新）

面向后续移植复用：每条结论都标注了**证据来源**（readelf / 设备 log 行号 / 反汇编地址），
未实测的推断一律写明"未验证"。

## 1. 游戏身份（已实测，勿再猜）

| 项 | 值 | 证据 |
|----|----|------|
| 引擎 | **Unity 6000.0.58f2**（Unity 6） | `META-INF/com.android.games.engine.build_fingerprint` = `6000.0.58f2;IL2CPP;Release;StripEngineCode:1;OptimizedFramePacing:0;AppEntry:1`；`libunity.so` 内串 `6000.0.58f2` |
| 后端 | **IL2CPP**，Release + StripEngineCode | 同上 fingerprint |
| metadata | **version 31** | `global-metadata.dat` 头：magic `4205910959`(0xFAF11BAF) + 次字 `31` |
| ABI | **arm64-v8a only** | `apk/lib/arm64-v8a/`，无 armeabi-v7a |
| 帧节奏 | `OptimizedFramePacing=0` 但 **Swappy 代码仍在 libunity 内并被调用** | 反汇编 `+0x7e0584` 取 `ALooper_forThread`，见 §7.1 |
| 线程模型 | `gfx-threading-mode=4`（分离渲染线程） | `assets/bin/Data/boot.config` |
| 包名/版本 | `com.RobotGentleman.sixtyParsecs.mobile` / 1.3.6 | 阶段 A 读 AndroidManifest（本轮未复核，标记为沿用记录） |

## 2. Native 对象（大小 + NEEDED，本轮 readelf 实测）

| 文件 | 大小 B | NEEDED | 端口是否装载 |
|------|--------|--------|--------------|
| `libmain.so` | 6,696 | liblog, libm, libdl, libc | ✅ Unity 官方 split loader，`JNI_OnLoad` → dlopen libunity |
| `libunity.so` | 21,784,672 | libmain, **libandroid**, liblog, libz, **libEGL**, **libmediandk**, libm, libdl, libc | ✅ |
| `libil2cpp.so` | 63,293,288 | liblog, libm, libdl, libc | ✅ |
| `libfmod.so` | 1,144,520 | liblog, **libstdc++**, libm, libc, libdl | ✅ 游戏自带 FMOD（FMODUnity 集成） |
| `libfmodstudio.so` | 1,054,344 | liblog, **libfmod**, libstdc++, libm, libc, libdl | ✅ |
| `libfmodL.so` / `libfmodstudioL.so` | 1.47M / 2.03M | （低优化 Logging 变体） | ❌ 不装载 |
| `libPDALIFE.so` | 1,087,512 | liblog, libandroid, **libEGL, libGLESv2**, libc, libm, libdl | ❌ 不装载（Robot Gentleman 自家保护/统计层，需 EGL 符号；装载会引入额外契约） |
| `libpdaprotect.so` | 572,776 | liblog, libc, libm, libdl | ❌ 不装载（校验层，同 greak/broforce 对 `libstub` 的处置） |

关键差异（相对 bad2bad / broforce 那批 Unity 2021–2022 案例）：

- `libunity.so` 的 NEEDED **不含 libOpenSLES**（bad2bad 含）。音频改由 FMOD 在运行时
  `dlopen("libaaudio.so")` 拿输出，见 §7.4。**这条直接决定 Unity 6 的音频契约位置。**
- `libunity.so` 的 NEEDED 含 **libmediandk**：媒体面符号必须齐（`media.c` 表），
  否则撞 greak 同款 GLOB_DAT 空槽（bad2bad ANALYSIS §10 已记录该家族坑）。
- Java 侧 Activity 是 `com.unity3d.player.UnityPlayerForActivityOrService`
  （实测 RegisterNatives 行：`nativePause/nativeResume/nativeFocusChanged/
  nativeSoftInputLostFocus` 都挂在它上面），不是老式 `UnityPlayerActivity`。

## 3. 数据布局

```
assets/bin/Data/
  boot.config                    172 B   (gfx-threading-mode=4, androidStartInFullscreen=1)
  data.unity3d             18,246,206 B
  datapack.unity3d         76,175,708 B   <-- 主资源包，体积主要是它
  resources.resource           92,352 B
  Managed/Metadata/global-metadata.dat      (v31)
  ScriptingAssemblies.json / RuntimeInitializeOnLoads.json
```

- 无 `Assembly-CSharp.dll`（IL2CPP AOT），业务逻辑在 `libil2cpp.so` + metadata。
- 设备侧数据目录：`/storage/roms/ports/60parsecs/{lib,assets,bin,datapack...}` +
  可写 `home/`（Unity 用它当 internal-storage 根）。

## 4. 目标设备实测画像（EE4.7 测试机，地址由当次任务提供 —— 与最初 brief 的假设不同）

| 项 | 实测值 |
|----|--------|
| 系统 | EmuELEC 4.7（`PLATFORM=emuelec`），机型标 "NGP45H" |
| 架构 | **aarch64**（不是 armhf） |
| GPU | **Mali-G52**，`OpenGL ES 3.2 v1.r49p0-00eac0`（不是 Mali-450/GLES2） |
| 面板 | **1024×768**，纯 KMS/DRM，无 `/dev/fb0`，无 X11/Wayland |
| EGL | 系统 glvnd 的 `egl_vendor.d` 为空 → 必须前置 `/usr/lib/libmali` 绕过 glvnd |
| 内存 | 1 GB 级：游戏运行 RSS ~753 MB，`avail` 967→975 MB 波动 |
| 布局 | ee47 分裂式：资源 `roms/ports/60parsecs/`，脚本 `roms/ports_scripts/60 Parsecs.sh` |
| 工具 | 设备无 ffmpeg、无 base64、busybox `ps` 无 `etimes` → 取图只能靠 runtime 自身 `glReadPixels` + SFTP |

**为什么这样选**：面板 1024×768 + GLES 3.2 意味着不需要 greak 那套 GLES2 降级，
但 4:3 面板与游戏 16:9 逻辑分辨率的冲突必须在显示契约层处理（§8 待办 #8）。

## 5. 最接近的 NextOS 案例与路线

| 维度 | 复用来源 | 说明 |
|------|----------|------|
| ELF 装载/重定位/JNI 骨架 | `newports/greakmemoriesofazur`、`newports/broforce`、`newports/bad2bad` 同族 `nx_elf + bionic + jni + android + egl_sdl + audio + input` | NextOS 自有 V5 路线，**不用 BENDER `unity_probe`**（该路线已放弃，本仓库仅留 `archive-bender/` 且已 gitignore） |
| Unity 版本 | bad2bad = 2022.3.62f3（metadata v31） | metadata 大版本相同，il2cpp 入口面基本可对照 |
| 显示/输入/内存守护 | greak（`quality.c`、`present_fill.c`、`display_contract.c`、`mem_guard.c`） | **继承项，注释里有未验证声明，需按本游戏复核** |

`port/vendor/{nxinput,nxgl,nxcompat}` 带 `VERSION` 文件，与 broforce 打包方式一致。

## 6. 阶段状态（对齐用户给的 17 阶段）

| # | 阶段 | 状态 | 证据 |
|---|------|------|------|
| 1 | 确认 Unity 版本 | ✅ 6000.0.58f2 / IL2CPP / metadata v31 | §1 |
| 2 | 确认架构 | ✅ arm64-v8a only | §1 |
| 3 | 分析 libmain.so | ✅ split loader，注册 NativeLoader | §2 |
| 4 | 分析 libunity.so | ✅ NEEDED 面 + 391 jmprel / 34288 rela / 456 init_array | 设备 log `[nx] loaded libunity.so` |
| 5 | 分析其他 *.so | ✅ fmod/fmodstudio 装载，PDALIFE/pdaprotect 不装载；L 变体默认不装载，仅 `SF_FMOD_L=1` 时作诊断映射（§7.7） | §2 |
| 6 | 找 Java/JNI 依赖 | ✅ UnityPlayerForActivityOrService 生命周期 + org/fmod + Firebase/GMS 中性化 | log `RegisterNatives ...` |
| 7 | 选最近案例 | ✅ greak/broforce/bad2bad 同族 | §5 |
| 8 | 建立最小 loader | ✅ `60parsecs-nextos` | `port/build.sh` |
| 9 | native 成功加载 | ✅ 5 模块装载，**17 个未解析重定位全部为 weak**，后续已归零 | log `[sf] modules loaded, 17 relocations unresolved (17 weak)`；修 `process_vm_readv` 后 `grep unresolved` = 0 |
| 10 | Unity 初始化 | ✅ initJni / recreateGfx / focus / resume / prefs loaded 8 | log `nativeResume OK`、`[nx] prefs: loaded 8 persistent entries` |
| 11 | 创建窗口 | ✅ KMSDRM 1024×768 + Mali-G52 GLES 3.2 | log `VIDEO: window=1024x768 driver=KMSDRM` |
| 12 | 显示画面 | ✅ **已进入游戏画面**（2026-10-03 run31）：splash（12–13% 非黑）→ 菜单（`rgb_non_black=100.0%`）→ 游戏场景（飞船舱内/终端/地球仪/星空窗，截图 37.3% 非黑） | run31 log `frame probe 1024x768 rgb_non_black=100.0% alpha255=100.0%`；截图 `logs/run31-shot.png` |
| 13 | 输入 | ⚠️ GPTK 通路在跑（`GPTK-INPUT-RECEIPT` 有 native/stick 采样），**未逐项验证游戏内响应** | log `GPTK-INPUT-RECEIPT {"native_button_samples":28,...}` |
| 14 | 音频 | ✅ **闭环**：Studio 初始化已通（§7.7 闭环 + §7.9 bank 预加载），5 个 bank 全部 `loadBankFile -> 0`；AAudio 出声链已闭环（§7.6）；§7.8 杂音 + §7.10/§7.12 断续两轮修复（run32/33/35/37 自动化实证）；**维护者实机听感确认正常（2026-10-03）** | run37 log 连续 7 窗口全部有声（peak 3175~5770）、0 次 QueueAudio 失败 |
| 15 | 存档 | ⏳ `home/` 已建立且被读写，未验证跨启动保留 | log `fopen("/roms/ports/60parsecs/home/Unity/...")` |
| 16 | 性能 | ⏳ 未做（约 31 fps，frame 计数慢于墙钟） | run31 |
| 17 | 可玩 | ⚠️ **解除阻塞**：菜单已建立（`AssignSpriteAtlases` 协程运行、无 `BusNotFoundException`），可玩性验证（输入/存档/性能逐项）未开始 | run31 |

**不跨阶段宣布成功**：#12 的"有画面"已实证，#14 音频已由维护者实机听感确认（2026-10-03），但 #13/#15/#16/#17 均未逐项验证。残留非阻断告警：`SpriteAtlasManager.atlasRequested wasn't listened to while April requested`（角色美术缺失）、`There is no RG.Parsecs.Common.GameManager instantiated on scene`（singleton 时序告警，非致命）。

## 7. 实机根因清单（本轮全部为"实测证据 → 修复 → 复测"闭环）

### 7.1 Swappy 单例为 NULL → SIGSEGV（`addr=0xe8`）
- 现象：UnityMain 起来即段错误。
- 根因：`ALooper_forThread()` 返回 NULL，Swappy 的初始化早退，
  后续 getter 返回未登记单例，调用方对 `NULL+0xe8` 取字节。
- 证据：反汇编 `libunity+0x7e0584` 调 `ALooper_forThread`，成功路径
  `+0x7e05bc str x20,[0x154f000+1160]`，getter `+0x7e06c8` 读同槽。
- 修复：`android.c` 里把"token 假 looper"换成**真 ALooper**（惰性 prepare、
  `pollOnce/pollAll` 由 `poll()` 驱动、wake 管道、addFd/removeFd 语义正确）。
- 复测：开始连续出帧并有 `SDL page flip`。

### 7.2 `pc=0` 跳转（`lr = libunity+0xbba2f4`）
- 根因：`process_vm_readv` 未在导入表里，`nx_elf.c:apply()` 对未解析符号把 PLT 槽写 0
  （这是设计如此：未解析必须诊断而非伪造成功）。
- 修复：`bionic.c` 用裸 `SYS_process_vm_readv` 提供，并补
  `__gnu_strerror_r`、`__assert2`、`tanhf`、`nextafter` 等缺失项。
- 复测：`unresolved` 归零，进程稳定跑到 5000+ 帧不再崩。

### 7.3 `strlen(NULL)` in libc（`libc+0x91d40`，x0=0）
- 根因：`jni.c` 的对象兜底只认**带 `)` 的方法签名**，字段请求（签名就是
  `Ljava/lang/String;`）漏网 → `Build.SOC_MODEL` 答 0 → libunity 对其 `strlen`。
- 修复：兜底同时接受字段描述符；并补 `SOC_MANUFACTURER/SOC_MODEL/SOC_HARDWARE/
  SOC_BOOTLOADER/SOC_RADIO` 字段值。
- 复测：该 SIGSEGV 消失。

### 7.4 splash 后黑屏 = FMOD 输出初始化失败（**当前主线**，根因已两次收窄）
- 最初的因果链（有 log 行号）：
  1. `FMOD failed to initialize the output device.: "Error initializing output device. " (60)`；
  2. `SystemNotInitializedException: [FMOD] FMOD.System.setOutput : ERR_PLUGIN_MISSING`
     自 `FMODUnity.RuntimeManager.GetBus` 抛出，命中
     `RG.Parsecs.Menu.PauseMenu.Awake()` 与 `OptionsManager.InitializeFmodBus()`；
  3. 菜单没建立 → frame probe 由 `12.1%/13.2%` 掉到 `0.0%`。
- 原判断"`libaaudio.so` 的 dlsym 返回 NULL 导致 (60)"**已被实测推翻**：真正原因见 §7.6。
- 现在的阻塞点是 §7.7（`Studio::System::initialize : ERR_INTERNAL`）。

### 7.5 帧栈回溯的教训（工具方法，非游戏缺陷）
- `on_fault` 原来做线性栈扫描，会给出**残留指针**（曾报 `frame 6: libunity.so+0x7f467c`，
  实际该函数无帧指针，第 1 帧链上是 ASCII `"rts/60pa"`）。
- 改为 x29 帧指针链 + 仅在链不足 2 帧时做标注 `stale-prone` 的兜底扫描；
  调用点最终由**寄存器佐证**（`x1 == base+0x7f4690` 对应 `adr x1,7f4690`）确认。

### 7.6 AAudio 门面**签名错误**才是 `(60)` 的根因（已闭环）
- 判据来自 `SF_AATRACE=1` 的逐调用追踪：run16 里 FMOD **只调了
  `AAudio_createStreamBuilder`，一个 setter 都没调**就报 `(60)`。
- 根因：本门面按"返回指针"实现了 `AAudio_createStreamBuilder`。NDK 真签名是
  `aaudio_result_t AAudio_createStreamBuilder(AAudioStreamBuilder **out)`。
  FMOD 把返回的堆地址当错误码读，builder 仍是 NULL，于是直接失败。
- 佐证：`ports/fp2-nextos/upstream/src/aaudio_shim.c`（Unity 6 + FMOD，实机有声）
  同样是出参形式。
- 复测（run17）：整链打通 —— `createStreamBuilder` → 全部 setter →
  `openStream(24000/2ch/float/callback 512, cap 8192)` OK →
  `getFramesPerBurst 400` → `setBufferSizeInFrames 1600` → `isMMapUsed 0` →
  `requestStart` → `waitForStateChange` 立即返回 `STARTED(11)` → 周期轮询
  XRun/buffer → 关闭 `requestStop`→`STOPPED(17)` → `AudioDevice.close (343 blocos
  entregues)`。`(60)` 消失。
- 连带修正的同类契约错误（都是"自造值/自造签名"，非游戏缺陷）：
  - `AAudioStream_isMMapUsed` 必须是**单参**；两参实现让 FMOD 用残值当 `bool*`
    传入，`str wzr,[x19]` 写到自己 magic `0x50464141` → SIGSEGV（run17 崩、run18 修复后消失）。
  - `getTimestamp` 是 4 参 `(stream, clockid, int64* framePos, int64* nanoTime)`；
    伪造的 `AAudioStream_setTimestamp` 已删除。
  - state/错误码/性能模式常量全部改为 NDK `aaudio.h` 真值
    （`OPEN=2 STARTED=11 PAUSED=13 FLUSHED=15 STOPPED=17 DISCONNECTED=20`，
    `ERR_BASE=-900`，`PERFORMANCE 10/11/12`）——FMOD 是拿这些数字比对的。
  - `waitForStateChange` 超时**不得**返回 `AA_OK`，改为 `AA_ERROR_TIMEOUT`。

### 7.7 `Studio::System::initialize : ERR_INTERNAL`（**已闭环**：libfmod 的 JNI_OnLoad 从未运行）
- 游戏除 Unity 内置 FMOD 外，还自带 `libfmod.so`(1,144,520) + `libfmodstudio.so`(1,054,344)，
  由 FMODUnity 的 C# 通过 IL2CPP P/Invoke 驱动。**dlsym 是这条路径唯一入口**，
  因此 `bionic.c:my_dlsym` 把相关名字过给 `sf_audio_fmod_interpose()`（`audio.c`）包装。
- 排查过程（run22–run24）：
  - `FMOD5_System_SetOutput(output=0 AUTODETECT) -> 54`（`ERR_PLUGIN_MISSING`）；
    全量探测得到这颗 libfmod **只接受 2 3 4 5 15 16**；
  - 改钉 `output=2(NOSOUND)` 或 `16` 后 `setOutput -> 0`，`ERR_PLUGIN_MISSING` **归零**；
  - 但 `FMOD_Studio_System_Initialize(...) -> 28`（`ERR_INTERNAL`），
    **与 output 选哪个无关**，失败发生在输出设备之前（无 AAudio 调用、无 bank 打开）；
  - 已排除（有证据）：模块链接错乱（libunity/libil2cpp 的 FMOD 导出数 =0、
    unresolved=0）、输出插件缺失、句柄无效、bank 路径。
- **根因（定论）**：Android 上 FMOD 的 JNI 层（`org/fmod/*`）必须靠 `JNI_OnLoad` 注册，
  而 Android 加载器会自动调 `JNI_OnLoad`，本 loader 不会。libfmod 的 JNI 层半死，
  内部初始化直接 `ERR_INTERNAL`。**修复**：`main.c` 在装载后显式调 libfmod 的
  `JNI_OnLoad` → run30 实测 `FMOD_Studio_System_Initialize -> 0`、`IsValid -> 1`。
- ⚠️ Initialize 通过后黑屏并未解除——真正的"菜单杀手"是第二根因，见 §7.9。

### 7.8 全程杂音 = AAudio float→s16 转换后**写入长度用错**（已修，自动化复测通过，维护者听感确认正常 2026-10-03）
- 位置：`audio.c:aa_submit()`。SDL 设备按 `AUDIO_S16SYS` 打开（`sf_audio_device_init`），
  float 块转换后缓冲区只有 `samples*2` 字节有效，但写队列时传的是 **float 的字节数**
  `bytes = samples*4` → 每块的一半是未初始化内存 → 表现为持续噪声/杂音。
- 修复：`sf_audio_device_write(converted, samples * sizeof(int16_t))`。
- **自动化复测（run32/33，`SF_AATRACE`+`SF_AUDIO_TRACE`，75s/95s）**：
  - 两条输出路径并存：**AAudio 门面流**（24000Hz float，Unity 内置 FMOD 实例）全程
    `pico s16=0` 纯数字静音——本作音频内容全走 FMOD Studio，该实例无内容可播，**属预期**；
  - **AudioTrack 路径**（`org/fmod/AudioDevice`，24000Hz，Studio 初始化后创建的第二会话）
    承载游戏真实音频：`bloco 512 peak=1737`、`1536 peak=4476`、`2048 peak=2677`、
    `3072 peak=4411`（≈-15~-25dBFS，间歇性 SFX/环境声特征）；
  - **全程无满幅随机值** → 修复前"每块一半是未初始化内存"的杂音源已消除；
    真实样本持续流动 → 转换路径健康。
  - ✅ 维护者已在设备上听感确认无杂音（2026-10-03）。

### 7.9 splash 后黑屏（第二根因）= `jar:` 空 sourceDir → bank 全 404 → `BusNotFoundException` 杀协程（**已闭环，run31 实证进入游戏画面**）
- **run30 现象**：§7.7 修复后 `Studio_Initialize -> 0`、`IsValid -> 1`，但菜单依旧不出现：
  - 托管层 `BankLoadException: Could not load bank
    'jar:file://!/assets/Master_Bank.strings.bank' : 404 Not Found`（5 个 bank 全 404）；
  - `StartupConsoleManager+<CompleteInitialization>` 协程内
    `OptionsManager.SetAllSettings → InitializeFmodBus → GetBus("bus:/Master")`
    因 Master Bank 缺失抛 **`BusNotFoundException`**；
  - Unity 协程内未捕获异常 = 协程 **abort** → 菜单永不加载 → `rgb_non_black=0.0%` 黑屏。
- **根因（定论）**：本 loader 下 Unity 的 `Application.streamingAssetsPath` 解析为
  `jar:file://!/assets/...`（`jni.c:asset_pack_dir()` 返回 game.apk 时 sourceDir 为空）；
  loader 没有 `jar:` 协议处理器，Unity 内部 `UnityWebRequest` 对该 URI 直接 404。
  404 发生在**托管层**，C 层 interpose 拦不到 → 只能绕过，不能拦截。
- **修复**（`audio.c`，native 预加载）：`sf_Studio_System_Initialize` 成功返回后调
  `sf_fmod_preload_banks(system)`——经 `nx_find_mod("libfmodstudio.so")` +
  `nx_lookup_in(..., "FMOD_Studio_System_LoadBankFile")` 取真函数，
  `opendir(sf_apk)`（= `$GAMEDIR/assets`）遍历 `*.bank`，
  逐个 `loadbank(system, path, 0, &bank)`（`LOAD_BANK_NORMAL=0` 同步加载）。
  bank 先于托管层就位 → `bus:/Master` 存在 → `GetBus` 不再抛 → 协程走完 → 菜单建立。
- **run31 实证**：
  - `preload-banks: loadBankFile(/roms/ports/60parsecs/assets/Master_Bank.bank) -> 0 bank=0x200000…`
    （5 个 bank 全 `-> 0`）；
  - `grep BusNotFoundException` = 空（消失）；
  - `gl: frame probe 1024x768 rgb_non_black=100.0% alpha255=100.0%`；
  - 截图 frame 3000（`logs/run31-shot.png`）：飞船舱内、电脑终端、地球仪、星空窗
    （37.3% 非黑）。**"先进入游戏画面"目标达成**。
- 构建版本：BuildID `b417822d0356b69c2b8ce2fd423a999deb0ab71a`，
  设备侧 sha256 `20bc35c7…` 校验一致。
- **残留（无害）**：预加载后托管层 `RuntimeManager.LoadBank` 仍会对 `jar:` URI 打一条
  `BankLoadException ... 404`（run32 line 11847，仅此一条）——但 `GetBus` 已能成功
  （bank 在 Studio 系统里先就位），协程不再被杀，菜单正常建立。若要日志干净，
  可让假 JNI 的 `getAssets().open`/AssetManager 把 `android_asset` bank 路径
  重定向到 `$GAMEDIR/assets`，属 P2 打磨。

### 7.10 声音断续 = 两个 FMOD 实例互相关闭共享 SDL 设备（**已闭环，run35 实证**）
- **现象**（维护者实机手动测试 run34）：已有声音但断断续续；日志 **5138 次
  `SDL_QueueAudio falhou: Invalid audio device ID`**（会话边界前 2562 次、后 2576 次，
  持续到进程结束）。
- **根因（定论）**：进程内有**两个独立 FMOD 实例**共享同一个全局 SDL 设备
  （Unity 侧 FMODAudioDevice 会话 + 游戏自带 Studio 系统的 AudioTrack/AAudio 路径）。
  旧实现里 `sf_audio_device_close()` 在每个会话边界（`fmod_pump` 检测 not-running、
  `AudioDevice.close`、`AAudioStream_close`）都**真的销毁 SDL 设备**，
  存活一方的写入随即打到死句柄上 → 丢块 → 断续。
- **修复**（`audio.c`）：设备生命周期提升为**进程级**——
  1. `sf_audio_device_close()` 只结束会话（清计数），不再销毁设备、不清队列；
  2. `sf_audio_device_init()` 规格相同（24000Hz×2）时**复用**现有设备（只更新
     queue_target/取消暂停），规格变更才经 `device_destroy()` 重开；
  3. `device_destroy()`（真销毁）只用于规格变更与 `sf_audio_stop()` 退出。
- **run35 实证**：`AudioDevice.init: reutilizando o dispositivo SDL (24000 Hz x 2,
  frames=512, buffers=4)`（第二次会话复用）、**`SDL_QueueAudio falhou` = 0 次**、
  真实样本持续（peak 1805/1535/3813/4166）、菜单正常（`rgb_non_black=100.0%`）。
  构建版本 BuildID `bd855833e36a463975669f53a06ea52ac9af972a`，设备 sha256 `6779971e…`。
- **内存排查结论（同期）**：设备实际 **2GB**（MemTotal 2011932 kB），无 swap；
  游戏运行时无 OOM 迹象 —— **内存不是断续因素**（此前 §4 记录的 1GB 级说法过时，以
  `/proc/meminfo` 实测为准）。

### 7.11 屏显按键点击位置不符 = 16:9 内容带 vs 4:3 全表面线性映射（**已修，实机验证**）
- **现象**（维护者手动测试）：游戏屏显虚拟按键，点击落点与按键实际位置不符。
- **取证**：设备 `/proc/bus/input/devices` **无触摸屏**（只有 gamepad/udt_keyboard/GPIO
  mouse）——"点击"来自端口右摇杆光标/虚拟 tap，经 `sf_jni_touch_event` 注入表面坐标。
  run31 截图像素分析：内容带 1024×576、上下黑边 ≈96px（游戏自己在 4:3 面板内信箱
  合成 16:9 视图；present_fill policy=screen identity 不干预）。
- **根因**：`input.c` 的 design→surface 映射是全表面线性
  （`y * screen_height / 720`）：设计底边 720 → 表面 768（落在下黑边），且 y 压缩
  比例错误（720→768 而非 720→576）。x 方向不受影响（内容带全宽）。
- **修复**：新增 `design_to_surface()`——y 按内容带缩放并加黑边偏移
  （`content_h = screen_width*720/1280`，`bar = (screen_height-content_h)/2`）；
  右摇杆光标点击与 `tap:X,Y` 虚拟注入两处统一走该函数。16:9 表面下 bar=0，
  映射退化为原线性（安全泛化）。swipe 起点居中（0.5/0.55），本就落在内容带内，不改。
- **实机验证**（run37，SF_GPVIRT+SF_INPUT_DIAG）：`tap:640,360 → DOWN 512,384`
  （中心不变点 ✓）；`tap:640,680 → DOWN 512,640`（新映射 680×576/720+96=640 ✓；
  旧映射会是 725，落在黑边）。BuildID `55b0d6bf`，设备 sha256 `21f82975…`。
- 参考：bad2bad 同族曾遇显示适配问题，修复见提交 `8e91cf1`（SF_ASPECT_SCREEN
  1:1 呈现策略）；本移植已含该策略，本次修复的是其上的**触控坐标层**。
- **§7.11b 补遗（run39/40，新测试机 192.168.31.192）**：维护者复测仍报
  "点击位置不符"。第二轮取证发现 §7.11 只修了**注入侧**——`egl.c:draw_gamepad_cursor`
  的**光标绘制**仍是旧全表面线性映射（`cursor_x * width / 1280`），可见箭头与
  实际落点分家（箭头指按钮、点击偏上，反之亦然）。修复：导出
  `sf_input_design_to_surface()`（sf.h），绘制与注入共用同一变换。
  实证（新机，BuildID `5ed034df`，sha256 `73a00191…`）：
  - 注入侧：`tap:640,680 → DOWN 512,640` ✓（run39/40 均复现）；
  - 绘制侧：`ry+` 把光标推到设计底边后截屏（SF_SCREENSHOT_FRAME=3400），
    像元聚类（奶油填充 RGB≈250,235,204）位于 **x 513-532, y 675-708**，
    与新映射预测（719×576/720+96≈671，填充顶 675）吻合；旧映射会画在
    y≥767（下黑边内、基本裁出屏外）。
  - 注意：中心点（y=360）两映射恰好重合（384=384），验证必须用**非中心**光标位。
- **§7.11c 回退修正（run41/42，结论：§7.11 的信箱理论是过度拟合）**：维护者第三轮
  复测仍报"点击有误差 + 当前界面点击无效"。取证链：
  - log.txt（run41）证实点击**确实到达 UI** 且命中按钮——但命中的是
    `OptionsManager:NextLanguage()`（语言切换）×4、存档按钮 ×1，全非用户瞄准目标；
  - UnityPy 扫 `datapack.unity3d` 找到**语言按钮（Flag）**的 RectTransform：
    锚点为**屏幕比例**（anchorMin [0.946,0.892]、anchorMax [0.988,0.980]）——
    即 UI 相对**全表面**定位，若被信箱压缩则右上角按钮永远点不到（log 佐证：
    用户点到的语言按钮在瞄准点下方）；
  - **定论**：Unity 指针合同 = Screen 像素 = **全表面 1024×768**。
    EventSystem/StandaloneInputModule 直接消费全表面坐标，相机视口（信箱）由
    相机内部换算，Overlay/Screen-Space UI 也按全表面锚定。§7.11 把注入与绘制
    双双压进信箱带（y×0.75+96），造成"指针整体偏低/够不到边缘 UI"。
    首次投诉（.218 机）的真因应是黑边可达性问题，不是坐标压缩。
  - **修复**：删除 `design_to_surface()`，注入与绘制**恢复全表面线性**
    （与 PC 鼠标 1:1 行为一致）；`sf_input_design_to_surface()` 从 sf.h 移除；
    保留 `tap:X,Y` 直通，另加 `rawtap:X,Y`（同义，显式原始表面坐标，校准用）。
    黑边可达性（光标走进黑边）如需处理，后续用**光标钳制**单独解决。
  - **实证**（run42，BuildID `0a59d556`）：`tap:640,680 → DOWN 512,725`
    （= 680×768/720，全线性 ✓）；`ry-` 推光标到顶后截屏，箭头奶油填充位于
    **x 513-532, y 4-37**——内容带映射下不可能低于 96px，全表面可达性 ✓。

### 7.12 声音仍断续（第二根因）= 静音 AAudio 实例挤占共享队列（**已修，run37 实证连续**）
- **现象**：§7.10 设备复用修复后（run36 失败=0），维护者实测**仍断续**。
- **根因**：另一 FMOD 实例的 AAudio 门面流（24000Hz，`pico s16=0` 永远纯静音）
  持续以实时节奏向共享 SDL 队列灌静音块；游戏可闻音频走 AudioTrack 会话。两个
  写入者合计 ~2× 实时速率挤入同一条 FIFO，播放时音频块与静音块交替 → 断续。
- **修复**（`audio.c:aa_submit`）：**全静音块不入队**（float 转换时顺带求峰值、
  I16 路径快速扫描），丢弃后 `aa_pace_drop()` 按块时长 usleep 保持泵节奏
  （否则回调循环会全速空转烧 CPU）。可闻路径（AudioTrack）不受影响。
- **run37 实证**：0 次队列失败；bloco 512→3584 **连续 7 个采样窗口全部非零峰值**
  （3686/4539/3643/5252/3175/5770/5617；此前 run35 为 音频/0 交替）——
  队列里只剩可闻内容，断续消除。✅ 维护者实机听感确认正常（2026-10-03）。

### 7.13 splash 冻结与"点击无效"= 序列只认 KeyCode(Space)，R3→Space 绑定（**已回退：维护者实机判定该按键绑定无效**）

> **最终结论（2026-10-03，维护者判定）**：R3 点击广播 KEYCODE_SPACE 的按键绑定
> **无效**，已从代码回退（input.c 恢复到 130c6ff 之前的状态，R3 点击只注入
> 触摸 DOWN/UP）。本节以下内容保留取证过程作为历史记录；splash 推进问题
> 重新回到待解决状态（见 §8）。

- **现象**：维护者实测"当前界面点击无效，无法进入游戏"。取证发现画面长期冻结在
  37.3% 非黑的 splash 帧（run44/45：145s→310s 四个探针同值；注入 `a`×10、`r3`、
  `rawtap` 全部 accepted 却纹丝不动）。
- **根因**：splash/intro 序列轮询 **Android KeyCode**、完全无视 MotionEvent——
  物理触摸、R3 触摸注入、手柄按钮全都推不动它。逐码注入实证（run46，120s 后
  每 4s 一码）：`key:62`（**Space**）注入后 86 行即出现 74.8% 推进探针；
  Enter(66)/Esc(111)/Back(4)/Tab(61)/106 均无效。仅等待自动播放需 ~4.4 分钟
  （run43 用户会话 264s 处才见 100% 主菜单背景），故体感"卡死"。
- **UI 完整性核查**：splash 各帧（12.1/13.2/37.3/74.7/100%）内容渲染正常，
  无洋红/error-shader 迹象；MenuScene 场景与 OptionsManager 早已加载
  （`Assigned MenuScene`/`RefreshLanguage`/`SetAllSettings`），语言按钮等
  Overlay UI 资源锚点完好——UI 本身完整，卡的是序列推进不是渲染。
- **修复**：`input.c` 的 R3 光标点击（DOWN/UP）现在**同时广播 KEYCODE_SPACE(62)**
  （`sf_jni_key_event`，scancode 用 MAX+2 占位，与 GPVIRT key: 通道一致）。
  本作是触屏驱动游戏，Space 的键盘 source 会把 Unity 输入模式切到触屏——
  这正是期望模式（greak 的 SOURCE_KEYBOARD 铁律在此不构成陷阱）。
- **实证**（run47，BuildID `05117b1f`，sha256 `086d1b6f…`）：120s 处注入
  `r3`×3，画面 74.7%→13.2%→100%→74.7%→100% 连续推进，且出现新资源集
  `Assigned Special`/`Assigned UI`/`Assigned April`（April 恰是此前
  atlasRequested 告警里缺的角色）——序列被点击解锁并开始装载主菜单内容。
- **无人工干预复核（run48/49，2026-10-03，应维护者要求排除 run47 混入手动点击的可能）**：
  - run48（`tmp_r3_nohuman.sh`）：启动后 180s/205s 两次基线探针同为
    37.2–37.3%（证明 SSH 纯注入会话里 splash 确实冻结）；仅注入 `r3`×3 后
    探针出现 13.2%→100.0% 跳变——**无人操作下 R3 注入推进了序列**。
  - run49（`tmp_r3_abshot.sh`，触发式抓帧 A/B/C）：冻结态抓帧 A =
    **60 Parsecs logo splash**；`r3`×3 后抓帧 B = **避难所房间场景**（主菜单
    背景，地球在美洲）；再注入后抓帧 C/D = 同一场景但地球自转到了非洲/欧洲。
    字节级 md5 四帧全不同（`get_abshots.py`）。
  - **教训①指纹撞车**：避难所场景的非黑覆盖率恰好也是 37.2–37.3%，与 logo
    冻结帧完全相同——`frame probe` 的覆盖率指纹**不能**唯一标识画面状态，
    run49 前半段探针"纹丝不动"实为已推进到同覆盖率的新场景。判定画面状态
    必须配截图（loader live 模式：`SF_SCREENSHOT_LIVE=1` 下
    `touch /tmp/bcshot` 即触发抓一帧到 `SF_SCREENSHOT` 路径的 `.NNN` 序号文件）。
  - **教训②点击时序方差**：R3 推进在三次运行中表现不同（run1/48 立即跳变、
    run3/49 探针数值不变但截图证实已推进），覆盖率撞车+淡入淡出过渡让
    "探针是否变化"不可靠，以截图为准。
  - 后续点击停在避难所场景不再推进属预期：该场景等待的是菜单交互
    （虚拟光标+R3 点击按钮），不再是 splash 跳过键。
  - **回退记录**：维护者实机物理验收判定该按键绑定无效（物理 R3 点击不能
    推进/进入游戏），2026-10-03 回退 input.c 的 Space 广播（恢复 130c6ff
    之前行为）。注入通道的"能推进"（run46/48/49，合成事件）与物理按键的
    "不推进"并存，说明合成注入与物理输入在 Unity 输入管线里的路径仍有
    未对齐处（物理 R3 经 SDL 手柄事件→映射→Unity 的链路待另行取证）。

## 8. 未闭环 / 下一步顺序

1. ~~§7.8/§7.10/§7.12 听感确认~~ ✅ **已完成（2026-10-03）**：维护者实机确认声音正常
   （无杂音、不断续）。音频线（#14）闭环。
   注意 libfmod.so 的输出插件串表里只有 `nosound/wavwriter/opensl/audiotrack`
   （**没有任何 aaudio 串**），所以 FMODUnity 那侧最终走
   AudioTrack/OpenSL 的 JNI 路线，与 Unity 内置 FMOD 的 AAudio 路线并存
   （run32/33 实测：AAudio 实例全程静音属预期，本作音频内容全走 Studio→AudioTrack）。
2. **splash 推进重新开题（§7.13 回退后遗留）**：R3→Space 绑定经维护者实机判定
   无效并已回退。遗留矛盾待取证——SSH 合成注入的 Space/r3 能推进 splash
   （run46/48/49），物理 R3 却不能；下一步需沿"物理 R3 → SDL 手柄事件 →
   映射层 → Unity 输入管线"逐段对账，找出物理链路与合成链路的分叉点。
3. **残留告警（非阻断）**：`SpriteAtlasManager.atlasRequested wasn't listened to while
   April requested`（角色 `April` 美术缺失，需要托管层 atlas 回调监听或资源侧补齐）；
   `There is no RG.Parsecs.Common.GameManager instantiated on scene`（singleton 时序告警）。
4. `android/media/AudioManager.getStreamVolume(I)I -> 0`、`STREAM_MUSIC` 常量、
   `Vibrator` 等未应答项：游戏可能据此判定"音量/振动不可用"，但**不是当前阻塞证据**。
5. `executeMainThreadJobs()V` 日志刷屏（JNI 日志噪声，需要限流）。
6. `stat("/roms/ports/60parsecs/g")`：路径被截断，疑似假 JNI 的 `String.getBytes`
   实现缺陷，尚未定性。
7. run19 出现过一次**未复现**的并发 SIGSEGV（两次 `pc=libc+0x8aff0 addr=NULL`，
   addr2line 落在 `jni.c:method_id`，伴随 `call on method id 0x14a with NULL name`）；
   run18/20/22 均未复现，暂记为疑似跨线程 method-id 表瞬态，未定根因。
8. **继承项复核**（任务 #8）：`quality.c:229`、`present_fill.c`（居中 16:9 合成）、
   `display_contract.h` 的宽高比策略 —— 这些注释是 greak 时代未验证声明，
   本游戏是 4:3 面板（1024×768），必须实机复核。
9. **内存监控（P2）**：设备实为 2GB（MemTotal 2011932kB，无 swap），进菜单/关卡后
   RSS 还会涨，`[sf/mem]` 持续观察。
10. 输入逐项验证（#13）、存档跨启动（#15）、性能（#16）逐项验收。
11. 取证埋点回收：`SF_EGLTRACE` 系列、AlertDialog 文本捕获、`[sf/capture]` 每 swap 打点、
   `SF_AATRACE`、`audio.c` 头部继承自其它端口的 "Blasphemous" 注释需订正。
12. 清理：设备侧 `shot.ppm`、`/tmp/bcshot`、`/tmp/parsecs_run.out`；
   `archive-bender/`（本地）与设备 `60parsecs-bender`（459 MB）待维护者决定是否删除。

## 9. 构建与部署（本轮验证过的命令）

```bash
# 交叉编译（Docker portmaster-builder:aarch64-latest）
cd newports/60parsecs/port && bash build.sh
#   门禁输出：GLIBC max: GLIBC_2.29；NEEDED 仅 SDL2/z/dl/m/pthread/libc/ld-linux
#   产物：60parsecs-nextos（ELF 64-bit ARM aarch64）

# 部署（SFTP 原子替换 + sha256 校验；设备无 base64，不能用编码传图）
python tools/put_file.py <abs>/port/60parsecs-nextos /storage/roms/ports/60parsecs 60parsecs-nextos

# 远端启动/取证（paramiko，IP 运行时传入）
python tools/run_remote.py tools/tmp_launch.sh ee47
python tools/run_remote.py tools/tmp_audio_state.sh ee47
```

Windows 侧两个必踩的坑：`python3` 是 Store 别名（不可用，一律 `python`）；
docker/objdump 需 `export MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*'`。

取证开关（env）：`SF_VERBOSE` `SF_JNILOG` `SF_LOGCAT` `SF_EGLTRACE`
`SF_SCREENSHOT=<ppm 路径>` `SF_SCREENSHOT_FRAME=<帧号>` `SF_SCREENSHOT_LIVE`
（配合 `touch /tmp/bcshot` 抓当前帧）、`SF_NO_AUDIO`、`SF_JNI_STRICT_NULL`、
`SF_AATRACE`（逐个 AAudio 调用打点，§7.6 就是靠它定性的）、
`SF_FMOD_L=1`（映射 APK 自带的 `libfmodL.so`/`libfmodstudioL.so` 带日志版本，
soname 不变，仅诊断用）、`SF_FMOD_OUTPUT=<n>`（钉住 FMOD output 编号、跳过探测）、
`SF_FMOD_STUDIO_FLAGS=<v>`（覆盖 `Studio_System_Initialize` 的 flags）。

**日志通道注意**：端口脚本的 `exec > >(tee log.txt)` 持有非 O_APPEND fd，
后台子进程追加写同一文件会被整段覆盖 —— 自己拉起时用独立 `/tmp/parsecs_run.out` 读取。

## 10. 风险与约束

1. **1 GB 内存**：RSS 753 MB / avail 975 MB，`[sf/mem]`+`[sf/gc]` 已在监控
   （heap 114/120 MB）。`datapack.unity3d` 76 MB 是主要纹理/资产来源，
   进关卡后还要涨；OOM 优先于 GPU 怀疑（用户 §十 要求）。
2. **4:3 面板 vs 16:9 游戏**：显示契约继承自 greak，未复核（§8.5）。
3. **无 Google Play 服务**：Play Games 登录、GMS Tasks、Firebase 必须中性化；
   已验证失败方式——返回 NULL 会让托管调用方解引用空并中断整个 Awake()
   （Blasphemous 同款黑屏根因），现策略是"返回声明类型的活对象 + 监听器永不触发"。
4. **PDALIFE / pdaprotect 不装载**：它们需要真实 EGL/GLESv2 与校验链，
   装载会把保护逻辑半仿真化；当前按 greak/broforce 对 `libstub` 的先例跳过。
5. **绝不提交/上传**：APK/OBB、专有 so、资源、存档、设备私有日志、设备地址与凭据。
   `.gitignore` 已覆盖 `apk/ gamedata/ logs/ lib/ assets/ home/ deploy/ *.ppm
   60parsecs-nextos tools/（含 SSH helper）archive-bender/ tmp_*`。
6. `publication/verify.py` 目前会对**其他 port 的本地工作文件**（greak 的 apk/gamedata、
   nxextract 的 release ELF）报 `Unexpected executable`；本轮未修，属既有状态。

## 11. 工程位置

```
newports/60parsecs/
  apk/            维护者提供的 APK 解包物（gitignore）
  gamedata/       待部署到设备的游戏数据（gitignore）
  port/src/       loader 源码（android/bionic/jni/egl_sdl/audio/input/nx_elf/...）
  port/vendor/    nxinput / nxgl / nxcompat（含 VERSION）
  port/build.sh   Docker 交叉编译 + GLIBC/NEEDED 门禁
  port/60 Parsecs.sh  启动脚本（ee47 上位于 roms/ports_scripts/）
  docs/ANALYSIS.md  本文档
```

编译产物 `60parsecs-nextos`、`tools/`（带设备地址的 SSH helper）均为本地产物，不入库。
