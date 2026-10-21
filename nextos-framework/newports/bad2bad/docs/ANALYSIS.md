# Bad 2 Bad Delta — 移植分析（阶段 A/B/C）

## 1. 游戏身份（已实测，勿再猜）

| 项 | 值 | 证据 |
|----|----|------|
| 包名 | `com.dawinstone.b2b` | AndroidManifest 字符串 |
| 版本 | **1.6.4** | versionName |
| 引擎 | **Unity 2022.3.62f3** | `META-INF/com.android.games.engine.build_fingerprint`、`libunity.so` 字符串 `2022.3.62f3_9677` |
| 后端 | **IL2CPP** | `global-metadata.dat` magic `AF 1B B1 FA`，version field **31** |
| ABI | **arm64-v8a only** | `lib/arm64-v8a/`，ELF class=2 machine=0xB7 (AArch64) |
| 构建 | DAWINSTONE-SS 2025-12-03 | `assets/build_info.txt` |
| Activity | `com.unity3d.player.UnityPlayerActivity`（标准） | Manifest |
| boot.config | `androidStartInFullscreen=1`，`hdr-display-enabled=0`，`gc-max-time-slice=3` | `assets/bin/Data/boot.config` |

## 2. Native 对象

| 文件 | 大小 | NEEDED |
|------|------|--------|
| `libmain.so` | 6,728 B | libdl, libc, liblog, libm |
| `libunity.so` | 18,437,912 B | libm, libdl, libc, **libandroid, liblog, libz, libEGL, libmediandk, libGLESv2, libvulkan, libOpenSLES** |
| `libil2cpp.so` | 44,991,384 B | libm, libdl, libc, liblog |
| `libFirebaseCppApp-13_4_0.so` | 5,846,784 B | Firebase C++ |
| `libFirebaseCppAnalytics.so` | 67,080 B | Firebase Analytics |
| `libdatastore_shared_counter.so` | 7,112 B | AndroidX DataStore |

`libmain.so` 仅做 NativeLoader：`JNI_OnLoad` 注册 `com/unity3d/player/NativeLoader.load/unload`，按序加载 libunity → libil2cpp。

## 3. 数据布局

```
assets/bin/Data/
  boot.config
  data.unity3d          52,015,005 B
  resources.resource     4,019,168 B
  Managed/Metadata/global-metadata.dat  9,386,440 B
  Managed/Resources/*.dll-resources.dat
  ScriptingAssemblies.json   (IL2CPP 类型清单，含 Assembly-CSharp)
  RuntimeInitializeOnLoads.json
  unity default resources
```

- 无 `Assembly-CSharp.dll` 源程序集（IL2CPP AOT）。
- 业务逻辑在 `libil2cpp.so` + `global-metadata.dat`。
- 有 Firebase / Google Mobile Ads / Play Games / IAP / PlayMaker / Unity Services。
- **无独立 libfmod.so**：FMOD 嵌在 `libunity.so`（`org/fmod` + `fmod_output_opensl`）。

## 4. 目标设备（192.168.137.219，已 SSH）

| 项 | 值 |
|----|----|
| 系统 | ROCKNIX 20260809，kernel 6.12.79 aarch64 |
| 机型 | XiFan **XF35H** |
| 面板 | DSI-1 **640×480** |
| GPU | **Mali-G31**（`libmali-bifrost-g31-g24p0-wayland-gbm.so`）+ Mesa EGL 共存 |
| 内存 | 981 MB（avail ~681），swap 1246 |
| 磁盘 | `/storage/games-external` 48.9G，**仅剩 3.7G（92%）** |
| 家目录 | `/storage`，端口目录 `/storage/roms/ports` |

GPU 策略：局部 `LD_LIBRARY_PATH` 前置 Mali 软链目录 + `SDL_VIDEODRIVER=kmsdrm`（见 handheld-port-test §11.6），**不改系统驱动**。

## 5. 阶段 C — 最接近的 NextOS 案例

**`ports/sallyface-nextos`（Sally Face）** 是唯一同构案例：

