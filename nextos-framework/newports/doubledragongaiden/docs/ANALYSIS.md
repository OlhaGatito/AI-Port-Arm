# Double Dragon Gaiden — 移植分析（阶段 A→C，持续更新）

> 本文档记录 2026-10-03 首轮 bring-up 的实测分析。所有结论均来自本地文件实测
> （pyelftools / 字节级读取），未做假设性填充。

## 1. 游戏身份（实测，勿再猜）

| 项 | 值 |
|----|----|
| Android 包名 | `com.intiny.hd1.android`（manifest 实测） |
| 主 Activity | `com.youda.xingzhan.MainActivity`（`unityplayer.UnityActivity` meta-data） |
| 引擎 | **Unity 2021.3.6f1**（`globalgamemanagers` 头部版本串实测；运行时自报 2021.3.36f1，见 run01） |
| 脚本后端 | **IL2CPP**（`libil2cpp.so` 54.5MB + `global-metadata.dat` 6.49MB，sanity=0xfab11baf，**metadata v29**） |
| ABI | `lib/arm64-v8a` + `lib/armeabi-v7a` 双目录；**取 arm64-v8a**（AGENTS.md 规则） |
| 游戏真实身份 | Unity 自报 `Company: yoogames-hd1 / Product: **完蛋！我被美女包围了！**`（FMV 视频驱动游戏；目录名 doubledragongaiden 仅为工程代号）。**LocalVideos 487MB 为核心内容** |
| 厂商配置体系 | **XOR-0x09 混淆**：app.iex / channel.iex / fgui_pkgs.csv / res_version.iex（`RESOURCE_VERSION=320`）及 luapath 的 YoudaFS 容器（=魔数改名+正文异或的 UnityFS） |
| 厂商特征 | 国内手游：xLua 热更（`assets/luapath/*.ab`）、FairyGUI（`assets/android/fgui*`，包清单 UIVideo/UILogin/UIMainHome…）、TapTap SDK（登录+防沉迷）、阿里云 SLS 遥测、cabbage SDK、WebView 活动页 |
| 第三方中间件 | AVPro Video（`libAVProVideo2Native`）、Audio360（Two Big Ears）、Opus/RTMP/samplerate（流媒体配套）、UniTask/DOTween/Newtonsoft（托管侧，见 ScriptingAssemblies.json） |

## 2. Native 对象（pyelftools 实测：NEEDED / JNI 导出 / 未定义符号面）

| 文件 | 大小 | NEEDED | 装载决策 |
|------|------|--------|----------|
| `libmain.so` | 10 KB | liblog, libm, libdl, libc | ✅ 仅 dlopen/dlsym + JNI_OnLoad，标准 Unity split loader |
| `libunity.so` | 25.8 MB | **libandroid, liblog, libz, libEGL**, libm, libdl, libc | ✅ 核心 |
| `libil2cpp.so` | 54.5 MB | liblog, libm, libdl, libc | ✅ 核心 |
| `libxlua.so` | 4.3 MB | libm, libdl, libc | ✅ 游戏逻辑走 Lua，**必须装载**（P/Invoke `xlua`） |
| `libtapsdkcore.so` | 6.0 MB | **libandroid**, liblog, libm, libdl, libc | ⏸ Java 侧驱动；先不装载，按日志决定 |
| `libAVProVideo2Native.so` | 278 KB | liblog, libm, libdl, libc | ⏸ 视频；首跑不装，卡视频时再接 |
| `libAudio360-JNI.so` / `libAudio360.so` | 154 KB / 3.3 MB | **libOpenSLES, libandroid** + AAsset* | ❌ Java 侧驱动（485 个 Java_ 导出），不装载 |
| `libopus.so` / `libopusJNI.so` / `librtmp-jni.so` / `libresample-rh.so` / `libsamplerate-android.so` | — | 各自干净 | ❌ 流媒体/推流配套，Java 侧驱动，不装载 |
| `libsecsdk.so` | 349 KB | libz, **libstdc++** | ❌ 安全 SDK，不装载 |
| `libsls_producer.so` | 146 KB | libz, **libstdc++** | ❌ 阿里云日志，不装载 |

关键观察（相对 60parsecs / Unity 6 案例）：

- `libunity.so` NEEDED **不含 libmediandk、不含 libOpenSLES** → 媒体面比 60parsecs 更小；
  音频运行期 dlopen（推测 `libOpenSLES.so`，以日志为准）。
