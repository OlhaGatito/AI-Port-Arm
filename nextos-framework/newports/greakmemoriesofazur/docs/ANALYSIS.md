# Greak: Memories of Azur — 移植分析（媒体崩溃 / 脚本缺失 / CJK 字体）

记录状态：2026-10-01。测试设备当前不在线，本文为本地静态证据链与修复记录；标注「待设备验证」的结论均需实机复核。

## 1. 游戏身份（已实测，勿再猜）

| 项 | 值 | 证据 |
|----|----|------|
| 包名 | `com.navegante.greak` | 启动日志 SystemInfo |
| 版本 | **1.6.4** | 启动日志 |
| 引擎 | **Unity 2022.3.62f2** (7670c08855a9) | 启动日志 Built from '2022.3/respin/2022.3.62f2-4af31df585-v2' |
| 后端 | **IL2CPP**，Stripping Enabled | 启动日志 Scripting Backend |
| ABI | arm64-v8a（另含 armeabi-v7a 目录） | `lib/` |
| 音频 | **FMOD 嵌在 libunity.so**（`fmod_output_opensl`/`audiotrack`），无独立 libfmod.so | 日志 + 本目录说明 |
| 视频 | Unity VideoPlayer + 内嵌 mp4（6 段 splashes/prologue/ending/postcredit） | `apk/assets/*.mp4` |
| 实测设备特征 | ROCKNIX, wayland/sway, **Mali-G52 (Panfrost)**, Mesa 26.0.4 GLES 3.1, 1280x720, 3925MB | `port/logs/greak-text-2026-09-30.log` |

## 2. 问题 A：load-game 崩溃 `libunity+0x8d7484`（x25=0 空全局）

### 2.1 现象

`getTrackFormat` 之后 PC 落在 `libunity+0x8d7484`，`x25=0`。前一轮提交 `8ee35ec` 已用 EOS-fast stub 修好过场视频的 `PC=0`（PLT 未解析），但 load-game 崩溃仍未闭合。

### 2.2 根因链（已闭合）

1. `AMediaExtractor_getTrackFormat` 返回我们的假 fmt 对象后，Unity 立即按 mime 解码器分派：`AMediaFormat_getString(fmt, AMEDIAFORMAT_KEY_MIME, &mime)`。
2. `AMEDIAFORMAT_KEY_*` 在真实 libmediandk 中是 **`const char *` 全局数据符号**（frameworks/av `NdkMediaFormat.cpp` 导出），libunity 通过 **GLOB_DAT 数据槽**引用——不是函数调用，不走 PLT。
3. 修复前的符号表只覆盖函数（38 个 `AMedia*/ANativeWindow*` stub），数据槽未被填充 → 槽值 0。
4. `LDR x25, [GLOB_DAT 槽]` → **x25 = 0（空全局）**，随后解引用路径崩溃于 `libunity+0x8d7484`。

证据：`port/logs/greak-menu-full.log` 行 22–62 命名列出 41 条未解析（10 个 `AMEDIAFORMAT_KEY_*` 数据符号 + 31 个函数，为 `8ee35ec` 之前日志），`unresolved=70`（余 29 为 weak/无名，如 TLS `_ZTH`）。`tools_media_und.py` 扫描 `libunity.so` dynsym：**media 面 UND 共 61**。

### 2.3 修复（本次，media.c）

- 新增 **31 个 `AMEDIAFORMAT_KEY_*` 数据符号**：宏 `SF_MEDIA_KEY(id, val)` 生成 `static const char sf_key_*[] = "..."` + `static const char *const sf_keyp_*`，表内登记 **`&sf_keyp_*`（指针变量的地址）**——与真实库导出形状一致：GLOB_DAT 槽填「指针变量地址」，运行时 `LDR` 得到字符串。字符串值与 AOSP 逐一核对（如 `durationUs`、`rotation-degrees`、`push-blank-buffers-on-shutdown`）。
- 新增 **6 个 `AMediaDataSource_*`**（API 30 自定义流接口）：setter 仅存回调不调用（extractor stub 已全线 EOF，回调不会被触发），为后续真解码留位。
- 新增 `AMediaExtractor_setDataSourceCustom`（返回 AMEDIA_OK）与 `ANativeWindow_toSurface`（返回 window token 自身）。
- `m_fmt_get_int32` 语义修准：`width/stride=1280`、`height/slice-height=720`、`frame-rate=30`、`channel-count=2`、`sample-rate=48000`（此前 height 也误报 1280）。
- 覆盖核验：`tools_media_und.py ../apk/lib/arm64-v8a/libunity.so src/media.c` → **media UND 61，缺失 0**（表 77 条，含 16 条防御性冗余 KEY，无害）。

