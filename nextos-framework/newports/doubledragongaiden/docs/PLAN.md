# Double Dragon Gaiden — 推进计划（对齐 17 阶段）

> 路线已定（docs/ANALYSIS.md §5）：复制 60parsecs port 骨架并适配。
> 铁律：不跨阶段宣布成功；一次只改一个变量；每步 编译→测试→看退出码→看日志。

## 阶段状态

| # | 阶段 | 状态 | 证据 |
|---|------|------|------|
| 1 | 确认 Unity 版本 | ✅ 2021.3.6f1 / IL2CPP / metadata v29 | ANALYSIS §1 |
| 2 | 确认架构 | ✅ arm64-v8a（另有 armeabi-v7a 不用） | ANALYSIS §1 |
| 3 | 分析 libmain.so | ✅ 10KB split loader，仅 dlopen + JNI_OnLoad | ANALYSIS §2 |
| 4 | 分析 libunity.so | ✅ NEEDED 面 + 81 个非 libc 未定义 + Swappy 导出 | ANALYSIS §2 |
| 5 | 分析其他 *.so | ✅ 15 个全表；装载决策：xlua ✅ / tapsdk ⏸ / 其余 ❌ | ANALYSIS §2 |
| 6 | Java/JNI 依赖 | ✅ 包名/Activity/TapSDK/cabbage/WebView 清单 | ANALYSIS §1 |
| 7 | 选最近案例 | ✅ 60parsecs 骨架 + greak 系降级件备用 | ANALYSIS §5 |
| 8 | 建立最小 loader | ⏳ 本轮：复制骨架、摘 FMOD、换身份 | |
| 9 | native 成功加载 | ⏳ | |
| 10 | Unity 初始化 | ⏳ initJni 签名以 RegisterNatives 日志为准（2021.3 疑为单参 Context） | |
| 11 | 创建窗口 | ⏳ Mali-G52 GLES 3.2 + KMSDRM | |
| 12 | 显示画面 | ⏳ | |
| 13 | 输入 | ⏳ | |
| 14 | 音频 | ⏳ 首跑允许静音（no-op 音频门面），画面后按 dlopen 证据接真实通路 | |
| 15 | 存档 | ⏳ home/ 布局沿用 60parsecs | |
| 16 | 性能 | ⏳ | |
| 17 | 可玩 | ⏳ | |

## 实机运行记录（证据 → 动作）

### run01（2026-10-03，45s，SF_JNILOG=1）
- **全链路通了**：4 模块装载重定位（libmain 0 / libunity 7 / libil2cpp 3 / libxlua 15 未解析），
  `JNI_OnLoad(libmain)->0x10006`，`NativeLoader.load` 完成，**`initJni` 实测为 2021.3 单参
  `initJni(Landroid/content/Context;)V`**（调用点四参 ABI 安全），UnityMain 起线程，
  surface/focus/resume 全 OK，**nativeRender 60fps 空转**，Mali-G52 GLES 3.2 上下文建立
  （GL 扩展串实证），游戏自有框架跑起来：`CoreMain.Init/Init2`、`HttpClientMgr`、
  `PackageMgr.LoadFullResVerionDb`、`CabbageSdkMgr`、`VideoPlayerMgr`。
- **游戏真实身份修正**：Unity 自报 `Company Name: yoogames-hd1 / Product Name:
  完蛋！我被美女包围了！`（FMV 视频驱动游戏）——APK 目录名 doubledragongaiden 仅为
  工程代号；**LocalVideos 487MB 是核心内容**（后续必须部署）。Unity 运行时实测
  2021.3.36f1（数据文件头为 2021.3.6f1，不阻塞启动）。
- 21 个强未解析（libunity nearbyintf/execl/kill/perror/waitpid/fork；
  il2cpp tc*；xlua isgraph/iscntrl/inet_*/__stack_chk_guard 等）→ 已补进 bionic 表
  （run02 归零，剩 4 weak 无害）。`__stack_chk_guard` 直接引 glibc 全局 canary。
- 包名泄漏：jni.c 答 60parsecs 包名 → 改 `com.intiny.hd1.android`。

### run02/03（截图取证）
- 截图机制语义：`SF_SCREENSHOT_LIVE=1` 只认 `/tmp/bcshot` touch（FRAME 被忽略），
  每次 live shot 写 `<path>.<seq>`。
- **画面近黑**（nonblack=454/786432, alpha-zero, verdict=INCONCLUSIVE）。
- 根因（run01 日志实锤）：游戏经 UnityWebRequest 读
  `jar:file://!/assets/Datas/app.iex`、`res_version.iex`、`channel.iex`、
  `android_files.csv`、语言表 → **全 404**（sourceDir/dataPath 为空，
  Unity 内部 jar 读取器无 APK 可开，无 libc open 痕迹 → fp2 式 open() 翻译不适用）。