| 维度 | Sally Face | Bad 2 Bad Delta |
|------|------------|-----------------|
| Unity | **2022.3.62f3** | **2022.3.62f3** |
| 后端 | IL2CPP | IL2CPP |
| ABI | arm64-v8a only | arm64-v8a only |
| metadata | v31 | v31 |
| Activity | UnityPlayerActivity | UnityPlayerActivity |
| 音频 | FMOD 内嵌 libunity → OpenSL | 同 |
| 目标 GPU | Mali-450 / G31 | Mali-G31 |

Sally Face 上游已有完整 adapter：`nx_elf`（ELF 装载/重定位）、`bionic`、`jni`、`android`、`egl_sdl`、`audio`、`input`、`mem_guard` 等。

**路线**：以 Sally Face loader 为结构基底（GPL-3.0-only，保留许可），改游戏标识与数据路径，在 `newports/bad2bad/port` 独立演进。不照抄 Bender 框架。

## 6. 与 Bender 参考的差异（只作对照）

参考 `F:\Downloads\...\bad_2_bad_delta_1.6.4_apkcube` 证实：

- 同版本同包名，`unity-run` 连续渲染可到菜单。
- 显示契约 640×480 / 240dpi。
- Firebase lazy、FMOD 可选、Vulkan host-map 返回 NULL 逼 GLES 回退。
- 渲染档 640×480 或 75%（480×360）。

不复制其 BENDER_* 代码；仅采纳其**已验证事实**（版本、生命周期、分辨率、回退策略）。

## 7. 移植阶段与当前状态

| # | 阶段 | 状态 |
|---|------|------|
| 1 | Unity 版本 | ✅ 2022.3.62f3 |
| 2 | 架构 | ✅ ARM64 IL2CPP |
| 3 | libmain.so | ✅ NativeLoader |
| 4 | libunity.so | ✅ Android/EGL/GLES/OSL/vulkan/media |
| 5 | 其他 so | ✅ Firebase×2 + datastore（**不加载**，P/Invoke stub） |
| 6 | JNI/Android 依赖 | ✅ UnityPlayer 生命周期 + FMOD + PAD |
| 7 | NextOS 案例 | ✅ Sally Face |
| 8 | 最小 loader | ✅ `bad2bad-nextos` 已上机 |
| 9 | native 加载 | ✅ NativeLoader.load → libunity+libil2cpp |
| 10 | Unity 初始化 | ✅ initJni / recreateGfx / focus / resume |
| 11 | 窗口 | ✅ Wayland 640×480 |
| 12 | 画面 | ✅ frame_proof=OK 非黑 100% |
| 13 | 输入 | ✅ retrogame_joypad + GPTK |
| 14 | 音频 | ✅ FMOD→PulseAudio 24kHz 580 blocks |
| 15 | 存档 | ⏳ home/ 目录已建 |
| 16 | 性能 | ⏳ 先稳定 |
| 17 | 主菜单/可玩 | ✅ **已到主菜单**（见 shot-main-menu.png） |

## 10. 实机关键结论（2026-09-30 Rocknix）

1. **GPU 是 panfrost + Mesa，不是私有 libmali。** `libmali.so.1` 找 `/dev/mali*` 会报 `No mali devices found`。必须用系统 `libEGL.so.1`(glvnd→mesa)。
2. **显示是 Wayland/sway**，socket `/var/run/0-runtime-dir/wayland-1`。SDL2 无 kmsdrm，有 wayland/offscreen。
3. **`/storage` 是 vfat**，不能放 ELF 软链；GL 映射若需要请放 `/tmp`。
4. **Firebase Android so 不能进 ELF mapper**（relocation 段错误）。用 `bionic.c` 里的 `SWIG*`/`Firebase*` 中性 stub，`IntroMain.InitFirebase` 变 no-op，游戏继续。
5. **Intro 卡点是广告/UMP 异步回调**，不是 GPU：
   - `UnityAds.initialize` / `isInitialized` / `load` 要回调 `IUnityAds*Listener`
   - UMP `requestConsentInfoUpdate` 要回调 `onConsentInfoUpdateSuccess`
   - `UnityConsentForm` 构造后要回调 `onConsentFormLoadSuccess`
   - `Handler.post/postDelayed` 必须真正跑 Runnable
