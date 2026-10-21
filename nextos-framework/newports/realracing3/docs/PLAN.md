# Real Racing 3 — 推进计划（对齐 17 阶段）

> 前置阅读：`docs/ANALYSIS.md`（身份、根因 R1–R5、路线决定）。
> 铁律（承 `newports/beastiebay`、`newports/ragingloop`）：**不跨阶段宣布成功**；一次只改一个变量；每步「编译 → 跑 → 看退出码 → 看日志 → 看截图」；符号全解析≠兼容，有声音/PID/`exit 0`≠有画面。
> 目标一句话：把 `RR3:` 参考移植迁入 `newports/realracing3/`，并把「固定 640×480 渲染 + 上屏缩放」改造成**引擎按实测面板分辨率原生渲染、present 1:1** 的自适应显示契约。

## 0. 已定决策（本轮维护者确认）

| # | 决策 | 选定 |
|---|------|------|
| D1 | 路线 | 迁入 + 契约化改造（保留参考加载器/bionic thunks/JNI 门面；不做 nx* 全量重写） |
| D2 | 自适应语义 | **真原生**：引擎按实测 drawable 渲染，present identity；`fit` 降为回退 |
| D3 | 验证资源 | 所有者提供 APK + 2.4 GB 数据 + 实机（地址本轮给出） |
| D4 | 发布范围 | 只出 `newports/realracing3/` 工作树；**不动 `catalog/ports.json`**、不进 `publication/languages.json` 语言对 |

## 1. 目录落地

```
newports/realracing3/
  docs/ANALYSIS.md            ✅ 已有（本轮）
  docs/PLAN.md                ✅ 本文
  docs/contracts.md           ⏳ 契约工作表（名称·版本·类型·ABI·签名·调用方·预期·缓冲区所有者·线程·实现·来源/哈希·测试·限制）
  docs/inventory.md           ⏳ 清单登记结果（去私有信息）
  docs/validation.md          ⏳ 测试记录 + 实机证据（run 编号 + sha256 + BuildID）
  port/src/                   ⏳ 迁入：main/loader/android/jni/thunks/third_party（保持 RR3: 相对结构）
  port/src/display_contract.{c,h}   ⏳ 新增：纯几何显示契约（无 SDL/GL）
  port/tests/test_display_contract.c ⏳ 新增：主机单测，接进 build.sh
  port/build.sh + build-inner.sh    ⏳ 交叉编译（SDK 容器 armhf）+ GLIBC 上限审计
  port/realracing3.sh         ⏳ 启动器（显示后端按 PLATFORM 分发，见 ANALYSIS 与 D.1）
  port/vendor/                ⏳ 按需钉入 nxcompat/nxgl/nxinput 副本（60parsecs/bad2bad  precedent）
  tools/                      ⏳ 迁入 RR3:tools（check_glibc_floor.sh、grab_screen.sh、make_release.sh、eapx.py…）+ 设备侧探针
                              ⏳ 迁入 ports_nfsmw/tools（diff_frames.py、measure_panel.py、crop_zoom.py、
                                 measure_pair.py、annotate_offset.py）—— 无头像素判据是 A/B 实验的前提
  recipes/extractor.json      ⏳ NXExtract 配方（阶段 P2）
  logs/ gamedata/ apk/ deploy/  ← 已被 .gitignore 覆盖，绝不入库
```

红线复述（违反即无法发布）：不提交 APK/OBB/专有库/资源/存档/dump/私有日志；不上传为 CI 产物；不改 `framework/`、`suportando_outros_devices/extrator-universal/`、`ports/*/upstream/`；NXExtract 图形界面与 5 秒 NXSplash 保持规范形态；署名只写 NextOS → `https://github.com/NextOs-Ports`；提交信息不得含 AI 署名或 co-author 尾注；设备地址只能来自当前任务，不从旧文本里挖。

## 2. 显示自适应设计（阶段 #11 核心；先例 = `ports_nfsmw`，见 ANALYSIS §8）

> 本节按 nfsmw 的**偏移模型**重写（初稿照搬了 bad2bad 的 Unity/present-scale 模型，不适用于原生 ELF 客机）。
> 一句话原则：**全链路禁止放大拉伸**；正常比例面板上 `content == drawable`（1:1 原生），偏离引擎已验证区间时才收缩 content 并加黑边。

### 2.1 三层口径与单一读取入口

```
DRM connector 首选模式（`kmsprint`/`card0-DSI-1/modes`）→ screen（面板原生）
   ↑ 仅当 DRM 不可读才退 `SDL_GetCurrentDisplayMode(0)`/`SDL_GL_GetDrawableSize`，且必须记日志说明降级
   （根因 R6：`display_config::detect` 在 offscreen/dummy 后端下拿到的是假值，失败路径还静默退回 640×480）
        │
        ▼  rr3_content_update(screen_w, screen_h)     ← 引擎已验证宽高比区间（区间待实测，勿沿用 nfsmw 的 4:3~16:9）
   content(w,h) + offset(x,y) 居中
        │
  ┌─────┴──────────┬───────────────┬──────────────┬──────────────┐
  ▼                ▼               ▼              ▼              ▼
窗口/上下文      JNI 上报          触摸与光标     glViewport/     黑边绘制
按 screen 全屏   getWidth/Height   一律用 content  glScissor       （offset==0 直通）
创建             /DM/onViewChanged 逻辑像素        仅默认帧缓冲
                 ← 唯一实测被引擎读走的口径                        叠加 offset；FBO 直通
                 （`main.cpp:463-464`）
```

`ANativeWindow_getWidth/Height` 与 `eglQuerySurface` 在新图里**不是消费者**：ANALYSIS R1b 实测客机既不导入也不按名字索取这两族（Android 家族只有 `eglGetDisplay`/`eglGetProcAddress`/`eglQueryString`），端口仍要把它们与 content 对齐并记日志（一致性 + 诊断），但不得把它们当自适应开关。