### 2.4 验证状态

本地：静态符号覆盖核验通过 + 编译冒烟（见 §5）。待设备验证：load-game 路径跑通、过场视频 EOS-fast 不回归。

## 3. 问题 B：`The referenced script on this Behaviour (Game Object '_GroupContainer') is missing!`

**判定：良性警告，不处理。** 依据：

- 出现于 `sharedassets2.resource` 加载后，游戏继续加载后续资源（sharedassets5/level5…），帧循环 `keep=1` 正常推进（`greak-menu-full.log` 行 1070–1095）。
- Unity **Stripping Enabled** 下场景序列化引用的被裁剪 MonoBehaviour 会打此警告，原版 Android 同样打印；IL2CPP AOT 不存在「缺 assembly」问题。
- 加载器侧已有兜底：`nx_safe_il2cpp_invoke` 拦截 `il2cpp_runtime_invoke(NULL)`（缺脚本导致空 MethodInfo），不会 PC=0。

另记录两个同级别的次要异常（暂不处理）：

- `[AmplifyColor] Failed to initialize shaders`（NullReferenceException）：Amplify Color 后处理 shader 初始化失败，可能影响调色效果，待画面对比评估是否需要补 GLES shader 兼容。
- `KeyNotFoundException: 'GUI_InputButtons_Blank'`：GUI 键缺失，输入提示占位渲染问题。

## 4. 问题 C：CJK 字体（LiberationSans 缺字，TMP 无 fallback）

### 4.1 现状

游戏动态字体为 Liberation Sans（无 CJK 字形），内嵌 TextMeshPro 资产未配置 CJK fallback；汉化文本渲染为豆腐块。

### 4.2 方案（本次落地件）

走 Unity Linux player 动态字体的 OS-fallback 通道（fontconfig），零侵入游戏资产：

- `port/fonts/fonts.conf`：`<dir>.</dir>` 相对配置文件目录（可随 gamedir 迁移），`<cachedir>.cache</cachedir>`；`Liberation Sans` 与 `sans-serif` 均 prefer `Noto Sans CJK SC/TC/JP`。
- `main.c` 新增 `install_fontconfig()`：`setup_paths()` 后若 `FONTCONFIG_FILE` 未设且 `<gamedir>/fonts/fonts.conf` 存在，则 `setenv("FONTCONFIG_FILE", ...)`；用户显式设置优先。

### 4.3 部署约定（owner-supplied 字体）

```
<gamedir>/fonts/fonts.conf
<gamedir>/fonts/NotoSansCJKsc-Regular.otf   （所有者自备，不入库）
```

### 4.4 验证状态

待设备验证三件事：fontconfig 生效日志（`[sf] fontconfig: FONTCONFIG_FILE=...`）、CJK 文本不再豆腐块、TMP 场景（若静态图集字体则 OS fallback 不适用，需另评估替换 TMP 资产）。

## 5. 本地验证记录（2026-10-01）

- `tools_media_und.py` 覆盖核验：media UND 61 / 缺失 0（修复前缺失 23）。
- Docker `portmaster-builder:aarch64-latest` 编译冒烟：**BUILT greak-nextos**，GLIBC 上限 `GLIBC_2.29`（≤ 2.30 合规），`DT_NEEDED` 全部落在基线白名单（SDL2 stub / z stub / dl / m / pthread / libc / ld-linux），用时约 2m36s。既有小瑕疵：`pthread_bridge.c`、`quality.c` 源内 `#define _GNU_SOURCE` 与 `-D_GNU_SOURCE` 重定义警告，无害，未处理。
- `build.sh` 增加 `MSYS_NO_PATHCONV` 豁免（Git Bash 下 `-w /src` 被路径转换劫持导致 docker 启动失败）。
- 未做（设备不在线）：load-game 实机复测、CJK 渲染实测、AmplifyColor 画面评估。

## 6. 下一步（设备恢复后）

1. 实机跑 load-game 路径：确认 `libunity+0x8d7484` 不再出现，`unresolved` 计数中命名项归零（剩余应仅为 weak/无名）。
2. 部署 CJK 字体并验证渲染与 TMP 场景。
3. 评估 AmplifyColor 调色缺失对画面的实际影响。
4. 归档新一轮日志至 `port/logs/`（本地目录，gitignored）。