- **无第三方音频引擎**（60parsecs 的 FMOD 整套不存在）→ 音频走 Unity 自带栈
  （OpenSL/AAudio/AudioTrack，具体以运行期 dlopen 日志为准）。
- libunity.so 非libc 未定义符号仅 **81 个**：ALooper×6、ANativeWindow×6、
  ASensor×14、egl×20、`__android_log*`×3、`__system_property*`×3、
  `android_set_abort_message`，其余为 glibc 均有的 POSIX（syscall/prctl/vasprintf 等）。
- libunity.so Java 导出仅 5 个：Swappy 三件套（`Java_com_google_androidgamesdk_*`）
  + `JNI_OnLoad/OnUnload` → **UnityPlayer 的 native 方法走 RegisterNatives 动态注册**
  （与 60parsecs 相同契约，jni.c 已有该通路）。
- libmain.so `JNI_OnLoad` 是唯一入口，`NativeLoader.load` 链式 dlopen libunity→libil2cpp。
- 三个 bionic `libstdc++.so` 依赖者（secsdk/sls_producer/opusJNI）全部决策不装载 →
  **无需 bionic libstdc++ 桩**。

## 3. 数据布局（实测）

```
assets/bin/Data/
  globalgamemanagers / globalgamemanagers.assets / level0 / sharedassets0.assets
  unity default resources / unity_app_guid
  Managed/Metadata/global-metadata.dat (v29)
  Managed/Resources/mscorlib.dll-resources.dat
  ScriptingAssemblies.json / RuntimeInitializeOnLoads.json
assets/android/            # 游戏自有资源（136MB uiatlas + 47MB font + fgui + sound + ui）
assets/Datas/              # 配置（Language/ app.iex / channel.iex / preload.csv ...）
assets/luapath/            # xLua 脚本 AssetBundle（lua_org_*.ab）
assets/LocalVideos/        # 487MB 剧情/演出视频（AVPro 播放）
assets/LocalSubtitles/
assets/res_version.iex / android_files.csv / supplierconfig.json / zlsioh.dat
```

- **非 data.unity3d 打包**（散装 globalgamemanagers），与 60parsecs 的打包布局不同
  但同属"Unity 相对路径打开"模型：chdir(gamedir) 后 `assets/bin/Data/...` 直读即可。
- 无 StreamingAssets 目录。
- dex 5 个（classes.dex 6.5MB 为主，TapSDK/防沉迷在 classes5.dex）。

## 4. 目标设备画像

沿用 60parsecs §4 实测（同一台测试机，地址由当次任务提供）：
EE4.7 / aarch64 / **Mali-G52 GLES 3.2** / 1024×768 KMS-DRM / 1GB 内存 / ee47 分裂布局。

> brief 中的 "Mali-450 / arm-linux-gnueabihf" 为陈旧模板描述，**与实测设备不符**
> （60parsecs ANALYSIS §4 已证）。本端口按 aarch64 + GLES 3.2 路线执行；
> essl1 降级与 ETC 软解模块保留在骨架里但默认不启用。

## 5. 最接近的 NextOS 案例与路线（阶段C 结论）

| 维度 | 复用来源 | 说明 |
|------|----------|------|
| 主骨架 | **newports/60parsecs/port**（nx_elf+bionic+jni+android+egl_sdl+input+audio+pthread_bridge） | 同设备、同族 loader、最新最成熟（run35 已进游戏+音频闭环）；血统 sallyface-nextos(GPL-3.0-only) |
| 引擎代次 | greak(2022.3.62)/bad2bad(2022.3.62) | 与 2021.3.6 同代相近；initJni 签名以本作 RegisterNatives 日志为准 |
| 硬件降级件 | greak 系 quality/present_fill/display_contract + essl1/etc1/etc2_decode | Mali-G52 上默认旁路，保留备用 |

**路线**：复制 60parsecs port 骨架 → 摘除 FMOD 全家（audio.c/jni.c/main.c/sf.h）→
换游戏身份（gamedir/包名/Activity）→ 补 TapSDK 系良性桩（按日志迭代）→
音频按 Unity 自带栈的 dlopen 证据接（首跑允许静音通过）。

**ports/ 成品案例引擎版本核查（维护者指示，2026-10-03）**：`ports/` 无 Unity 2021.x
成品；最接近的代次是 Unity 2022.3 LTS 族（huntdown 2022.3.47f1、horizonchase/
oceanhorn 2022.3.33f1），其余为 2017/2018/2019。已从该族核实两点 2021-邻近契约：
1. `Build.VERSION.SDK_INT` 回答 0 会让 Unity 2021.3 abort（horizonchase/
   huntdown/oceanhorn 的 `jni_shim.c` 均有记录）——本骨架 jni.c 已回答 API 33，安全。