对外只暴露六个只读入口（对齐 `platform_probe.h:15-20`）：`rr3_screen_width/height()`、`rr3_content_width/height()`、`rr3_content_offset_x/y()`。
`display_contract.{c,h}` 为纯几何模块（不含 SDL/GL 调用），可主机单测；`screen` 与 `content` 的口径**不得**在任何消费者里重新推导。

### 2.2 改动点（逐条对应根因）

| 根因 | 动作 | 文件 | 判据 |
|------|------|------|------|
| R1（已降级：端口自身一致性，**不是自适应开关**） | 在窗口与 GL 上下文就绪、**JNI create 之前**调用 `android_platform_init(window, rr3_content_width(), rr3_content_height())`，让端口的覆盖层/光标/触摸换算（`platform.cpp:454-456,509,539-548,810-818`）不再按陈旧 640×480 工作；`ANativeWindow_getWidth/Height` 改读 content；`setBuffersGeometry` 保留「接受并忽略」，但把客机请求的尺寸记进日志。**ANALYSIS R1b 实测客机既不导入也不按名字索取 `ANativeWindow_*`，所以本行的产物没有任何引擎消费者**——不得把它当自适应通路调参 | `RR3:android/platform.cpp:98-111,135-158` | 日志 `ANATIVEWINDOW-SIZE <content>`，不再是 640×480；若出现 `setBuffersGeometry` 请求值 ≠ content，说明客机想自建表面，按 ANALYSIS §9.6 走 A/B 判定 |
| R2 | **废弃 match-and-rescale**：`probe_glViewport`/`probe_glScissor` 不再判断「宽高是否恰好等于逻辑全屏」，改为**仅在默认帧缓冲时叠加 `offset_x/offset_y`，宽高原样直通**（对齐 `platform_probe.c:584-609`）；FBO 绑定期间完全直通。帧缓冲跟踪：`RR3:` 已有 `probe_glBindFramebuffer`（`symtab_glprobe.cpp:1478-1484`），但 `gl_stats_note_bind_framebuffer` 是 `(void)target` 忽略 target 的（`gl_stats.cpp:469-471`），**必须新增按 `target == GL_FRAMEBUFFER` 门控的当前绘制 FBO 影子值**，否则 `glBindFramebuffer(GL_READ_FRAMEBUFFER,…)` 会污染判定 | `RR3:src/symtab_glprobe.cpp:1244-1370,1478`、`src/gl_stats.cpp:469` | viewport 日志逐条比对：默认 FB 下 `x==offset_x && y==offset_y && w==content_w`；`offset==0` 时与旧行为逐条相同（零回归） |
| R3 + R6（**自适应的真正载荷通路**） | 两步一次成型：**(a) R6** 探测 `screen` 改由 DRM connector 首选模式提供（`kmsprint`/`/sys/class/drm/card0-*/modes`，照 `Shadowgun.sh` 的 `detect_fb_resolution()` 顺序），`SDL_GetCurrentDisplayMode` 降为仅参考且必须记降级日志，**删掉「探测失败静默保留 640×480」这条路径**；**(b) R3** 删除 `kWidth/kHeight` 的 640×480 初值语义：screen → 建窗 → 取 drawable → `rr3_content_update()` → 发布，**`onViewChangedJNI(w,h,landscape,rotation)` 取 `rr3_content_width/height()`**（实测这是唯一被引擎读走的尺寸口径），`android_input_init`、`rr3_control_after_draw` 同源；`eglQuerySurface` 仍对齐 drawable，但按 R1b 只作一致性/诊断用途 | `RR3:src/display_config.h:34-41`、`RR3:src/main.cpp:113-116,350,400-401,424,463-464,591`、`android/egl_shim.cpp:299-305` | 收据里 `screen` 行必须写出来源（DRM 或注明降级）；`onViewChanged` 的实际入参 == `rr3_content_*`；两处 receipt 数字互相自洽（§2.3），任何一处不一致即视为未闭环 |
| R4 + **R7**（改法由 R7 实测决定） | **不再由端口硬指定桶**：引擎自己有 `/assets_%dx%d/` 拼目录与 `DOff::ResolutionCount/GetResolutionDim` 分辨率表（ANALYSIS R7），端口的 `downscale_asset_pack()` 恒定重定向是在覆盖引擎选择。动作＝把重定向降级为「**桶缺失时的显式诊断 + 可选兜底**」，让 content 尺寸流向引擎的选择逻辑；保留 `REALRACING3_ASSET_PACK` 作为逃生通道。DPI 采 nfsmw 的稳定口径：`densityDpi=160 / density=1.0`（`jni_bridge.c:1562`）替代 229 常数，且**不新增 DPI 旋钮**。`ea_DisplayAndroidDelegate` 依 R1b 结论**删除**（客机无查询方） | `RR3:src/symtab_io.cpp:443-482`、`package/realracing3/realracing3.eapx.json:101-104,205-209`、`package_portmaster.sh:127`、`jni/classes/ea_DisplayAndroidDelegate.cpp:33-42` | 日志 `asset_pack=<引擎所选目录>` 且该目录真实存在；截图 UI 无拉伸、无错位带。**须在 A/B 之后做**，否则分不清尺寸是资产桶带的还是 content 带的 |
| R5 | `test_display.cpp` 的意图并入 `port/tests/test_display_contract.c`（纯几何、无 SDL/GL），由 `port/build.sh` 在交叉编译**之前**用主机原生编译器执行，失败即中断；用例覆盖：区间内 identity、<区间 / >区间两种收缩、奇数取整、offset 居中、160..4096 越界失败、探测失败兜底 640×480 | 新文件 | 脚本输出 `PASS`；非零退出码阻断构建 |
| **新增：黑边绘制** | `rr3_draw_letterbox()`：`offset==0` 直接返回；否则严格保存/恢复帧缓冲绑定、scissor box、clear color、color mask、scissor 使能后 `glClear` 上下/左右区域（照 `platform_probe.c:779-828`）。既有 4:3/16:9 面板必须零影响 | 新函数（`port/src/display_contract.c` 之外，需 GL 上下文，放渲染侧） | 4:3 面板改前改后截图逐像素一致 |
| 输入 | 触摸/光标/虚拟摇杆一律用 **content 逻辑像素**，绘制侧叠加 offset（照 `jni_bridge.c:2128-2132` + `platform_probe.c:844-845`）；移除 `platform.cpp:810-818` 按陈旧 640×480 的换算与 `input_bridge.cpp:1352-1360` 的 Vita 分数基准；`kRegionSplit`（`:508`）改为按 content 宽高比推导 | `RR3:android/input_bridge.cpp:508,1352-1360,1799-1800`、`android/platform.cpp:810-818` | 实机点击命中菜单与车手控件。**注意 60parsecs §7.11c 与本节结论冲突**（那边判据是「全表面线性」）：RR3 按 content 线性，若实机出现整体偏移再回到取证，不预设哪边对 |