## 11. load 闪退：createDecoderByType 签名错误（2026-10-01 晚修复）

用户实测 load 触发开场视频后闪退。现场：`AMediaExtractor_setDataSourceFd(45)` 正常 → `AMediaCodec_createDecoderByType(` 日志在打印 mime 参数时中断 → SIGSEGV `addr=0x10`，PC 落在 libc 字符串函数（strlen 读 0x10）。

根因：stub 写成了 `(void *name, const char *mime)` 双参数，而 NDK 真实签名 `AMediaCodec_createDecoderByType(const char *mime)` 只有**一个参数**（x0）。stub 从 **x1** 取 mime，寄存器现场 x1=0x10（垃圾值），printf %s 对 0x10 做 strlen 即崩。寄存器转储与地址完全吻合。

修复与顺带加固：
- `m_codec_create` 改为单参数签名。
- `m_codec_dequeue_out` 补填 `AMediaCodecBufferInfo`（此前返回 index 但 info 全垃圾，Unity 靠 `flags=BUFFER_FLAG_END_OF_STREAM` 判定视频结束，不填会死等）。
- `m_ext_new`（真实 0 参）、`m_ext_read_sample`（真实 2 参）按 NDK 签名对齐。
- bad2bad 于 7c8323a 移植的 media.c 带同一签名错误，已同步修复并重建。

部署验证（md5 21704a91）：启动健康，菜单 probe 99.7–100% 非黑，0 SIGSEGV；`createDecoderByType(video/avc)` 路径待用户实测 load/新游戏触发。

## 12. 二次黑屏：EOS 握手不完整（2026-10-01 晚二修）

修复签名后 load 不再闪退（`createDecoderByType(video/avc)` 正常、0 SIGSEGV），但视频段画面持续全黑 + `routine is null` 周期刷屏（`port/logs/greak-device-20261001-black2.log`）。

用户提供了可运行的 BENDER 参考工程（`E:/BaiduNetdiskDownload/roms/port_test/greak_memories_of_azur_v1.2.10_full_apkvision/`，含 `BENDER_VIDEO_FIX75-80.txt` 与 16986 行 `unity_probe.c` 全源）。其 FIX75 记录的正是我们的反模式：**EOS 交付过早**——Unity MoviePlayer 期望的握手为 FORMAT_CHANGED → 等输入 EOS 入队 → 才交付输出 EOS；FIX79 记录 EOS 后 Unity 会 delete→create 循环重开同一视频源，协程永不结束。

移植 BENDER 状态机到 media.c（参考 unity_probe.c:7353-7425）：
- start/flush 置 `format_change_pending`；首次 dequeueOutputBuffer 返回 -2（FORMAT_CHANGED）。
- 合成帧 2 帧（env `SF_VIDEO_SYNTH_FRAMES` 可调），distinct PTS（33333us 步进）、size=1、flags=0；上一帧未 release 前不发下一帧，8 次 poll 停滞则超时强放并触发 listener。
- EOS 仅在 Unity 入队输入 EOS 后交付（24 次 poll 兜底 fallback）；输出 EOS 时 arm `g_media_completed`。
- fix79 重启守卫：setDataSource* 世代自增；同世代完成后 `createDecoderByType(video/*)` 允许一次有界重开、其后返回 NULL。
- releaseOutputBuffer(render) 时派发 `sf_jni_surface_frame_available`（jni.c 早已具备 listener 捕获 j_SurfaceTexture_setListener + 派发函数，media.c 此前从未调用——接线缺口）。
- Greak 的 libunity UND 无 AImageReader/AHardwareBuffer 引用，FIX78 件不需要。
- 全部 codec/extractor stub 加限频调用追踪（TRACE_CAP=24，TRY_AGAIN 按 600 次采样）。

新二进制 md5 a9b958fd 已部署。判定依据（trace 日志）：若黑屏持续但 trace 显示 EOS armed + 后续无 media 调用 → 问题在视频后场景渲染；若 dequeueOutput TRY_AGAIN 持续 → 握手仍卡。

## 13. 三修通过：BufferInfo 布局错误（2026-10-01 深夜，实机验证通过）

### 13.1 现象与弯路