- **对照 ports/ 成品案例**（维护者指示）：fp2(2018.4) 用 open() 层翻译 jar: URL
  （因 2018 的 jar 串落 libc）；horizonchase/huntdown（Cuphead 壳）用 il2cpp 方法
  hook 改写 get_streamingAssetsPath（版本偏移绑定）；60parsecs(Unity 6) 用
  **store-zip game.apk + getAssetPackPath 返回真 zip**（Unity 内部 zip 读取器消费）。
  本作 2021.3 的消费方与 60parsecs 相同 → 采用 store-zip 路线：
  `tools/make_store_apk.py` 打 `game.apk`（全 assets 树，ZIP_STORED），
  `ApplicationInfo.sourceDir/publicSourceDir → asset_pack_dir()`，
  bionic.c `sf_store_zip_path` 把 `<gamedir>/game.apk/assets/X` 文件路径重写回
  `<gamedir>/assets/X`（YCore 的直读路径两不误）。

### run04/05（store-zip 落地）
- run04：dataPath 仍空 → 证据链指向 `getPackageCodePath`：Unity 用它作 APK 校验
  （zip 可开才有 dataPath），目录形态直接落空。
- run05：`getPackageCodePath → asset_pack_dir()`（store-zip game.apk）后：
  **dataPath=/roms/ports/doubledragongaiden/game.apk、streamingAssetsPath=
  jar:file://<gamedir>/game.apk!/assets、404 归零、`LoadFullResVerionDb Finished`、
  `m_package_resource_version:320`、`_curPackageType: Full`**。
  日志实证 libunity `fopen(game.apk,"rb")` 双句柄 = 内部 zip 读取器在读 store-zip。
  （此前 stat(game.apk/assets/...)=-1 是 Unity 探测"目录形态 APK"，-1 才是期望答案。）

### run06/07（视频假设）
- 流程止于 `VideoPlayerMgr！Init` → `UnityPlayer.hidePreservedContent()`（开屏结束）
  之后**再无任何请求**：无视频文件 open、无 AVPro P/Invoke、无 TapSDK 类请求、
  无 slCreateEngine（音频初始化从未启动；getDevices 每帧轮询只是设备监视）。
- 线程活体探测（35s）：UnityMain 活跃（utime≈2s），无死锁，mali 线程在位 ——
  渲染循环在跑，内容为黑。
- **假设**：开场视频经 jar: URL 读 store-zip，而首版 zip 不含 LocalVideos
  （缺条目静默失败）。行动：`make_store_apk.py --with-videos` 重打全量 zip。
- 另发现：60parsecs 血统注入 `-force-gles20`（GLES2 后端）——保留观察，
  若 zip 补齐后仍黑再试 `SF_NO_FORCE_GLES20=1`（代码自带旁路）。

### run11-13（重写 bug 修复 + 配置破译 + 假 CDN）
- **store-zip 路径重写是指针 bug**（on-device HIT 日志暴露：输入==输出，恒等重写）——
  head 在推进 rest 之后才计算。修复后 `<zip>/assets/X` 直读路径真正打通。
- **厂商配置全家桶 = XOR-0x09**：app.iex / channel.iex / fgui_pkgs.csv / res_version.iex
  全部可解。app.iex 关键项：UPDATE_URL=http://10.0.0.249/cdn/hd1_neiwang/（内网，不可达）、
  VIDEO_CDN_ROOT=https://hd1-cdn.xiaoyoujuchang.com/webmp4/（**实测可达**）、
  LOGIN/GAME_IP_PORT=hd1taptap-*-trial.xiaoyoujuchang.com、GAME_SDK=CabbageAndroid。
- 设备有 python3.11 → 起 `python3 -m http.server 8080` 假 CDN；
  LocalFile 覆盖层 app.iex 把 UPDATE_URL 指到 127.0.0.1:8080（**覆盖层配置生效实锤**）。
  投喂 XOR 编码的 `RESOURCE_VERSION=320` → 游戏发起 GET 200（run12/13 实证），
  但 m_remote_resource_version 仍打 0（打印发生在异步 fetch 完成前），且无后续下载 →
  更新检查不是阻塞点。

### run14-16（GetSdkParam 桥 + 托管栈诊断）
- 新缺口：游戏经反射调 **MainActivity.GetSdkParam(key)** 拿 SDK 配置 → 原先回空串。
  jni.c 新增 j_GetSdkParam：首用时解密 app.iex（LocalFile 覆盖层优先）+ 内置
  thirdChannelID=17 / thirdChannelFlag=huchi / IsFirstAuthorizationSuccess=true。
  实测三个 key 均正确应答，**流程仍未越过 VideoPlayerMgr！Init** → 该桥也非唯一阻塞点。
- main.c 新增 **SIGUSR1 托管栈转储**（kill -USR1 <pid>，转发到 UnityMain）：
  原生栈 97% 在 Swappy 等 vsync（libunity+0x6292c0 → pthread_cond_wait，健康帧节奏），
  偶发 libil2cpp 帧（+0xb64334，游戏逻辑每帧一瞬）；并接入
  **il2cpp_format_stack_trace()**（libil2cpp 导出）——下一次触发将给出精确 C#/Lua 卡点。