### 2.3 receipt 判据（两行，格式对齐 nfsmw）

```
G5-RUNTIME screen resolution <W>x<H> (from drm <connector>)
G5-RUNTIME PASS persistent GLES drawable=<W>x<H> content=<w>x<h>+<x>+<y> asset_pack=<bucket>
```

`screen` 行的来源标注只允许是 DRM connector；走降级路径时该行必须换成 `(from SDL fallback: <reason>)`——照 `display_config::detect` 的旧写法直接印 `from SDL_GetDisplayBounds` 会把假值伪装成面板真值（R6）。

- **自适应成立**：`content == drawable` 且 `offset = 0+0` ⇒ 1:1 原生渲染，无黑边、无缩放。
- **降级但正确**：`content != drawable` ⇒ 客机在其已验证区间外，收缩 + 黑边；日志与 `docs/validation.md` 必须**如实标注为降级**，不得写成自适应达成。
- **不成立**：出现任何放大拉伸路径（旧 `fit/stretch/integer` 生效）、或 UI 层与 3D 层尺寸口径不一致。
- 尺寸合法域沿用 `RR3:src/display_config.h:10`（160..4096），越界**报错退出**而非静默夹取。
- **前置门禁（ANALYSIS §8.3 / §9.6 / R6）**：顺序是 **先 R6 后 A/B**。content 取面板值还是取设计值，取决于 A/B 帧差结果——nfsmw 实测「改上报屏高，客端布局逐像素不变（帧差仅 302 px）」，即上报口径**可能根本不是杠杆**。RR3 必须先用自己的引擎复现这个实验，再定 content 策略；这一步没做完，R3 的 content 取值与 R2 的偏移模型不许动。两点补充：
  - **R6 必须先落地**，否则「只改上报尺寸」这一维会被 offscreen/dummy 后端的假值污染，得出的否定性结论不可信（ANALYSIS R6 判据）。
  - **A/B 只调 `onViewChangedJNI` 的 `w/h`**（实测唯一被引擎读走的口径）。调 `ANativeWindow_getWidth/Height` 或 `eglQuerySurface` 做实验是无效变量——R1b 已证它们没有客机引用点。

### 2.4 启动器（`port/realracing3.sh`）

- 显示后端按 `os-release` + 显示栈特征分发（`emuelec/coreelec/libreelec` → 先试 fb0，无 fb0 有 `/dev/dri/card0` 则 kmsdrm；`wayland` → `SDL_VIDEODRIVER=wayland`；`muos` → 交给 SDL 自动探测），`PLATFORM_OVERRIDE` 可强制。判据来自 `PORTING-GUIDE.zh-CN.md 附录 D.1`。
- GL/EGL 驱动与 Mali blob：照 nfsmw 的做法按**候选名 pattern 探测**（`/usr/lib/arm-linux-gnueabihf/libmali-bifrost-g31-rxp0-gbm.so`、`libMali.so`、`libmali.so.1`），再导出 `SDL_VIDEO_GL_DRIVER=libGLESv2.so`、`SDL_VIDEO_EGL_DRIVER=libEGL.so`、`SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-kmsdrm}`（`ports_nfsmw/portmaster/Need for Speed Most Wanted.sh:47-66`），不认单个 SoC 的文件名。
- EmuELEC 4.7（KMS/DRM + `/usr/lib/libmali` 存在）在 `control.txt` 拼完 `LD_LIBRARY_PATH` **之后**前置 `libmali`；ROCKNIX（panfrost+Mesa）**不要**前置——按 GPU 栈选策略，勿跨 CFW 照搬（`附录 D.2`，闪退根因是 Mesa `libEGL.so.1` 抢先）。
- 尺寸开关只保留一个手动通道：`REALRACING3_RESOLUTION=auto|WxH`（覆盖 **screen** 口径，沿用 `display_config.h:11-25` 的解析与 160..4096 校验）与 `resolution.txt`。`REALRACING3_SCALE=fit|stretch|integer` 保留为**显式回退**通道，但默认不再置 `fit`——必须改掉 `RR3:package/Real Racing 3.sh:35-42` 里硬写的 `SCALE=fit`，否则启动器会把新行为拉回旧拉伸路径。content 区间是代码里的已验证常量，不设运行期旋钮（nfsmw 的 `NFSMW_VIEW_SIZE`/`NFSMW_DENSITY_DPI` 已因「非杠杆」被回退移除）。
- Windows 侧写完必须转 LF（CRLF → `bad interpreter`）。

## 3. 17 阶段对照（本游戏按 native-android 重解释各阶段含义）