2. initJni/UnityPlayer 生命周期面与 60parsecs 骨架一致（RegisterNatives 契约）。
结论：60parsecs 骨架（同设备、最新代、ee47 实证）+ 2022.3 案例的 trap 记录，
是当前可用的最优参照组合。

## 6. 风险清单（首跑前已知）

1. **xLua 热更链**：启动期 Lua 虚拟机初始化 + `lua_org_*.ab` 解包/解密失败可能阻塞
   在 splash（对应 60parsecs §7.9 的"协程 abort 黑屏"模式）。xLua 本体依赖干净（仅 libc）。
2. **TapTap 防沉迷**：若游戏强制实名/防沉迷回调，可能卡登录页。桩策略：登录态返回
   "游客已登录"，防沉迷回调 no-op —— 以日志证据逐个补。
3. **AVPro 视频**：487MB 本地视频。若剧情强依赖视频则必须接 MediaCodec/软解桥；
   首跑先跳过（启动视频通常可点跳）。
4. **热更自检**：`res_version.iex` / `android_files.csv` 是厂商热更清单，联网校验失败
   可能弹窗阻断。首跑离线观察。
5. **音频**：Unity 2021.3 的 Android 音频输出（OpenSL/AAudio/AudioTrack）在 ee47 上
   无对应系统栈，audio.c 需按 dlopen 证据实现门面；首跑先 no-op。
6. **1GB 内存**：uiatlas 136MB + font 47MB 全量常驻风险；沿用 [sf/mem] 监控。

## 7. 启动卡点取证（run14-17，2026-10-03 深夜 / 10-04 凌晨）

### 7.1 卡点定位：死锁在 `GameFramework.InitBaseSys()` 协程内

- **长跑实证（run14，10 分钟 soak）**：18300 帧、60fps 正常，**Unity 日志恒 29 行**，
  最后一行永远是 `[frame=1] VideoPlayerMgr！Init`。→ **是死锁/永久等待，不是"慢"**。
- 完整初始化序列（29 行游戏日志，全部发生在 frame=1，time<0.5s）：
  `CoreMain.Init → CoreMain.Init2 → HttpClientMgr！Init → PackageMgr.LoadFullResVerionDb
  (android_files.csv) → Finished → _curPackageType None → LoadResDownloadedList
  bytes==null → m_update_domain(假 CDN) → 版本号四条 → Full3 → Full →
  CabbageSdkMgr！Init → m_app_channel_id/flag/cdn → **VideoPlayerMgr！Init** → 静默`。