### run14-17（2026-10-04 凌晨）：卡点定性为死锁 + 三件新取证工具
- **10 分钟 soak（run14）**：18300 帧、60fps、Unity 日志恒 29 行停在
  `VideoPlayerMgr！Init` → **死锁，不是慢**。
- **排除项（逐条实测）**：GLES2 强制注入（run15 `SF_NO_FORCE_GLES20=1` 同样卡）、
  网络等待（假 CDN 零请求、无 pending socket）、JNI/SDK 回调（`SF_JNILOG=1` 全量
  无异常；`runOnUiThread`/`Handler.post` 已 inline 执行）、AVPro/MediaPlayer（无
  dlopen、无 Java 请求）、引擎线程卡死（SIGUSR2 全线程转储：38 线程均空闲等待）。
- **新工具 1 `SF_TRACE_INVOKE=1`**（`il2cpp_trace.c`，dlsym 层包裹
  `il2cpp_runtime_invoke`）：过滤引擎噪声后，卡住期只有 5 个方法在跑，
  `<Start>d__10::MoveNext` 仅 3 次 → 协程挂在**纯引擎侧 awaitable** 上。
- **新工具 2 `SF_DUMP_METHODS=<path>`**：运行时反射导出 12012 条
  `ns.class::method @ addr`（`logs/methods.txt`），供宿主机反汇编。
- **新工具 3 `kill -USR2 <pid>`**：全线程内核态转储（comm/state/utime/syscall）。
- **结构发现**：`VideoPlayerMgr` 无 `Init` 方法（日志来自基类 `MonoSingleton`）；
  `InitBaseSys` 是顺序 AddComponent 链 → 卡点更可能在**下一个 Mgr**
  （GameResMgr / GameLuaMgr / FairyGUIMgr 之一）。

## 当前状态与下一步（接续点）

**已实证**：装载→重定位→JNI→initJni→UnityMain→60fps 渲染循环全通；配置链全通
（假 CDN + GetSdkParam 桥）；画面黑，游戏逻辑在 VideoPlayerMgr！Init 后静默。

### 2026-10-03 深夜追加：托管栈转储实测结果
- `il2cpp_format_stack_trace()` 在信号处理器内调用**会段错误**（libil2cpp+0xb7b6d8，
  帧遍历撞上 vsync 等待期的原生 cond_wait 栈链）——第一次实现因此把游戏带崩
  （fault handler `_exit(2)`）。
- 已加 **sigsetjmp/siglongjmp 护栏**（sf_trace_segv_trampoline）：walker 崩溃只损失
  本次 trace，游戏存活（BuildID 6e204a61，实测 2/2 次安全恢复）。
- 随机 SIGUSR1 采样 40 次：97% 命中 Swappy vsync 等待（libunity+0x6292c0 →
  pthread_cond_wait，健康帧节奏），1 次命中 libil2cpp+0xb64334（游戏代码执行窗口）。
  **等待期栈链不可走，必须采样命中游戏代码窗口 managed trace 才能出名字。**

**下一步（按优先级）**：
1. 高频采样：`tools/tmp_stackburst.sh`（100×0.05s）已备好未跑——命中游戏代码窗口时
   managed trace 应能给出 C#/Lua 方法名；多采几轮收集不同 libil2cpp 帧偏移。
2. 依据卡点对症：若在等视频 → 接 UnityEngine.Video/AVPro 通路（VIDEO_CDN_ROOT
   hd1-cdn.xiaoyoujuchang.com 可达，注意 HTTPS 证书链）；若在等 SDK 回调 →
   按 60parsecs defer_proxy_invoke 模式补回调；若在 StartHotfix → 本地假 CDN 已就绪。
3. 音频通路（OpenSL ES 门面真实实现）在画面点亮之后。

### 环境与纪律

### P1 — 画面与初始化
- [ ] initJni/UnityPlayer 2021.3 契约对齐
- [ ] xLua 初始化 + lua_org_*.ab 装载（黑屏阻塞点候选 #1）
- [ ] TapSDK 桩策略按日志补（候选阻塞点 #2）
- [ ] 截图验证非黑帧

### P2 — 输入/音频/存档/性能
- [ ] 输入逐项验证（继承 GPTK 通路）
- [ ] 音频真实通路（按 libunity dlopen 证据：OpenSL/AAudio/AudioTrack）
- [ ] 存档跨启动
- [ ] 1GB 内存监控（[sf/mem] + [sf/gc]）
- [ ] AVPro 视频通路（若剧情强依赖）

## 环境与纪律
- 设备：192.168.31.218（ee47），部署 `.new`+`mv` 原子替换 + sha256；先确认进程归零
- 布局：资源 `roms/ports/doubledragongaiden/`，脚本 `roms/ports_scripts/Double Dragon Gaiden.sh`
- 编译：Docker `portmaster-builder:aarch64-latest`（build.sh 已有模板）
- 代码保存：`newports/doubledragongaiden/`（用户指定，不落 work/）
- 推送：明确说"提交/推送"才动 git