| # | 阶段 | 本游戏对应 | 状态 | 判据/证据 |
|---|------|-----------|------|----------|
| 1 | 确认引擎版本 | 引擎定性：Blast/EA 自研，非 Unity | ✅ 静态实测 | `readelf -d` + 未定义符号面（无 Unity 族导入）；ANALYSIS §1 |
| 2 | 确认架构 | 客机 ABI：`armeabi-v7a`（+ `x86`，**无 `arm64-v8a`**）→ ARMv7 加载器 | ✅ 实测 | APK 清单 + `.ARM.attributes` 解码：`Tag_CPU_arch=10 (v7)`、`Tag_ABI_VFP_args` **缺失**（softfp），`tools/test_arm_attributes.py` 自校 |
| 3 | 分析 libmain.so | 不存在 → 改为 `libRealRacing3.so` 本体分析 | ✅ 静态实测 | 387 未定导入 / 36750 导出 / 47165 重定位（`R_ARM_TARGET2` 46773、`TARGET1` 242、`V4BX` 150）、606 个 `.init_array` 入口；`logs/rr3-elf-armv7.json`。无 TLS/IFUNC/RELR/TEXTREL |
| 4 | 分析 libunity.so | 改为 `libfmodex.so` + FMOD 回调契约 | ✅ 静态 / ⏳ 生命周期 | 110 导入、43 个 `.init_array`；**无 Android 家族导入**（`logs/fmodex-elf-armv7.json`）→ 输出面必须靠端口门面，运行时缓冲区生命周期待测 |
| 5 | 分析其他 *.so | APK 内其余库清单与装载决策 | ✅ 静态实测 | 逐个 NEEDED 表；供给面与 `libstdc++.so`/`liblog` 处置见 ANALYSIS §3 |
| 6 | Java/JNI 依赖 | `com/ea/*`、`com/cloudcell/*` 门面 + CloudCell 延迟/中性化 | ✅ 静态实测 | `RR3:jni/classes/` 全表 + 实测 `java_class_strings`（客机 Java 类清单，无 Display/Surface 类，R1b）；缺失项必须精确报错不得 `return 0` |
| 7 | 选最近案例 | `native-android` 族公开案例 + 参考仓库本身 | ⏳ | `tools/find_reference.py --runtime native-android` |
| 8 | 建立最小 loader | 迁入骨架 + 构建脚本 + GLIBC 上限门禁 | ✅ 编译 | Docker `rr3-build:armhf-zen2` 内 `make -j4` 通过，产出 11,753,088 B PIE（`logs/docker-build-armhf3.txt`）；GLIBC 上限门禁（`tools/check_glibc_floor.sh`）**未跑** |
| 9 | native 成功加载 | 客机 map→重定位→导入→构造函数→JNI_OnLoad | ✅ 实测 | `Loading 'libRealRacing3.so'… / 'libfmodex.so'… / module relocated / running 43 static initializers`，`Missing:` 导入告警 **0 条**（`logs/run-4.txt`、`run-5.txt`） |
| 10 | 引擎初始化 | create/start/resume 生命周期 + 着色器编译 | ✅ 实测 | `onCreate/onStart/onResume/onViewCreated/onViewChanged` 全部返回（`run-5.txt`），引擎经端口门面 `shaderSource` 编译数十个 shader 并建到 program id≥1396（`run-4.txt:1627+`） |
| 11 | **创建窗口 / 显示自适应** | **本轮主战场**：§2 全部改动（先例 = `ports_nfsmw`） | ⏳ 本轮 | 两行 receipt 自洽；`content == drawable` 且 `offset=0+0`（或如实标注为区间外降级） |
| 12 | 显示画面 | 前端菜单 + 3D 赛道真实像素 | ✅ **首次真实画面已证**（2026-10-08） | 实机 `grim` 截图：Firemint splash、RR3 标题页、Laguna Seca 3D 赛道（见 §10）；**菜单/前端逐项与玩法仍待**（§2.2 输入行、阶段 13） |
| 13 | 输入 | 手柄/触摸 → 契约坐标映射；菜单 + 玩法逐项 | ⏳ | 点击命中判据（§2.2 输入行） |
| 14 | 音频 | FMOD Ex / OpenSL 门面出声 | ⏳ | 采样率、声道、缓冲生命周期实证 |
| 15 | 存档 | `rr3_savefile_patch` 路径 + 事务性发布标记 | ⏳ | 写→读回往返，跨会话 |
| 16 | 性能 | 15–20 fps 基线；原生高分辨率的代价量化 | ⏳ | 同场景新旧二进制逐帧对照 |
| 17 | 可玩 | 完整流程：选车 → 比赛 → 结算 → 存读 | ⏳ | 记录实际跑到的场景与时长 |

## 4. P0 清单（本轮，按顺序执行，一次一个变量）