EOS 握手版（2f747a5）部署后仍闪退，日志（crash3）显示握手已生效
（FORMAT_CHANGED → 合成帧 → onFrameAvailable dispatch），但第二个视频
合成帧 2/2 之后 SIGSEGV `libunity.so+0x8d6314`，`addr=(nil)`，
寄存器 x22=1、x24=0、x25=0、x8=0。

第一修（6eeb76a）猜测 `getOutputBuffer` 返回非 NULL dummy 把 Unity 引入
字节缓冲路径（BENDER FIX195），改为返回 NULL。实测（crash4）签名完全
相同 → 该改动不是真凶（保留：与 BENDER 合成路径契约一致）。

### 13.2 真根因：AMediaCodecBufferInfo 字段偏移错误

我们用 `struct { int32_t offset; size_t size; int64_t pts; uint32_t flags; }`
填 dequeueOutputBuffer 的 info。而真实 NDK 布局（BENDER
`BenderMediaCodecBufferInfo`，unity_probe.c:7257）是
`{ int32_t offset; int32_t size; int64_t pts; uint32_t flags }` ——
size 是 **int32_t**，pts 在 +8、flags 在 +16，总 24 字节。

libunity 反汇编证实 MoviePlayer 按 +8 读 pts、按 +16 读 flags
（bit2 测 EOS）。错误布局引发三重后果：

1. `Same timestamp for frame 0 and 1: 1 us` 警告——Unity 把 size=1 当 pts；
2. EOS flag 写在 +24，Unity 在 +16 读到的是 pts 低 32 位——
   fix75 握手在 Unity 侧从未完成，视频结束后 Unity 直接重开第二个视频；
3. 第二帧 pts=33333（bit2=1）被误判为 EOS；叠加 dequeue 里 32 字节
   memset 越界 8 字节、清掉 Unity 栈上 sp+0x38 的"待渲染帧 vector"
   数据指针（初始 NULL、cap=1，已从 .rodata 常量证实）→
   drain 路径读 size=1 + data=NULL → 崩溃寄存器逐位吻合。

### 13.3 修复

- 912357c：greak media.c 改用 24 字节 `m_buffer_info`，memset/memcpy
  同步收窄。md5 15adf8ef 部署后实机验证通过：游戏正常进入。
- 09018fd：bad2bad 同款修复（其 EOS 也写在 +24，从未被看到）。

### 13.4 经验教训（跨 port 通用）

1. **NDK 结构体必须对照权威参考实现核对偏移**，不能凭记忆——
   AMediaCodecBufferInfo 的 size 就是 int32_t 不是 size_t。
   同系列成熟移植（BENDER/unity_probe.c）是最快的对照源。
2. **反汇编客端读取代码是终极裁判**：日志反常文本（时间戳警告、
   EOS 不生效）+ 寄存器逐位比对，能把"猜桩实现哪里错"变成"读客端
   期望什么"。
3. **memset/memcpy 的尺寸随结构体连带审计**：写错结构体时往往同时
   写越界，破坏调用方栈上相邻局部变量（本案 vector 指针），症状会
   远离病根。
4. **签名修复后签名不变 = 修的不是这个错**：两轮日志逐字 diff 应
   在第二轮立刻做，本例浪费了一次部署往返。
5. 调试期保留的限频 trace（TRACE_CAP）与无条件 fprintf（EOS armed、
   bounded reopen）分工明确，是五轮实机联调能闭环的关键。

## 14. 按键与屏显不符：GPTK 键盘化映射劫持了原生手柄（2026-10-01，实机验证通过）

### 14.1 现象

进游戏后手柄可用，但「游戏中 B = 对话/推进」的屏显提示与实机不符：
按物理 B 不推进对话，而是直接弹出暂停菜单。

### 14.2 取证链

1. **InputManager 无自定义绑定**：用 UnityPy 解析
   `assets/bin/Data/data.unity3d` 的 InputManager，231 条全是 Unity 默认
   匿名摇杆轴，没有一条 `joystick button N` / 键盘绑定 —— 游戏所有输入
   都是代码里直接轮询 KeyCode，不看 InputManager。
2. **输入插件是 InControl（Unity 模式）**：global-metadata.dat 实锤
   `InControl|UnityInputDeviceManager`、`UnityInputDeviceProfileList`、
   `TankAndHealerStudioAssets|BasicInputDisplay`（屏显提示组件），
   APK 内无 InControl native 库 → 走 Unity 模式，手柄绑定即
   JoystickButton，键盘绑定即 KeyBindingSource，两套并存。