- 卡点栈（Unity Debug.Log 自带托管栈）：
  `YCore.DebugLogger:_Log ← GameObject:AddComponent() ← YCore.MonoSingleton`1:get_Ins()
  ← Game.Framework.GameFramework:InitBaseSys() ← Game.Framework.<Start>d__10:MoveNext()
  ← UnityEngine.SetupCoroutine:InvokeMoveNext`。

### 7.2 已排除的假设（均有实证）

| 假设 | 结论 | 证据 |
|------|------|------|
| 只是启动慢 | ❌ | 10 分钟 18300 帧日志零增长 |
| `-force-gles20` 血统注入导致 | ❌ | `SF_NO_FORCE_GLES20=1` 跑 4 分钟同样卡（run15） |
| 等网络（假 CDN / 热更） | ❌ | 假 CDN 访问日志零请求；`/proc/net/tcp` 仅 1 条 ESTABLISHED（SSH 自身） |
| 等 JNI/SDK 回调 | ❌ | `SF_JNILOG=1` 全量：Init 后仅 `NotchScreen()->""`、`hidePreservedContent()`；`runOnUiThread`/`Handler.post` 已 inline 执行 Runnable（jni.c 已实现） |
| AVPro / MediaPlayer 视频通路 | ❌ | 无任何 `libAVProVideo2Native` dlopen，无 `android.media.MediaPlayer` JNI 请求 |
| 引擎工作线程卡死 | ❌ | SIGUSR2 线程转储：38 线程全部 S/R，`Job.Worker*`、`Loading.AsyncRead`、`Loading.Preload`、`UnityGfxDeviceW` 均在 futex(98)/nanosleep(115) 空闲等待，utime 正常 |

### 7.3 新增取证工具（本轮沉淀，可复用）

1. **`SF_TRACE_INVOKE=1` — IL2CPP 方法级插桩**（`port/src/il2cpp_trace.c`）。
   Unity 用 dlsym 绑定脚本后端，故在 bionic 的 `my_dlsym` 出口包裹
   `il2cpp_runtime_invoke`，用运行时反射 API（`il2cpp_method_get_name` /
   `_get_class` / `il2cpp_class_get_name` / `_get_namespace`）打印每次进入托管代码的
   方法全名 + AOT 代码地址（MethodInfo slot 0）。
   - `SF_TRACE_INVOKE_MAX`（预算，防日志爆炸）、`SF_TRACE_INVOKE_SKIP`（噪声过滤，
     默认排除 `UpdateFunction::Invoke` 等引擎 tick —— 实测 45 秒内它独占 10622/19122 行）。
   - **定论**：过滤后卡住期**只有 5 个方法在跑** —— `GameFramework.Update`、
     `UnitySynchronizationContext.ExecuteTasks`、`Application.InvokeOnBeforeRender`、
     `Event.Internal_MakeMasterEventCurrent`、`ScreenOrientationCheck.Update`；
     **无任何 UniTask / AsyncOperation / Video / XLua 托管调用**；
     `<Start>d__10::MoveNext` 全程仅被驱动 **3 次**。
     → 协程停在一个**纯引擎侧等待对象**上（AsyncOperation / awaitable 完成回调），
     该等待不产生任何托管调用。
2. **`SF_DUMP_METHODS=<path>` — 运行时反射导出方法名→代码地址**（同一文件）。
   遍历 domain→assemblies→images→classes→methods，写 `ns.class::method @ addr`
   （按关键字过滤，本轮 12012 条，见 `logs/methods.txt`）。这让任意 C# 方法都能
   在宿主机反汇编，无需手工解析 global-metadata.dat。
   - 关键条目：`GameFramework.InitBaseSys @ libil2cpp+0x105f90c`、
     `GameFramework.Start @ +0x105f5c4`、`VideoPlayerMgr.PlayVideo @ +0x891d4`、
     `UnityVideo.Helper.OnBeforeSplashScreen @ +0x2584730`、
     各 Mgr 的 `Init`（AppVersionMgr / FairyGUIMgr / GameChannelMgr / GameLuaMgr /
     GameResMgr / GameUtilMgr）。
3. **`kill -USR2 <pid>` — 全线程内核态转储**（main.c）：读 `/proc/self/task/*` 的
   comm / stat(state,utime) / syscall，无需栈遍历（托管栈 walker 在 vsync 等待栈上
   必然 fault，见 PLAN §run14-16）。

### 7.4 结构性发现

- **`Game.Framework.VideoPlayerMgr` 类本身没有 `Init` 方法**（只有 `PlayVideo` /
  `_DoPlayVideo` / `GetVideoPlayed` / `SetVideoPlayed` / `.ctor`）→ 日志里的
  "VideoPlayerMgr！Init" 来自**基类 `YCore.MonoSingleton\`1`** 的初始化路径
  （`AddComponent` → `Awake` → `Init`）。
- **`InitBaseSys` 是顺序 `AddComponent` 链**（反汇编 +0x105f90c：连续调用，无 yield
  指令序列）→ 卡点更可能在 VideoPlayerMgr 之后**下一个管理器**（如 `GameResMgr` /
  `GameLuaMgr` / `FairyGUIMgr`），它连 "Init" 日志都来不及打印就挂起。
- 游戏使用 **UniTask（Cysharp.Threading.Tasks）**：trace 捕获
  `PlayerLoopHelper::Init` —— await 依赖 PlayerLoop 注入，是"无托管调用式挂起"的
  典型承载体（待反编译确认）。

### 7.5 下一步（按性价比）

1. **反编译 C# 定位 await 条件（推荐，一次性解决）**：用 Il2CppDumper 生成 dummy DLL
   + dnSpy/ILSpy 反编译 `GameFramework.InitBaseSys`、基类 `MonoSingleton.Init`、
   `GameResMgr.Init` / `GameLuaMgr.Init`，直接读出 yield/await 的是什么。
2. 备选：继续反汇编候选 Mgr 的 `Init`（地址见 `logs/methods.txt`），但需先解决
   libil2cpp 多段映射（部分方法地址落在非 `.text` 区，objdump 取不到指令）。
3. 待反编译确认等待对象后再对症：等 AB/资源 → 补资源加载路径；等 SDK 回调 →
   按 60parsecs `defer_proxy_invoke` 模式补回调；等 UniTask player loop → 校准
   PlayerLoop 注入。