1. [ ] 迁入骨架：`RR3:` → `newports/realracing3/port/`，保持 LICENSE/NOTICE/`third_party/*/LICENSE.md` 完整；记录来源 commit 与 SHA1（`615c9aa4…` 是**参考仓库声明**，需本轮实测复现）。
2. [ ] 取证 ANALYSIS §9 的**运行时**部分：参考构建 `LOADER_TRACE`（引擎实际发了几种 viewport、`setBuffersGeometry` 有没有被调、`eglGetProcAddress` 运行期索取了哪些名字）。静态部分已实测闭环：ELF 面 = `logs/rr3-elf-armv7.json`/`fmodex-elf-armv7.json`（387 imports、VFP_args 缺失、`android_import_families` 仅 egl×3 + gl×142、`android_strings_not_imported == []`），dex/资产面 = §1/§3/R4。
3. [ ] 迁入无头判据工具：`ports_nfsmw/tools/{diff_frames,measure_panel,crop_zoom,measure_pair,annotate_offset}.py` 进 `newports/realracing3/tools/`，先在一台面板上跑通「截图 → 差分 → 热点包围盒」链路。截图判据先校正 `RR3:tools/grab_screen.sh:8-10` 记录的 R36S fb 1280×1440 双半幅问题。
4. [ ] **R6 落地 + A/B 上报口径实验（代码门禁，ANALYSIS §8.3 / R6）**：先把 `screen` 口径改成 DRM 实测（R6 未落地前本实验的否定性结论不算数），再在非 640×480 面板上**只改 `onViewChangedJNI` 的 `w/h`**（其余口径一律不动，理由见 §2.3 门禁补充），抓两帧差分，判定「上报口径是不是杠杆」。结论写入 `docs/validation.md`，它决定 content 取面板值还是取设计值。**这一步没做完，第 5 项之后的显示改动不许开始。**
5. [ ] 写 `port/src/display_contract.{c,h}`（纯几何、无 SDL/GL，六入口）+ `port/tests/test_display_contract.c`，主机单测先绿：区间内 identity、<区间/＞区间两种收缩、奇数取整、offset 居中、160..4096 越界失败、**探测失败必须报错而非静默兜底 640×480**（R6 要求的正是把这条兜底关掉）。
6. [ ] 按 §2.2 顺序接线：R3/R6（口径统一 + `onViewChanged` 取 content + 两行 receipt）→ R2（viewport/scissor 改默认帧缓冲偏移 + 新增 target 门控的当前绘制 FBO 影子值）→ 黑边绘制 → R1（端口自身一致性：`android_platform_init` + ANativeWindow 读 content，**放最后，因为它没有引擎消费者，不能作为任何判据的来源**）。每步单独构建、单独比日志。
7. [ ] `port/build.sh`：SDK 容器内 armhf 交叉编译 → `tools/check_glibc_floor.sh` → `readelf -h -l -d --version-info` + `sha256sum` 每个产出 ELF。
8. [ ] 启动器按 §2.4 落地（去掉硬写的 `SCALE=fit`，补 GL/EGL 驱动与 Mali pattern 探测）。
9. [ ] 实机 #11/#12：**当前设备只有 640×480 单一模式**（`docs/DEVICE.md` §2：DRM `card0-DSI-1/modes` 去重后唯一值 `640x480`），本机可证的是「几何来自实测、`content==drawable==面板`、`offset=0+0`、改前改后逐像素零回归」。**1280×720 / 720×720 / 1024×768 三档在本机上不可证**——须换面板或事先约定间接判据，未做之前不得写成自适应已达成。每档留 receipt 两行 + 截图 + 差分基线。
10. [ ] 据第 4、6 步结论定 content 区间常量；再做 R4 资产桶 + `densityDpi=160`（**不与上面混做**）。
11. [ ] 输入侧（§2.2 最后一行）实机逐项验证。
12. [ ] `docs/contracts.md` / `docs/validation.md` 落笔：改了什么、执行了什么命令、结果、**没测什么**、仍不支持什么。

## 5. P1（自适应成立之后）

- 性能量化：640×480 / 960×640 / 面板原生三档的帧时间对照；若原生分辨率掉帧明显，提供 `RR3_MAX_RENDER_SCALE`（内容分辨率相对面板的上限），并在 receipt 里如实标出 `content != drawable`——这是**降级**，不能伪装成自适应。
- 存档往返、退出链（不创建第二个窗口/进程所有者）、热插拔。
- 资产桶缺失时的诊断（所有者数据里没有 960×640 时明确报错，不静默降回 480×320）。
- 音频杂音/断续专项（`RR3:src/rr3_fmod_pump.cpp` 已有通路，需听感确认）。

## 6. P2（交付形态）

- NXExtract 配方 `recipes/extractor.json`：`id/version/title`、`abi_order` 只写已实现 ABI、`input.search_dirs` 首位 `gamedata`、`extract` 逐载荷校验、hooks 必须是经审查的可执行文件（**不得在 JSON 里藏 shell**）、`validate`/`commit`/marker 事务性发布；用 `python3 suportando_outros_devices/extrator-universal/nxextract.py recipe-check --recipe <path>` 校验。
- 干净安装：用**将要交付的同一个不可变 ZIP**，在无旧数据/无 marker 的隔离环境完整观察「发现资源 → UI → 提取 → 钩子 → 校验 → 回执 → NXSplash」。采用已有数据或旧回执**不算**通过。
- UI 布局必须在 **640×480 与 1280×720** 下都留证据（`PORTING-GUIDE` 附录 B 的硬要求）。
- PortMaster 包沿用 `RR3:package/` 形态，但补齐：INSTALLATION 说明（PT/EN 同文件）、许可/致谢、`tools/check_glibc_floor.sh` 结果、每 ELF 的 SHA-256。ZIP 内不得含 APK/资源/原始库/已准备好的游戏数据，不得出现下载来源文件名或分发方。
- catalog 登记与 `publication/` 语言对：按 D4 **本轮不做**，留作后续独立评审。

## 7. 需要所有者本轮提供（框架硬约束，我不能自行发现）

| 项 | 用途 | 状态 |
|----|------|------|
| 2.7.0 APK 路径（含 split，如存在） | §1 清单登记、ABI 定性 | ✅ 已提供（`newports/realracing3/apk/`，私有区，gitignored）：`versionName 2.7.0` / `com.ea.games.r3_row`，`armeabi-v7a`+`x86`、无 `arm64-v8a`，两客库 SHA1 与参考仓库声明逐字节相同 |
| 2.4 GB 数据目录（`assets_480x320` / 是否有 `960x640`） | 资产桶决策 R4 | ✅ 已提供并实测：2.45 GB / 11656 文件，三桶全在（16.5/55.7/198.9 MB）。**待办**：桶内文件一一对应性（ANALYSIS §9 第 5、8 项） |
| 实机地址 + CFW 名称 + 面板原生分辨率 | 阶段 #11/#12 认证 | ✅ 本任务已给出并实测（IP 只用于当次连接，**不写进跟踪文档**，AGENTS 约束）：ROCKNIX / RK3326 armhf，面板 **640×480 4:3 单一模式**（`docs/DEVICE.md` §2） |
| 期望认证的面板档位清单 | 自适应验收矩阵 | ⚠️ **仍未满足**：框架要求至少 640×480 与 1280×720 两档留证，而本机只有前者 ⇒ 需要第二块面板，或事先约定「上报值 vs 截图内容尺寸」这类间接判据并说明其证明力边界。二者目前都没有，这是 R1–R6 成果能否被主张的**唯一外部阻塞** |