3. **设备身份决定屏显**：假 JNI 层上报 "Microsoft X-Box 360 pad"，
   InControl 据此匹配 Xbox360 profile，屏显按 Xbox 图形布局显示。
4. **我们的注入把动作推到另一套绑定上**：GPTK 内置默认（继承自 BENDER
   的"手柄→键盘"方案）把 A/B/X/Y 劫持成 Enter(66)/Esc(111)/Space(62)/Tab(61)，
   并显式把这些物理键从 legacy 原生手柄路径中排除。于是屏显说的是
   DeviceBinding（JoystickButton1 = B），实际触发的是 KeyBinding（Esc
   = 打开菜单）—— 显示与实机必然不符。

### 14.3 修复（c40fa00，md5 44e5e83c，实机验证通过）

`gptk_adapter.c` 内置默认只保留语义摇杆路由：

```
[gameplay]
LEFT_STICK = sf.move
```

A/B/X/Y/START/L1/R1/SELECT/D-pad 全部交还 legacy 原生直通：
96/97/99/100、START→105（实测菜单响应码）、102/103、109、19-22。
动作与屏显同源于 DeviceBinding，一一对应。owner 的
`NEXTOSCONTROLLERS.gptk` 仍可重新语义映射，框架能力不受损。

### 14.4 经验教训（跨 port 通用）

1. **先判"客端是键盘驱动还是手柄驱动"，别照抄兄弟 port 的键位表。**
   BENDER 的键盘化方案适用于 UI 只认键盘的老游戏；本作 Android 版原生
   支持手柄、屏显又是手柄图形，就该全量直通，语义层反而制造错位。
2. **判断依据要取自游戏自身**：`InputManager` 有无自定义绑定 +
   元数据里的输入插件（InControl / Rewired / 原生 Input），
   两分钟就能定性，比猜按键表可靠得多。
3. **双绑定并存的游戏，屏显与动作可能走不同套件**：一旦注入的 keycode
   与屏显套件不匹配，就会出现"按键有效但做的不是提示的那件事"——
   这种症状几乎总是映射层选错套件，而不是游戏 bug。
4. **语义映射层要允许"什么都不映射"**：内置默认把按键全让给原生直通，
   是支持手柄的游戏最安全的默认；需要重映射时再用 owner gptk 文件。
5. 工具沉淀：UnityPy（1.25.3，已装 default venv）可直接读
   `data.unity3d` 的 Typetree，查 Unity 游戏输入绑定/设置最省事。
6. **同时支持手柄与触屏的游戏，注入码必须恒定走手柄源（96..110）。**
   这类游戏按"最后一次输入事件的 source"切换操作模式：一个
   `SOURCE_KEYBOARD` 事件就会把它踢进触屏/键盘模式，随后手柄全废
   （§14.5 的 START→Esc 就是这么栽的）。菜单/暂停键也要在手柄区段内解决，
   不能图省事发 Enter/Esc。

### 14.5 START：踩了"输入源切换"的坑（同日补修，md5 649de597 实机通过）

实机反馈：物理 START 无任何反应，希望能打开菜单。

**第一修（错误，已回退）**：把 START 发成 **111（Escape）**。菜单确实弹出来了，
但**手柄随即整体失灵**——按 A/B/X/Y、摇杆全部无响应。

**根因（本次最关键的机制发现）**：本作同时支持**手柄**与**触屏**两种操作模式，
并且**按"最后一次输入事件的 source"决定当前模式**。111 走
`SOURCE_KEYBOARD`（`sf_jni_key_event()` 对非 96..110 的码强制键盘源），
游戏据此切换到键盘/触屏模式，之后所有 `SOURCE_GAMEPAD` 事件被丢弃。
所以：**开局之后任何按键都不能用键盘码注入**，菜单键也必须在手柄区段内找。

**第二修（正确）**：START 发 **108（KEYCODE_BUTTON_START）**，属 96..110 区段、
`SOURCE_GAMEPAD` 源，不触发模式切换；与 InControl 在 Android 上匹配的
`Xbox360AndroidUnityProfile`（Start = Unity joystick button 12 = 108）一致。
触发器 LT/RT 仍是模拟轴、不产生 KeyEvent，不会误暂停。

**此前 105（KEYCODE_BUTTON_R2）为何无效**：该码来自兄弟 port 注释
"InputManager 把 Menu 映射到 joystick button 7/9"，而 §14.2 已证伪——本作
InputManager 零自定义绑定，105 只落到 `JoystickButton9`（右摇杆按下键）。