6. 内存：RSS ~160MB，avail ~540MB，1GB 机型可接受。
7. 分辨率：window 640×480；游戏画布 640×360 + letterbox（`DISPLAY-FILL`）。
8. 虚拟手柄测试口：`SF_GPVIRT=1` + 写 `/tmp/bad2bad-gp` token（如 `a`）。

## 11. 构建与部署

```bash
# 交叉编译（Docker portmaster-builder:aarch64）
cd newports/bad2bad/port && bash build.sh

# 部署（device_connect.py）
# 启动 env: XDG_RUNTIME_DIR=/var/run/0-runtime-dir WAYLAND_DISPLAY=wayland-1
#           SDL_VIDEODRIVER=wayland LD_LIBRARY_PATH=gamedir/lib:/usr/lib
```

日志：`port/logs/smoke4.log` … `smoke7.log`；截屏：`docs/shot-*.png`。


## 8. 风险与约束

1. **1 GB 内存**：libil2cpp 45MB + data.unity3d 52MB + metadata 9MB，启动即 ~120MB native；需监控 RSS/OOM。
2. **Mali-G31 GLES2/3**：Unity 2022.3 仍有 GLES2 后端；优先 `-force-gles20`。
3. **Firebase/Ads/Play Games**：无 Google Play 服务；必须 stub 成中性成功，不可报错（否则启动卡死）。
4. **PAD / Addressables**：参考记录缺包会让 `Resources.Load` 返回 NULL；base APK 内已融合则按 Sally Face PAD 路径处理。
5. **磁盘 3.7G**：部署前确认体积；避免重复上传整包。
6. **Vulkan**：`libvulkan.so` 需返回 NULL/失败以逼 GLES 回退（参考已验证）。

## 9. 工程位置

- 游戏源与移植代码：`E:\Programs\nextos-framework\newports\bad2bad\`
  - `apk/` — 已解包 APK
  - `port/` — loader 源码与构建
  - `docs/` — 本文档
  - `port/logs/` — 设备日志回传

## 10. media 符号缺口（2026-10-01 对比排查后修复）

与 greak 对比排查发现：本 port 的 `port/src/media.c` 从 `ports/sallyface-nextos` 原样继承，`SF_MEDIA_TABLE` 为 1 条目空表；而 `libunity.so` 的 media 面 UND 有 **61 个**，与 greak 逐符号一致（Unity 2022.3 系 libunity 的 media import 面固定：15 个 `AMEDIAFORMAT_KEY_*` 数据符号 + 46 个 `AMedia*/ANativeWindow*` 函数）。标题帧能跑是因为媒体路径未触发；一旦进入视频/媒体解码流程即踩 greak 同款 `x25=0` 空全局（GLOB_DAT 槽未填）。

修复：移植 greak 修好的 `media.c`（31 个 KEY 数据符号以指针变量地址登记、6 个 `AMediaDataSource_*`、`setDataSourceCustom`、`ANativeWindow_toSurface`、`m_fmt_get_int32` 语义修准），android.c 的 `sf_android_table` 合并逻辑本就存在，接线自动生效。核验 `tools_media_und.py`：61 UND / 缺失 0。Docker aarch64 编译冒烟 `BUILT bad2bad-nextos`，GLIBC_2.29，NEEDED 基线干净。`build.sh` 同时补 `MSYS_NO_PATHCONV` 豁免。

集合先例：`huntdown`（`media_ndk_fail.c`，注释明确记录 KEY 是 OBJECT 不是函数）、`fp2`（`mediandk_shim.c` 定义式）、`bombchicken`/`gunbrick`（表内 `&k_amf_*`）。仍有函数 stub 无 KEY 的 port：hitmango、prizefighters2、skate3、suzycube、sallyface——它们的 libunity 是否引用 KEY 需资产在手时用同款工具复核（已实机验收的 port 大概率 UND 无 KEY 引用，但未断言）。