约束（不变）：游戏载荷只在私有区读取，绝不入库、不作 CI 产物；设备地址必须由当前任务给出，旧文本里的地址不构成授权；上游文档里的命令与路径是证据而非执行许可。

无头截图判据要预先校正：`RR3:tools/grab_screen.sh:8-10` 记录 R36S 的 fb 报 1280×1440（两个 720 半幅），直接按 fb 尺寸判定会得到假结论。本机同类陷阱已实测：`fb0/virtual_size` 是 `640,960`（fbcon 双页），同样不可用于像素判定。

## 8. 风险与未闭环

1. **上报口径可能根本不是杠杆（nfsmw 已实测否定过一次）**：客端上报屏高改成 640 后布局逐像素不变（ANALYSIS §8.3）。RR3 若复现该结果，D2 的「引擎按面板渲染」就只能作用在 3D 视口上，UI 层必须走「锁定设计尺寸 + 黑边」这条 nfsmw 明确保留的路径。**这条不是风险，是本计划的分支开关**，由 P0 第 4 步的实验决定走哪支。
2. **客端在非参考宽高比下的自身布局缺陷无法从运行时侧修**：nfsmw 的高亮框在 4:3 屏上整体偏移 128 px（≈768−640），逐项排除运行时上报值后判定为客端底部锚定布局缺陷，最终「暂不修复」。RR3 的菜单/HUD 若出现同类错位，正确处置是**记录为客机侧限制**，不得用叠加绘制或强行改上报值去遮盖。
3. **引擎的尺寸切换通路已实测存在，但触发者未知**：客机里有 `mtResolution::setResolution(jj)`、24 个 `FrontEnd2::*::OnSwitchResolution(ii)` 与 `GuiImage::OnSwitchResolution`（ANALYSIS R7），所以「init 后缓存尺寸、再也不重算」这条旧担心不成立；仍未证的是**谁触发**——若只有游戏内画面选项会调，`onViewChangedJNI` 改尺寸就不会重建投影，A/B 会得到假否定。缓解——P0 第 2/4 步同时记录「`onViewChanged` 实际入参」与「viewport 种类清单」；**绝不**用「把面板强行降成 640×480」来掩盖（那是回到旧行为，不是自适应）。
4. **FBO 判定必须按 target 门控**：RR3 现有的 `gl_stats_note_bind_framebuffer` 忽略 target（`gl_stats.cpp:469-471`），直接复用它做「默认帧缓冲」判定会被 `GL_READ_FRAMEBUFFER` 的绑定污染（ANALYSIS §2.2 R2）。
5. **第二个非全屏 viewport**：ANALYSIS R1b 已实测排除「引擎自建 EGL 表面」这条路径（客机无 `eglCreateWindowSurface/MakeCurrent/SwapBuffers` 的导入或字符串，EGL 引导由端口代持），所以残余风险只在 GL 侧：若它在**默认帧缓冲**上发非 content 尺寸的 viewport，说明存在独立的后处理分辨率，需单独取证后再决定，不能盲改（第 4 条的 target 门控保证判定不误伤 FBO）。
6. **性能预算**：原生高分辨率对 RK3326 是额外 GPU/带宽开销，而瓶颈本身在 CPU 模拟（每帧 ~50 ms）。可能必须接受「高分屏 = 降低内容分辨率」，并以 P1 的 `RR3_MAX_RENDER_SCALE` 明示为**降级**。
7. **DPI/字号**：229 dpi 常数影响 UI 缩放；改 DPI 可能引发文字尺寸连锁变化。nfsmw 的经验是 `densityDpi=160` 固定即可用，另用**文本缩放旋钮**（`NFSMW_TEXT_SCALE` 默认 0.75，`nfsmw_font.c:322-344`）而不是改 DPI 来调字号——RR3 若字号不对，优先走这条，且与资产桶分开验证。
8. **截图判据**：fb 报 1280×1440 的双半幅问题必须先解决，否则后续所有像素判据不可信；差分工具依赖 PIL（`diff_frames.py`），构建容器里需确认可用。
9. **参考仓库的继承脚手架文档**（`input_bridge.cpp:465-475` 引用 Mass Effect 符号）不可直接采信。
10. **`newports/` vs `AGENTS.md:10` 的 `work/ports/`**：成文约定与实际已跟踪的 8 个移植目录冲突，需维护者裁决（本文按用户指定的 `newports/realracing3/` 执行，并在 ANALYSIS §6 记录冲突）。
11. 参考仓库自身声明：显示改动「需掌机复核」、5 档分辨率「未做新手柄测试」——本轮必须以实机证据替换这两条自述。

## 9. 接续点

静态取证已闭环（`logs/rr3-elf-armv7.json`、`logs/fmodex-elf-armv7.json`、`tools/test_arm_attributes.py`、ANALYSIS §1/§3/R1b/R4/R6），§3 的 1–6 阶段因此已是「实测」而非「预期」。
§4 第 1 项（迁入骨架）与实机首轮出画面（§10）已完成，**当前接续点 = §4 第 4 项**：R6 落地 + `onViewChangedJNI` 上报口径 A/B——本机面板只有 640×480 一档，所以这一步在**本机上只能证「口径来自实测、改前改后零回归」，不能证自适应成立**（§4 第 9 项已写明）。
紧随其后是第 5–6 项（`display_contract` + 按 §2.2 顺序接线）。**这些没出结论之前，R2/R3 的显示代码一律不动**：content 取面板值还是取设计值、以及偏移模型能否成立，完全依赖它们的答案。R1 不在此限制内（它没有引擎消费者），但仍排在接线顺序的最后。
当日午后的驱动更换复测（§11）**没有改变这个接续点**：同一产物在 vendor Mali 下同样真出画面，端口运行时代码零改动，因此下一步仍是 §4 第 4 项。
与显示主线并行的两个已知非显示阻塞（不挡出画面，挡可玩）：§8 之外的**存储根**（任务：`SD card is not mounted!` / `Failed creating cache/doc/res directory!`）与**缺失 Java 门面**（`Input`、`Fiksu`、`NimbleManager`、`LocalNotificationsCenter.<init>`、`setEnableTracking(Z)V`）。

## 10. 首次真实画面（2026-10-08，实测闭环）

**结论**：客机 `libRealRacing3.so` 在 ROCKNIX / RK3326（aarch64 主机 + armhf 加载器）上真正渲染出 3D 画面。判据是 `grim` 抓的合成器输出，不是 PID、不是 `exit 0`、不是声音：

| 证据 | 内容 |
| --- | --- |
| `logs/cap-3.png` / `cap-4.png` | Laguna Seca 赛道：两辆 911 GT3 RS、看台广告牌、路面与天空；两张之间相机位移 → 画面在**动** |
| `logs/attach-3.png` | RR3 标题页 + 「Game Controller Connected」提示 + 加载转轮 |
| `logs/attach-4.png` | Firemint splash（引擎自绘，非启动器位图） |
| `logs/run-5.txt` | `GLES1 table: 190/190 imports resolved (190 from the live driver, 0 from libmali.so.1, 0 answered by neither)`、`dispatch audit: 0 of 18`、`run finished: 400 frame(s)` `exit=0` |

截图与 `logs/` 同级但被 `.gitignore:30` 排除，**不得入库**（专有画面）。

### 10.1 根因：GL 派发层与当前上下文不同源（不是缺符号）

阻塞 SIGSEGV 的完整因果链，逐条实测：

1. 崩溃点在 `MainActivity.onViewCreatedJNI` 内部（加载器的 `onViewCreatedJNI returned` 从不打印；栈回溯最深 = `libRealRacing3.so+0x008b2964`，即 `ndActivity::onViewCreated` 调 `mtGLWrapper::Init(this,-1)` 的返回地址）。
2. `mtGLWrapper::Init`（+0x890b78）的自动探测分支（+0x890ea0）用 `glGetString(GL_VERSION)` 解析版本；**NULL ⇒ 版本成员留 0**。
3. `InitPointers`（+0x8971c8）头部 `cmp r1,#1; ble …`：版本 ≤1 走 GLES1 路径，**不会**把核心入口写进引擎自己的 515 个 `ptr_gl*` .bss 变量。
4. `wrapper_glGetIntegerv`（+0x892068）读 `*ptr_glGetIntegerv` 后 `blx r3` ⇒ `pc = 0x00000000`，与崩溃报告的 `pc=0 / lr=+0x892084` 完全吻合。
5. 为什么 `glGetString` 返回 NULL：本机的 EGL/GLES 上下文由 **Mesa/Panfrost** 拥有（`OpenGL ES 3.1 Mesa 26.0.4`），而 `symtable_gles1` 的 provider 链按「导出 `glMatrixMode`」采纳了 `libmali.so.1`。**导出名字 ≠ 拥有上下文**：那个 blob 没有可派发的当前上下文，`glGetString` 因此给 NULL。
6. 不是缺导入：全程 `Missing:` 告警 **0 条**；引擎向 `eglGetProcAddress` 索取的 251 个名字全是扩展名，没有核心名（`REALRACING3_EGLPROC_TRACE=1` 实测）。

**修复**（`port/thunks/khronos/gles1.cpp:135-172`）：GLES1 表改为**先问当前驱动**（`SDL_GL_GetProcAddress`），provider 的 `dlsym` 只作兜底。这样两张 GL 表持有同一批指针，派发层与上下文同源；在「vendor blob 就是驱动」的机器上 `SDL_GL_GetProcAddress` 返回的仍是该 blob，所以这个顺序两边都对，不需要按设备开关。原先这行为藏在 `REALRACING3_GL_SINGLE_DISPATCH=1` 的 A/B 旋钮里（`port/thunks/khronos/gles1.cpp:123-147` 旧注释），A/B 已跑完、结论已定，旋钮随之删除。

**顺带永久改进**：崩溃处理器改用 `process_vm_readv` 安全读（`port/src/crash.cpp`，此前寄存器 dump 到 `[r0] = ` 就截断）；`eglGetProcAddress` 未命中现在按名字打印（`port/android/egl_shim.cpp`）。

### 10.2 已证 / 未证

- 已证：加载→重定位→构造函数→JNI 生命周期→着色器编译→3D 场景上屏→多帧连续（637/1268 帧）→present 到面板。
- 未证：前端菜单逐项、玩法（阶段 13/17）、存档往返（15，且被 §9 的存储根问题挡住）、音频样本级实证（14）、性能基线（16）、**显示自适应**（11，本机只有 640×480 一档）。
- 仍不支持：`Tracks/Laguna_Seca/processed/seca.lights` 等载荷缺失（引擎自报 `Failed to load lights file`）、204 处 `Failed to obtain GL function pointer`（GLES2/3 名，扩展查询未答）、sway 全屏尝试时好时坏（`No matching node` ↔ `Successful, exiting`）。

## 11. 换驱动复测（2026-10-08 下午，vendor libmali）

所有者把测试机 32 位图形栈由 Mesa/Panfrost 换成 vendor Mali blob（为便于适配其他系统）。设备侧事实见 DEVICE §11；本节只记对移植主线的结论。

**结论一：出画面这件事与驱动无关，端口代码零改动。** 换驱动前后跑的是**同一个产物**：本地 `port/build/realracing3` 与设备 `/storage/roms/ports/realracing3/realracing3` 的 sha256 同为 `227104069febdff3…`（11752696 字节，与 inventory §3 一致），本轮没有重新编译。Mali 下依旧真出画面：

| 证据（端口自己的 `glReadPixels` 回读，非合成器） | 内容 | 日志 |
| --- | --- | --- |
| `logs/mali-game-cap-1.png` | Firemint splash，右下角加载转轮 | `mali-run-3` 前一次运行 |
| `logs/mali-cap-b-1.png` | RR3 标题页 + `Game Controller Connected / Wheel B control scheme still active` | `mali-run-3.txt:5897-5899`（`capture source: framebuffer=0`、`nonblack=307200/307200`） |
| `logs/mali-cap-b-3.png` | Laguna Seca 第一人称座舱：方向盘、仪表、前车、看台、教学气泡"Press the left analog stick left to steer left" | `mali-run-3.txt:14629-14631`（`nonblack=305648/307200`） |
| `logs/mali-cap-c-2.png` | 同一 splash 的**半暗**帧（`max_rgb_sum=325`） | `mali-run-4.txt:4970-4971` |

**结论二：本机主判据换人。** `grim` 在 vendor blob 下 4 张全黑（direct scanout 绕过合成，DEVICE §11），而端口自己的 `rr3_control` 回读在**两个驱动下都出图**。因此以后"有没有画面"以回读为准，`grim` 只在它给得出图时作旁证。这条通路此前从未在任何运行里启用（缺 `REALRACING3_CONTROL_DIR`），现已由 `tools/device_capture.py --source game` 默认导出——**换判据不需要改端口一行代码，只需要把已有的开关打开**。
配套教训：整屏黑不等于无画面。回读的三张连起来是 亮 splash(765) → 半暗(325) → 全黑(0，但 alpha 全 255) → 标题页，即引擎自己的淡入淡出；判黑必须同时看 alpha 与前后帧。

**结论三：R8 的修复是驱动无关的，这是第二次独立验证。** Mali 下 `GL provider: libmali.so.1` 被采纳，而 GLES1 表 190 个指针**全部**来自活驱动、0 个来自该 provider 的 `dlsym`（`mali-run-3.txt:33-34`），`dispatch audit: 0 of 18`。"先问当前上下文的所有者"在两种栈上都同源，所以不需要按设备开关（PLAN §10.1 的预判成立）。

**结论四（喂给 R4 的运行时实证）：引擎按驱动 advertised 的扩展自选纹理格式家族。** 同一产物、同一载荷：

| | Mesa/Panfrost | vendor Mali |
| --- | --- | --- |
| 压缩上传 | `ETC1_RGB8` 2651 次 + `S3TC_DXT1_RGBA` 2202 + `S3TC_DXT5` 29（`run-6.txt`） | `ETC1_RGB8` 4775 次，**S3TC 0 次**（`mali-run-3.txt:14757`） |
| 端口的 DXT 兜底通路 | 进入并打印 `S3TC: native driver support - DXT1 present, DXT3/DXT5 present; software fallback disabled` | **一条都没有**（该函数在首次 DXT 上传时才惰性探测，说明引擎根本没发过 DXT） |
| `dropped` | 0 | 0 |

即：驱动不广告 S3TC，引擎就不发 DXT，改发 ETC1，两边都 `dropped=0`。这是"把格式选择权交回引擎"能成立的直接证据，与 R4/R7（`assets_%dx%d/` 桶选择同理）方向一致。

**性能（不作受控 A/B，只作记录）**：帧预算随场景变化极大，两次运行的场景序列不同，因此不能把 fps 差归因于驱动。可归因于驱动的是常量项：`swap` 由 Mesa 的 1.7–11.2 ms 降到 Mali 的 0.6–0.9 ms；GLES 版本由 ES 3.1 升到 ES 3.2；`identity` 由 `Mesa / Mali-G31 MC1 (Panfrost)` 变为 `ARM / Mali-G31`。末窗 fps：Mesa `run-6` 5.5–6.0，Mali `run-3` 12.7–14.1（该次 `run finished: 1611 frame(s)` / 145 s ≈ 11.1 fps 均值，`exit=0`）。**性能基线（阶段 16）仍未立**：它要求固定场景序列，本轮没有。

**未因换驱动而减少的东西（逐条两边同数）**：`Failed to obtain GL function pointer` 204 处、`Failed to load resource file` 92 处、`SD card is not mounted!` 1 处、`Failed creating … directory` 3 处、`Missing:` 0 处。⇒ §9 的存储根与载荷问题、以及 GLES2/3 名未答的缺口，都是**驱动之外**的账，仍按 §4 第 4 项往下的顺序处理。

**本轮改了什么**：只有 `tools/device_capture.py`（新增 `--source game`，启动时导出 `REALRACING3_CONTROL_DIR`）与启动器一句措辞（`port/ports/Real Racing 3.sh:800-810`，旧句 `no Mali blob` 在新驱动下为假，已复验为 `no blob tier chosen`，`mali-run-4.txt:13`）。**端口运行时代码一行未动。**
**编译了什么**：无（产物沿用 §10 的那一次）。
**测了什么**：见上表四张回读图 + 三次运行日志（`mali-run-1/2/3/4.txt`）。
**没测什么**：`grim` 在新驱动下是否存在任何可抓到画面的窗口态（未穷举）；ES 3.2 带来的新能力是否被引擎用到（引擎仍只走 GLES1 表）；音频样本级实证。
**仍不支持**：与 §10.2 相同，无新增，也无消除。
