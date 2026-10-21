# Beastie Bay — 推进计划（对齐 17 阶段）

> 路线已定（ANALYSIS.md §5）：复制 ddgaiden port 骨架（60parsecs 血统）适配 Kairosoft/2018.4。
> 铁律：不跨阶段宣布成功；一次只改一个变量；每步 编译→测试→看退出码→看日志。

## 阶段状态

| # | 阶段 | 状态 | 证据 |
|---|------|------|------|
| 1 | 确认 Unity 版本 | ✅ 2018.4.24f1 / IL2CPP / metadata v24 | ANALYSIS §1 |
| 2 | 确认架构 | ✅ arm64-v8a only | ANALYSIS §1 |
| 3 | 分析 libmain.so | ✅ 6KB split loader, NEEDED 含 libstdc++ | ANALYSIS §2 |
| 4 | 分析 libunity.so | ✅ 直链 libGLESv2/libmain; 无 Swappy; ALooper/ANativeWindow/ASensor/egl 面 | ANALYSIS §2 |
| 5 | 分析其他 *.so | ✅ 仅 3 个, 零插件 | ANALYSIS §2 |
| 6 | Java/JNI 依赖 | ✅ 经典 UnityPlayerActivity 面 + kairo 插件 + 广告中性化 | ANALYSIS §1 |
| 7 | 选最近案例 | ✅ ddgaiden 骨架 + fp2 同代实证 + broforce 老代实证 | ANALYSIS §5 |
| 8 | 建立最小 loader | ⏳ 本轮 | |
| 9 | native 成功加载 | ⏳ | |
| 10 | Unity 初始化 | ⏳ | |
| 11 | 创建窗口 | ⏳ Mali-G31 GLES + KMSDRM 720×720 | |
| 12 | 显示画面 | ⏳ | |
| 13 | 输入 | ⏳ | |
| 14 | 音频 | ⏳ 首跑允许静音 | |
| 15 | 存档 | ⏳ | |
| 16 | 性能 | ⏳ | |
| 17 | 可玩 | ⏳ | |

## 本轮推进清单（P0）

- [ ] 复制 ddgaiden/port → beastiebay/port,摘除 ddgaiden 专属(xlua/TapSDK/GetSdkParam/XOR-0x09)
- [ ] 换身份: 包名 net.kairosoft.android.mujin_en、gamedir、启动脚本、build OUTPUT=beastiebay-nextos
- [ ] ArkOS 单目录布局: 启动脚本与游戏同在 /roms/ports/（无 ports_scripts）
- [ ] 编译过门禁(GLIBC ≤2.30 —— ArkOS 恰为 2.30)
- [ ] 部署: /roms/ports/beastiebay/{lib,assets,home} + /roms/ports/Beastie Bay.sh
- [ ] 首跑收集日志 → 按日志迭代

## 实机运行记录（ArkOS，2026-10-04）

### run1-2（骨架首跑）
- fp2 骨架一次编译通过（GLIBC 2.29 ≤ 2.30，NEEDED 基线净）。
- **GLES 3.2 (Mali-G31) + KMSDRM 720×720 实证**；模块装载 8 未解析（后查为已宣告 weak 类）。
- FATAL #1：fp2 的 mobile-controls 二进制补丁守卫 fail-closed（FP2 专属 RVA）→
  beastiebay 早退跳过补丁（该补丁对本作无意义）。
- FATAL #2：`NEXTOSCONTROLLERS 缺失/无效`（输入 fail-closed 门禁）→
  写 `defaults/NEXTOSCONTROLLERS.gptk`。格式实测：fp2 代解析器只认
  **NEXTOS_CONTROLLERS/1-3 魔数**（/4 是新代）、/2 起要求每 context 全按键声明、
  `native` 按钮值需 /2；**fp2.move 只有向量 sink,十字键必须 native**。
  修正后：**sinks=8 密封通过，GO-Super 手柄识别，输入门禁放行**。

### run4-10（Unity 起来后的两次退出链）
- run4：nativeRender 循环跑了，但 `Unity requested render-loop stop at frame 5` +
  **NullReferenceException 链**：TapjoyUnity ApiBindingAndroid ctor →
  `java/lang/Class.forName(String)` 反射路径 unhandled → null。
  修复：绑定 Class.forName（**必须同时发布 Class 返回型与 Object 擦除返回型两个签名**）。
- run10：NRE 清零，但仍在 frame 5 quit。FP2_MISSLOG 实锤：
  `Settings$Secure.ANDROID_ID 静态字段 → 0(null)` → getString(null) → ""
  → 空设备 ID → Kairosoft `CheckInitOrFinish`（元数据字面名）判定失败 →
  `plugin/Config.finish()`（miss log 实证）→ Application.Quit。
  修复：绑定 ANDROID_ID 字段返回 "android_id"、getInstallerPackageName 返回
  "com.android.vending"、启用 FP2_NONNULL_OBJECT_FALLBACK（未处理对象返回给活桩）。
- 截图机制：FP2_SCREENSHOT + FP2_SCREENSHOT_FRAME（帧号）。**尚未拿到非黑截图**——
  frame-5 quit 先于截图帧。两个修复(ANDROID_ID 等)已编译未上机验证（设备掉线）。

### 下一步（接续点）
1. 部署最新二进制（含 ANDROID_ID/getInstallerPackageName 绑定），跑 bb_cycle，
   看 quit 是否消失、frame 900 截图是否产出。
2. 若 quit 仍在：FP2_MISSLOG 找 frame 1-5 之间的新 miss；重点怀疑
   Tapjoy connect 回调（onConnectSuccess 永不触发 → Kairosoft 超时 quit——
   但 frame 5 即 quit,更像同步判定）。
3. 画面出来后：输入逐项、音频（FMOD AudioTrack 泵已在跑）、存档、性能。

### 2026-10-04 深夜：NRE/quit 修复链（run12-17）
每轮修复揭示下一层，游戏启动逐次深入：

| 修复 | 根因（run 实测） | 结果 |
|------|----------------|------|
| `mk_class` NULL 防护 | FindClass(NULL) 沿 null-metaData 反射链到达 | strcmp(NULL) 死 |
| `method_id` NULL 防护 + 循环防御 | 反射链送 null 成员名 | strcmp(NULL) 死（帧内 SIGSEGV, 自带 handler 转 Error→quit）|
| **fallback 跳过字段型 sig** | 静态字段（无括号描述符）全走不到 fallback → POWER_SERVICE 等 → null → Unity 内 index(NULL) | **本轮修复，SIGSEGV 清零** |
| getPreferenceKeys 非空 | LINQ Contains(null) → ArgumentNullException 对话框（帧 2）| 已过 |
| PackageInfo/Bundle 字段 | metaData=null → 反射链空参 | 已过 |
| Class.forName 双签名 | 反射路径返回 Object 擦除型 | 已过 |
| setParam/getParam 键值存储 | 参数丢失（getParam→""）| 已过 |
| DisplayMetrics/Display/Point 绑定 | 显示指标全 0 → "Texture has out of range" | 警告仍在，但非直接死因 |
| getScaleRatio 替换零参数 | (240,240,0,0) → 纹理 0×0；现 (240,240,720,720)→3.0 | 已过 |
| getInstallerPackageName/ANDROID_ID | Kairosoft CheckInitOrFinish 许可链 | 已过 |

### 当前状态（接续点）

- **SIGSEGV 已清零**（字段 fallback 修复后）。渲染循环可跑完整 50s 周期。
- 剩余阻塞：C# 启动期仍捕获一个异常（Kairosoft 错误码 100）→ IApplication.Error →
  `showDialog(id=0/3, "Error")`（英文+日文错误框，含"请发送错误信息"文案）→
  游戏调用 Application.Quit → `frame 6 keep=0`。
- 诊断线索：错误框紧跟 `getScaleRatio(240,240,720,720)→3.0` 与
  `getParam("ADMOB_SCREEN_HEIGHT")→"720"`（已试覆盖 "0"，quit 提前到 frame 3，
  异常源不同轮表现有波动——时序敏感）。
- 下一层（本轮未验证完）：
  1. `Utility.fullscreen(true)` 之后的纹理创建（"out of range" 是 Unity 警告；
     具体非法尺寸未抓到——需要 FP2_GLLOG 下的 glTexImage2D 前序）。
  2. TapjoyConnectUnity.connect 之后 SDK 回调永不触发 → Kairosoft 可能在等
     Tapjoy 初始化结果（code 100 = 通信/初始化失败候选）。
  3. 若上述无果：考虑绑定 Tapjoy connect 的 success 回调对象（defer_proxy_invoke
     模式），或找 Kairosoft 错误码 100 的元数据定义。

### 环境与纪律（ArkOS 版）
- 设备：192.168.31.196（ark/ark，sudo 同密码）；网络极不稳（每几分钟掉线），SSH 全部带重试
- 工具脚本：/tmp/bb_launch.sh、/tmp/bb_stop.sh、/tmp/bb_cycle.sh（设备端自包含轮）
- 部署：sftp rename 拒绝覆盖 → 先 remove 再 rename（deploy_game.py 已处理）
- 本机远程执行：`tools/run_remote.py`（paramiko，地址只从 `DEVICE_HOST` 环境变量取，
  绝不硬编码；带 4 次重连）。解释器必须用
  `C:/Users/windstarry/.workbuddy/binaries/python/envs/sshtool/Scripts/python.exe`
  （默认 python 无 paramiko），Git Bash 侧加 `MSYS_NO_PATHCONV=1`。

## 2026-10-05：方法表并发写 ⇒ frame 3 SIGSEGV（错误 100 之前就先崩）

### 实测事实（08:43 那轮，sha256 234c02d5…，与本机二进制一致）
- **不是错误 100 的 dialog quit**：`keep=0` 前一行是
  `[D/CRASH] signal 11 (SIGSEGV), code 1 (SEGV_MAPERR), fault addr 0x0`，
  崩在 **frame 3**。之前"错误 100 → showDialog → Quit"的结论只对更早的轮次成立，
  08:39 之后的版本先被这个崩溃挡住，错误 100 暂时观察不到。
- `frame 1 keep=1` / `frame 2 keep=1`，Unity 起来、SharedPreferences 读写、
  asset stat 全部正常，语言包 `_language_pack_match=1` 也已落盘。
- 崩溃前 5 行就是 Choreographer 起链：
  `HandlerThread.start("UnityChoreographer")` → `Message.sendToTarget what=1` →
  `handleMessage(what=1)`。

### 定位方法（可复用）
1. `on_fault` 没打出来——**Unity 自己的崩溃处理器抢在前面**（输出 `[D/CRASH] *** ***`
   头），所以只能拿到它的 backtrace：两帧，`#00 [vdso]` + `#01 libc (index+108)`。
2. `index` 是 glibc 里 `strchr` 的旧别名（`__strchr` 家族导出名），于是
   **frame #01 就是 strchr 自身**，参数直接取自 `x0/x1`：
   `x0=0x0000000000000000`、`x1=0x28`（ASCII `'('`）⇒ **`strchr(NULL, '(')`**。
3. `x30(LR)=0x555584102c` 落在我们自己的 loader 正文 ⇒ 是**我们的代码**在解引用
   NULL，不是 libil2cpp。`dladdr` 对该 pc 返回 `(none)` 只是因为 pc 落在 libc，
   而 libc 不在 `nx_mod` 表里。
4. 本机 grep 全部 `strchr`：只有 `jni.c` 的 `strchr(..., '(')` 两处，且
   `x1='('` 与之精确吻合 ⇒ 锁定 `registry_member_kind()`（原 jni.c:478）与
   `dispatch()` 的字段判定（原 jni.c:3801）。

### 根因：methods[] / method_n 的无锁读-改-写
`method_id()` 原来直接 `for (i<method_n) …; methods[method_n]…; ++method_n;`，
**全程没有锁**。而调用方跨线程：
- `fp2_choreographer_start()` 在 **UnityChoreographer HandlerThread** 里注册
  `doframe_mid` / `handlemsg_mid` / `run_mid` / 回调 mid；
- 主线程同时在注册 `Message.what`（原 4272）与反射代理的 4 个 mid
  （原 4369/4378/4388）、以及每个 `GetMethodID` / `GetStaticFieldID`。

两线程读到同一个 `method_n = n`，各写 `methods[n]` 并各自 `++method_n`
⇒ 表尾 `method_n == n+2`，而 **`methods[n+1]` 从未被初始化**（`sig == NULL`）。
`by_id()` 只校验 `1 <= i <= method_n`，照样把这个空槽发出去；
`registry_member_kind()` 随即 `strchr(NULL, '(')` → SEGV_MAPERR @ 0x0。
`fp2_jni_bind()` 还有第二个窗口：先 `method_id()` 发布槽位、再单独写 `.handler`，
期间该槽会以 `handler == NULL` 被别的线程 dispatch 掉。
日志里被撕裂的 `[fp2] jni: boxed java/lang/Boolean = 1jni: new method id 445 …`
就是两线程同时写 stderr 的直接证据。

### 修复（本轮，一次改完不试错）
- 新增 `method_table_lock`（`PTHREAD_MUTEX_INITIALIZER`），把
  **查重 + 占槽 + 写四字段 + 递增 `method_n` 整段**包进临界区，
  槽位只有在四字段齐备后才可见。
- 拆出 `method_id_locked()`（假定已持锁），使 `fp2_jni_bind()` 能在**同一把锁内**
  完成"查/建 + 写 handler"，消掉 handler 的发布窗口。
- 防御纵深（即便锁再次失效也不该死在 libc 里）：
  `registry_member_kind()` 增 `!entry->name || !entry->sig` 守卫；
  `dispatch()` 入口增"槽位不完整"fail-closed 并打日志；
  `strchr(m->sig,'(')` 改判 `m->sig &&`；
  `NewObjectUTF`/`GetStringUTFChars` 风格的两处 `strcmp(m->sig, …)` 改判
  `m->sig &&`；`ToReflectedMethod` 的 `strcmp(entry->name, …)` 改判
  `entry->name &&`。

### 接续点
1. 编译 → 部署 → 跑 `bb_cycle`，确认 **frame 3 的 SIGSEGV 消失**（`keep=1` 继续推进）。
2. 崩溃清零后才重新观察 Kairosoft 错误码 100 / showDialog 是否重现——**两者是
   先后关系，不是一次性结论**；若重现，再按上面原三条候选（纹理尺寸 /
   Tapjoy connect 回调 / 错误码 100 元数据定义）推进。
3. 画面出来后才进入输入/音频/存档/性能（阶段 13-17）。

## run18：方法表锁修复已实测通过，错误 100 定位到 TimeZone

### 第一步：方法表加锁 ⇒ SIGSEGV 清零（sha256 904ffccf…，BuildID b0eec321…）
- 部署后 50s 轮实测：`signal 11 / SEGV_MAPERR` **计数 0**，
  渲染循环 frame 1→**frame 6**（修复前 frame 3 即崩）。
- 新增的 fail-closed 日志 `incomplete method id` **计数 0** ⇒ 空槽再未出现，
  证明锁真正消除了根因（而不是碰巧没触发）。方法表高水位 466 条。
- 编译门禁：GLIBC max 2.29 ≤ 2.30，NEEDED 基线净。

### 第二步：错误 100 的真实触发点 = java.util.TimeZone 缺失
崩溃清零后，`frame 6 keep=0` 前紧跟两条真实错误框（不再是猜测）：
```
Utility.showDialog(id=0, title="Error", msg="An error has occurred.\n100")
Utility.showDialog(id=3, ..., "エラーが発生しました\n問題解決の為にエラー情報を
                              送信して頂けませんか？\n（※個人情報は送信されません）")
```
⇒ **Kairosoft 错误码 100**（消息体第二行就是字面量 `100`），随后
`Application.Quit`。这与 PLAN 早先记录的"错误 100 → showDialog → Quit"一致，
只是先前被 frame 3 的竞态崩溃挡在前面，从未真正跑到这一步。

**决定性证据链**（日志 + IL2CPP 元数据双向印证）：
1. 设备日志（崩溃消失后可见）紧邻 showDialog 之前：
   ```
   ReflectionHelper.getMethodID(java/util/TimeZone.getTimeZone,
                                (Ljava/lang/String;)Ljava/lang/Object;)
   → unhandled … -> stub java/lang/Object          ← 类名退化
   ReflectionHelper.getMethodID(java/lang/Object.getRawOffset, ()I, static=0)
   → unhandled java/lang/Object.getRawOffset()I -> 0
   → [fp2/miss] ����.getRawOffset ()I -> 0        ← 乱码类名，裸 Object
   ```
   `getTimeZone` 落进通用 stub，`getRawOffset` 打在**裸 java/lang/Object** 上，
   时区偏移拿不到有效值。
2. 元数据 `global-metadata.dat`（`assets/bin/Data/Managed/Metadata/`，
   4.2MB，字符串为 **UTF-8** 而非 UTF-16——按 UTF-16 扫会全部 0 命中，勿走弯路）
   偏移 `0x3de80` 的成员表把两者并列在 KairoLibrary 段里：
   ```
   … Custom Keyboard Change! [A] | showInputPanel | startInputPanel |
   putPreference | existPreference | getPreference |
   getTimeZone | getRawOffset | GameOrderId | UDP …
   ```
   ⇒ 确属 Kairosoft Utility 插件的成对封装，不是引擎侧需求。
3. `getRawOffset` 在元数据里 0 命中 ⇒ C# 调的是 Java 侧方法本身，
   我们必须提供真实的 `java/util/TimeZone` 对象而非通用 stub。

### 本轮修复（TimeZone 族）——⚠️ 假设已被实测推翻，保留记录
新增 `j_TimeZone_*` 适配器并注册：`getDefault`（两种返回类型）、`getTimeZone`（两种
返回类型）、`getRawOffset`、`getOffset`、`getID`、`getDisplayName`、
`useDaylightTime`、`inDaylightTime`，以及 Kairosoft 包装
`Utility.getTimeZone` / `Utility.getRawOffset` 转发。设备为 UTC，offset 0 是诚实值。

**实测结果：绑定确实命中（`getTimeZone("") -> UTC`、`getRawOffset() -> 0`），
但错误 100 依旧，且 `showDialog` 出现在 line 1802，早于 TimeZone 调用 line 1808
—— 因果顺序反了。**

⇒ **教训：不要把"崩溃点附近出现过的未绑定调用"当成根因。** 上一轮我按日志邻接
把 TimeZone 判成触发点，实际它只是**同一个初始化流程里更靠后的另一处缺失**。
真正的判据必须是**时间序**：错误框之前紧邻的、且返回值确实非法的那个调用。
`getTimeZone` 的绑定本身仍是对的（补齐了真实缺失），但它不是 100 的原因。

### run19：错误 100 的真实触发点 = GLES20.glGetString 返回空扩展串
按时间序重新界定错误 100 之前的窗口（line 1735-1802），逐条未绑定调用：
```
Display.getRealMetrics(DisplayMetrics)              -> 0        未绑定
Object.getRealSize(Point)                           -> 0        未绑定
TimeZone.getDefault()                               -> stub Object
  └─ Object.getID()                                 -> ""       类名退化
GLES20.GL_EXTENSIONS                           (static field) -> 0
GLES20.glGetString(I)                                -> ""   ★
Utility.getNotificationFilter(Object)                -> 0        未绑定
Utility.getParam("ADMOB_SCREEN_HEIGHT")              -> "0"
AdMobView.removeAdMobView()                          -> 0
AdMobView.createAdMobView(...)                       -> stub Object
Utility.getScaleRatio(240,240,720,720)               -> 3.000
Utility.fullscreen(Z)                                -> 0
Utility.getNotificationData()                        -> empty array
Utility.showDialog(id=0, "Error", "An error has occurred.\n100")   ← 抛出
```
★ `GLES20.glGetString(GL_EXTENSIONS)` 返回**空串**是最强候选：Kairosoft 的
启动自检要读 GPU 能力/扩展集，空扩展串过不了它的校验，于是走自己的错误 100
（消息体第二行就是字面量 `100`，非我们拼的）。`GLES20` 在 jni.c 里此前**零绑定**。

注：`Config.init(1, 0, "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8A…")` 携带 **RSA 公钥**
（Kairosoft 许可校验用）。目前 `Config.init` 走 stub，若后续 100 仍不消，需考虑
这条许可链——但**先按证据顺序**修 GL 扩展串，不提前动它。

### 本轮修复（GL 扩展串 + 显示尺寸）
- `egl_sdl.c/h` 新增 `fp2_sdl_gl_string(unsigned name)`：从**活动 SDL 上下文**取
  真实 `GL_VENDOR/GL_RENDERER/GL_VERSION/GL_SHADING_LANGUAGE_VERSION/GL_EXTENSIONS`。
- `jni.c` 新增 `j_GLES20_glGetString`：绑定 `android/opengl/GLES20.glGetString(I)`，
  按真实驱动串返回（不再返回空串）。
- `jni.c` 新增 `j_Display_getRealSize`：绑定 `Display.getRealSize(Point)` 与
  `getSize(Point)`，**真实写入** Point 的 x/y 字段（此前 `j_Point_get` 读 x/y
  时返回的是屏幕宽高，不是 Point 自身字段值，语义是错的；本次让
  getRealSize 走 object_field 正确赋值）。

### 遗留观察点
- `Display.getRealMetrics` / `getMetrics` 仍未绑定 ⇒ DisplayMetrics 字段全 0。
  `density/densityDpi/widthPixels/heightPixels` 的 getFieldID 都已解析成功，
  但没有赋值来源；需要给 `android/util/DisplayMetrics` 的字段填真实值
  （density 建议 160.0f / densityDpi 160 / width,height 取 SDL 实际尺寸）。
- `Texture has out of range width / height`（Unity Line 596）仍在；由
  `getScaleRatio` 兜底扛住（零缓存时改用 SDL 实际可绘制尺寸 → 3.000）。
- `AdMobView.createAdMobView` 返回 stub Object；本作无广告，需确认 C# 侧
  不因 null 判定广告初始化失败。

---

## 2026-10-05：run20/run21 —— GL 缓存化 + 「错误 100」根因重新定位

### run20 实测（BuildID `50e28114`，sha256 `8e428588`）

上一轮两处修复都**确认生效**：

```
[fp2/video] GL_EXTENSIONS=2534 bytes          ← 缓冲 256→4096，截断消失
[fp2] jni: GLES20.glGetString(0x0) -> 2534 bytes  ← 扩展串不再为空
```

但**错误 100 依旧**（`showDialog(id=0, ...)` 仍在 1741 行）。⇒ 按 run18 的教训，
`glGetString` **不是**根因，不能再往 GL 方向加码。

### 决定性证据：`glGetString(0x0)` 的 0 是我们自己造的

run20 拉出错误框前 140 行原始日志，第一次看清了调用顺序：

```
getFieldID(android/opengl/GLES20.GL_EXTENSIONS, I, static=1)   ← 先读静态字段
unhandled android/opengl/GLES20.GL_EXTENSIONSI -> 0             ← 返回 0
getMethodID(android/opengl/GLES20.glGetString, (I)Ljava/lang/String;)
GLES20.glGetString(0x0) -> 2534 bytes                          ← 0 传进去了
```

**推翻了 run19 的假设。** C# 不是「传自己的枚举」，而是先读 `GLES20.GL_EXTENSIONS`
静态常量（真值 `0x1F03`）再传给 `glGetString`。`0x0` 来自我们未实现的静态字段。
⇒ 正确修法是补常量字段，而不是在 `glGetString` 里猜枚举。已回退 `case 0` 猜测逻辑。

### 错误框因果窗口内的 4 个真实缺陷（全部已修）

| # | 缺陷 | 证据 | 修复 |
|---|------|------|------|
| 1 | 包名是占位符 `net.kairosoft.android.mujin_en` | 元数据里唯一包名常量 `com.ballatergames.debug`，紧邻 `UnityType`（Kairosoft 包名自检） | 新增 `BB_PACKAGE_NAME "com.ballatergames.debug"` |
| 2 | `Utility.getAppWidth/getAppHeight -> 0` | 1494/1499 行，广告布局算术塌陷点，正在错误框之前 | `j_Utility_app_size` 返回真实 720×720 |
| 3 | `GLES20.GL_*` 静态常量全 0 | 见上 | `j_GLES20_constant` 返回 `0x1F00..0x1F03/0x8B8C` |
| 4 | `setParam` 只追加不覆盖 | `ADMOB_SCREEN_HEIGHT` 每轮布局都重写，`getParam` 永远返回首个旧值 | 改为**原地覆盖** |

另修两处：
- **短类名 `plugin/Config`**：IL2CPP 对同一方法查两次类名，1416 行
  `kairo/android/plugin/Config.init(1, 0, "<RSA公钥>")` **已命中**（许可链完整），
  1485 行 `plugin/Config.init` 落到 stub。已把短类名绑到同一 handler。
  `Config.finish` 加日志（错误路径收尾，可用来精确定位失败点）。
- **删除 run13 的 `ADMOB_SCREEN_HEIGHT -> "0"` 强制 hack**：那是在
  `getAppWidth/Height` 与包名还是假值时的猜测，现在只会掩盖真实值。

### 元数据取证要点（复用）
- `global-metadata.dat` 字符串表是 **UTF-8**；按 UTF-16 扫全 0 命中。
- 表项按**首次引用顺序**排列 ⇒ 语义相邻的字符串往往就是同一段代码的常量。
  `GL_EXTENSIONSGL_OES_depth24` 相邻 ⇒ 托管侧只查深度纹理扩展。
- SafetyNet 那一簇（`ErrorCheckEnabledPackageNameUndefined` /
  `ErrorJwsValidationCommunication` / `InvalidApkCertificateDigest`）是
  **Play Integrity 正版校验**，离线移植必然过不了。**不要去修它**，
  它与错误 100 无关（Kairosoft 对无 Play 服务环境有独立降级路径）。

---

## 2026-10-05：run22-25 —— 错误 100 的性质被确定（重大结论）

### 结论：错误 100 是**主动上报的自检失败**，不是异常/崩溃

run25 用 `backtrace()` + **IL2CPP 异常 hook** 两条新取证手段，拿到了决定性判据。

**取证手段 1：native 回溯（`FP2_DIALOG_TRACE=1`）**
```
[fp2/trace] showDialog id=0 native backtrace (4 frames):
  #0..#2  0x5575…        ← 我们自己的 loader（JNI 派发层）
  #3      libunity.so+0x1c95a4
```
`0x1c95a4` 处的指令是 `bl`（返回地址），调用点在 `libunity.so+0x1c959c`。
该偏移 **小于 `JNI_OnLoad`(0x1da4f4)** ⇒ 位于 libunity 的 **JNI 桥接层**，
**libil2cpp.so 完全不在栈上** ⇒ showDialog 是经 Java 桥转发的，不是托管代码直接调。

**取证手段 2：IL2CPP 异常 hook（`FP2_IL2_EXCEPTION_TRACE=1`）**

libil2cpp 通过 `dlopen/dlsym` 解析自己的 API，而本 port 的 `dlsym` 就是
`bionic.c: my_dlsym()`。在那里拦截两个符号即可**零侵入**捕获托管异常：
```
[fp2/il2] hooked il2cpp_raise_exception (real 0x…64 -> 0x…)
[fp2/il2] hooked il2cpp_unhandled_exception (real 0x…08 -> 0x…)
```
结果：**`[fp2/il2]` 只有这 2 条安装日志，实际异常 0 条**，
而 `showDialog(id=0, "Error", "An error has occurred.\n100")` 照常出现。

⇒ **Kairosoft 错误 100 是游戏在启动自检里判定失败后主动调 `showDialog` 上报，
不是托管异常、不是崩溃。** 这条结论推翻了前几轮"找哪个调用返回了坏值"的思路方向：
不存在"抛异常的那一行"，只有"自检判定为假的那一个条件"。

### 四项修复已确认生效（run21，实测）
| 项 | 证据 |
|---|---|
| 包名 | `[?/Unity] ApplicationInfo com.ballatergames.debug version 1.0.0` |
| app 尺寸 | `Utility.getAppWidth/Height` 走 `j_Utility_app_size` → 720×720 |
| GLES20 常量 | `GLES20.glGetString(0x1f03) -> 2534 bytes`（`0x0` 消失） |
| 参数表覆盖写 | `getParam("ADMOB_SCREEN_HEIGHT") -> "720"` |

**GL 方向已彻底排除**：日志 1187-1189 行 Unity 自己打印了完整扩展串，
其中确实含 **`GL_OES_depth24`** ⇒ 托管侧深度纹理检测**通过**。

### 设备侧已全部核对一致（run24b）
- 物理屏 `720,720`（`/sys/class/graphics/fb0/virtual_size`，DSI-1 `720x720p61`）
- drawable 720×720、`density=1.0`/`dpi=160`、`getScaleRatio(240,240,720,720)=3.000`
- 渲染 `GRAPHICS-EVIDENCE verdict=OK shader_probe=pass`
⇒ 尺寸/密度/GL/包名**都不是**失败条件。

### 下一步（run26 的唯一方向）
错误框前最后一个未实现契约是 **`Utility.getNotificationFilter(Object)I -> 0`**
（每轮必现，`[fp2/miss]`）。它是 Kairosoft 通知过滤位掩码。
另有 `AdMobView.createAdMobView` 返回 stub Object、`fullscreen` 未实现。

**判据原则（沿用 run18 教训并强化）**：
1. 先看**时间序**上最后一个「返回值确实非法」的调用，不要按日志邻接猜；
2. 本轮补充：先用**异常 hook** 证明"有没有异常"，再决定是查异常还是查自检条件——
   这一步本应更早做，能省掉 3 轮编译部署。

### 新增诊断开关（均为 opt-in，默认关闭）
- `FP2_DIALOG_TRACE=1` —— showDialog 处打印 native 回溯（`<execinfo.h>` `backtrace()`）
- `FP2_IL2_EXCEPTION_TRACE=1` —— 拦截 `il2cpp_raise_exception` /
  `il2cpp_unhandled_exception`，用 `il2cpp_format_exception` /
  `il2cpp_exception_to_string` 打印托管异常类型+消息

---

## 2026-10-05：run28 —— 换战场：从设备日志转向 IL2CPP metadata + dex

前 27 轮都在设备日志里找「哪个 JNI 调用返回了坏值」。run28 换了思路：
**把判据从日志搬到游戏自己的二进制里**，因为自检条件只写在托管代码与
Java 插件层中，日志只能看到调用顺序，看不到判定内容。

### run27 实测：JNI 基础解析补齐，音频链修复

| 项 | 证据 |
|---|---|
| `Integer.parseInt` | `parseInt("48000") -> 48000`、`parseInt("192") -> 192` |
| `String.length/isEmpty/trim` | 已绑定并命中 |
| 通知契约 | `cancelNotifications(1, 0)` / `setNotificationBackground(0)` 已记录 |

**修复意义**：`parseInt` 此前由通用 stub 回答 `0`，把刚修好的
`getProperty("OUTPUT_SAMPLE_RATE") -> "48000"` 又吞回 0，设备被配置成 0 Hz。
**stub 返回 0 比返回空更糟——它是「自信地错」，会把下层修复悄悄吃掉。**

### 新增离线分析工具（三件套）

| 工具 | 作用 |
|---|---|
| `tools/meta_dump.py` | 解 IL2CPP metadata v24 的字面量表与名称表 |
| `tools/dex_probe.py` | 直接遍历 `classes.dex` 的 string/type/method/class_def 表（无需反编译器） |
| `tools/fetch_meta.py` | SFTP 拉取 `global-metadata.dat` + `libil2cpp.so` 供离线分析 |
| `tools/const_find.py` / `lit_refs.py` | 在 il2cpp 段搜常量/字面量引用（capstone 反汇编） |
| `tools/elf_sections.py` | 列 section 表（**关键**：本 NDK 构建把托管代码放在独立的 `il2cpp` 段，不是 `.text`） |

**metadata v24 表布局（已用字节连续性验证，非猜测）**：
```
0x08 stringLiteral      off 0x00000110 size   74488  -> 9311 条
0x10 stringLiteralData  off 0x00012408 size  188360
0x18 string             off 0x000403D0 size  545316  -> 68164 条 NUL 分隔标识符
```
头部第二个字段是**字节 size 而非 count**：`272+74488==74760`（正是下一表偏移），
三表首尾相接。按 count 解读会静默越界。

### 关键证伪：许可 / IAP 全部排除

`Config.init(1, 0, "MIIB…")` 传入的是 RSA 公钥，dex 侧也确认存在
`kairo/android/plugin/licensing/LicenseChecker`（含
`RESPONSE_NOT_LICENSED`、`RESPONSE_ERROR_INVALID_PACKAGE_NAME`、
`RESULT_DONT_ALLOW`）与 `licensingV1/LicenseChecker`，一度高度怀疑。

**但设备日志里 `getLicenseCheck` / `startLicenseCheck` / `googlePassCheck` /
`billing_` 一次都没被调用**（无 Play Services，Kairosoft 整段跳过，
`Config.init` 只是把 key 存进 `Config.LICENSE_KEY`）。
⇒ **商业化路径全部排除**，又一个「看起来像但实测证伪」的方向。

### 关键证伪：资源加载也排除

`stat("assets/bin/Data/data.unity3d") -> -1` 一度可疑，实测 Unity 的加载序列：
`globalgamemanagers -> 0`、`unity default resources -> 0`、
`globalgamemanagers.assets -> 0`、`level0 -> 0`、哈希 bundle `-> 0`
**全部成功**；`.resS/.resG/.split0 -> -1` 是 Unity 的正常伴随文件探测
（主文件命中即止）。monobind 侧 `etc/`、`global-metadata.dat`、
`KairoLibrary.dll-resources.dat` 也全部 `-> 0`。
⇒ **资源不是失败条件**。

### 决定性发现：托管侧自检类 = `BootForm`

名称表按**首次引用顺序**排列，邻域即同一段代码的常量。`BootForm`（#28878）
的邻域直接给出了启动自检的全部组成：

```
BootForm, fontOff, LoadBox, InitApp, ClassLoad, DrawOsumaiLink,
RESLOAD_SEB_WAVE / RESLOAD_SEB_PARTS / RESLOAD_SEB_KAIRO / RESLOAD_SEB_MAN,
PARTS_SIGNBOARD / PARTS_COMPANY / PARTS_BIG_CLOUD / PARTS_CLOUD,
STATE_BOOT / STATE_DOWNLOAD / STATE_LOAD / STATE_REGIST /
STATE_ERROR / STATE_FINISH / STATE_VERSIONUP / STATE_OSUMAI,
httpErr_, classLoad_, regist_, interstitial_
```

⇒ 自检 = `InitApp` + `ClassLoad` + **四类资源存在性检查**
（`RESLOAD_SEB_*`：WAVE 音效 / PARTS 部件 / KAIRO 图标 / MAN 说明）。
错误 100 就是这四类中某一类判定失败后走的 `STATE_ERROR`。

另：字面量 `An error has occurred.`(#4390) 与 `\n100`(#6026) 是**两条独立
字面量**，运行时拼接 ⇒ `100` 是**格式化出来的整数**，非硬编码文案。
`const_find.py 100` 在 il2cpp 段**零命中**，进一步说明该数字来自运行时状态
（自检槽位编号），而不是 C# 常量。

### 社区情报（决定性，改变了策略）

Kairosoft 玩家社区对**错误 100** 的公认描述是：

> Error 100: Happens quite commonly. **No true way to fix this one, most
> effective for me however is to just spam click retry.**

⇒ 错误 100 在正版手机上也是**高频出现的可重试瞬时状态**，并非致命错误。
这与我们的观测一致：自检判定失败 → 弹框 → 框上带 Retry。

**策略调整**：不再追求「消灭弹框」，而应
1. 定位 `RESLOAD_SEB_*` 四类中**具体哪一类**判定失败（用 `showDialog` 的
   `listener` 参数与相邻 `getParam` 值锁定）；
2. 若为环境性误判（无 Play Services / 无 Google 账号导致），
   则让该分支走**降级路径**而非 `STATE_ERROR`——对齐原版在无 Play 环境下的行为。


---

## 2026-10-05：run30-32 —— 抓到托管侧调用栈，判定逻辑首次可读

### run29 结果

Kairosoft 插件层契约**已 100% 覆盖**：补全 `Config.getParam`/`setParam`
（dex 证据：`Config` 共有 4 个静态方法，此前只绑了 `init`/`finish`）
与约 100 个 `Utility`/AdMobView/Interstitial 契约。方法表从 37 条降到 33 条，
剩余全部属于 Android 框架（`PackageManager`/`Handler`/`MediaRouter`/
`Display`/`InputManager`）或第三方（`Tapjoy`）。

**但错误 100 仍在** ⇒ 方法表不是瓶颈，判定条件在托管侧读到的**数据**里。

### run30：`showDialog` 其实被调用了两次

设备日志（此前只看了 `id=0`）：

```
Utility.showDialog(id=0, title="Error", msg="An error has occurred.
100", listener=no) -> 0
Utility.showDialog(id=3, title="Error", msg="エラーが発生しました
問題解決の為にエラー情報を送信して頂けませんか？
（※個人情報は送信されません）", listener=no) -> 0
```

⇒ id=0 是**英文错误弹框**，id=3 是**日文「是否发送错误报告」**。
两者是 Kairosoft 标准错误上报对，同一次失败触发两次弹框。
`listener=no` 说明我们没实现回调，故第二次也无人应答。

### 决定性修正：`\n100` 是硬编码字面量，不是运行时状态

前一轮的结论「`100` 是格式化出来的整数」**错了**。按字面量邻域重新取证：

```
#4390  'An error has occurred.'                        ← body
#4391  'Error'                                         ← title
...
#6025  'Change Name'
#6026  '\n100'                                         ← 英文错误码
#6027  'エラーが発生しました\n問題解決の為に…'         ← 日文错误文案
#6028  'Return value address cannot be 0.'
```

`\n100`(#6026) 与日文错误文案(#6027) **紧邻**，属同一段代码的常量池；
且全表只有 `\n100`(#6026) 与 `\n102`(#6201) 两个「换行+纯数字」字面量。
`0100`(#6310)/`1100`(#6399) 属 `medal_*` 资源表，与错误码无关。

⇒ **`100` 是错误码字符串**，`'An error has occurred.' + '\n100'` 由
C# 直接拼成，**不经过整数格式化**。`const_find.py 100` 零命中因此并不矛盾：
它本来就不该以 4 字节整数形式存在于 il2cpp 段。

**方法论教训**：邻域法只能证明「同属一段代码」，不能证明「运行时拼接」。
上一轮我用一个跨表的假设（body 与 `\n100` 必属两处）去否定字面量证据，
结论反了。**字面量邻域是弱证据，但强于「我想不出拼接方式」。**

### 决定性修正：`meta_dump.names()` 的 docstring 与实现不符

`names()` 声称表 2 后紧跟 blob、并按 `{length, dataIndex}` 索引数组解析，
实际实现产出的全是**空串**（34301 个空 entry）。表 2 本身就是 NUL 分隔
blob（`mscorlib\0\0NULL\0\0...`），已在 run28 实证，本次把 docstring 与
实现一并改正——**一个工具的空结果比没有工具更危险**（run28 `contract_gap.py`
正则失效已犯过一次同类错）。

改正后才搜到 `BootForm`(#28879) 的完整自检项清单，以及 `e_`(#28893)、
`STATE_ERROR`(#28907)。`errorCode`(#4402)/`ErrorCode`(#4433) 经查属
**mscorlib COM 互操作**（`ErrorWrapper`/`MarshalDirectiveException` 邻域），
与游戏无关，排除。

### run31：拿到托管侧调用栈（本次最大突破）

`FP2_DIALOG_TRACE=1` 重启，`showDialog` 的 native backtrace 揭示了调用链：

```
#0 0x556a9d613c  ?+0x0          ← 主程序映像，非任何 LIBS[] 条目
#1 0x556a9d89e8  ?+0x0
#2 0x556a9d935c  ?+0x0
#3 0x7f7be365a4  libunity.so+0x1c95a4
```

前 27 轮全在设备日志里找「哪个 JNI 调用返回坏值」，**日志只能看到调用顺序，
看不到判定内容**。回溯栈第一次指出「哪个托管方法发起了这次弹框」。

但三帧显示 `?+0x0`——`fp2_mod_at()` 只遍历 `LIBS[]`（libmain/libunity/
libil2cpp）白名单，**主可执行映像不在其中**，所以最关键的三帧无法解析偏移。

### run32：修 `fp2_mod_at`（进行中）

改用 `dl_iterate_phdr()` 覆盖整个 link map，含主程序自身
（`dlpi_name[0] == '\0'` 即主映像）。一旦偏移可读，即用
`tools/disasm_at.py` 在 stripped 的 `libil2cpp.so` 上反汇编该偏移，
借每函数常量池 + PC-relative LDR 识别 `showDialog` 调用点**与其守护分支**，
直接读出「自检判定为假的那一个条件」。

**这是 27 轮以来第一次具备读取判定逻辑的能力**——不再靠猜。

### run33 失败：我自己的代码 SIGSEGV（重要教训）

第一版 `fp2_dump_managed_methods()` 直接用 `nx_lookup_in` 取反射 API 自己调，
结果在 `showDialog` 第一次触发时崩溃：

```
[D/CRASH] signal 11 (SIGSEGV), code 1 (SEGV_MAPERR), fault addr 0000005985bb179c
   pc   0000007f813316d0  (libil2cpp.so 内)
   x30  0000007f813316cc  (= pc-4，正是刚执行的 bl)
```

**根因**：`il2cpp_class_from_name` 的真实签名是
`(const Il2CppImage*, const char* namespace, const char* name)`——**三个参数**，
第一个是 **Image**。我传了 `(domain, ns, name)`，把 domain 当 image 用，
libil2cpp 随即解引用垃圾指针。

**项目里早就有正确实现**：`port/src/il2.c:20` 完整地写着这个签名，
且 `il2_class()` 会**遍历所有 assembly** 逐个试。

⇒ **教训：不要重新实现已有的能力。** 需要新能力时，把缺的那块**加到既有封装里**
（本次加了 `il2_methods_foreach()` / `il2_class_by_name()` 到 `il2.c`），
而不是在调用方重推一遍 ABI。重推出来的版本比原版差一个参数。

顺带第二条：`il2_load()` 里的 `GET()` 宏在符号缺失时会**整体返回失败**。
新增的 `il2cpp_image_get_class*` 是可选便利能力，若走 `GET` 会把
控制绑定等全部 il2_* 功能一起拖垮——故改为可选加载（`nx_lookup_in` 不检查）。

### run34：`BootForm` 找不到，改用运行时枚举

run33 修好后不崩了，但 `class BootForm not found`——`il2_class()` 需要**精确的
(namespace, name) 对**。我试着从元数据反推 namespace，失败：TypeDefinition 表
（slot 23，4006 项）**不是**我假设的 32 字节布局，`BootForm`(#28879) 不在
nameIndex 指向处。

⇒ 改为**运行时枚举** `image_get_class_count`/`image_get_class` 按简单名匹配，
再把 namespace **报告出来而不是假设**。枚举比解释元数据更便宜，也不吃布局假设。


### run34-36：拿到方法地址，但偏移字段取错（两次自我否证）

run34 用 `il2_class_by_name()` 成功枚举 `form.BootForm` 全部 **23 个方法**及入口地址：

```
.ctor  ToString  Init  Update  Draw  Resume  Finish  LoadBox  ChangeState
InitApp  Run  Load  ClassLoad  DrawOsumaiLink  FillGradationRect
GetColorOfRGB  GetTouchValue  SetTouchValue  GetHashCode  .cctor
<Load>m__0  <Load>m__1  <Load>m__2
```

**这已足够**：自检状态机的四个关键点（`InitApp` / `ClassLoad` / `Load` /
`ChangeState`）都在其中，可直接反汇编读条件。namespace 实测为 `form`。

但**偏移算不出来**，且我连续用两次错误假设试图解释：

1. run35 假设 `fp2_mod_at()` 能解析游戏对象 → 仍得 `?+0x0`。
   真实原因：它只认主程序 + `LIBS[]` 白名单。
2. run36 打印实际基址后发现**决定性事实**：
   ```
   libil2cpp.so base = 0x7f7b407000
   BootForm::.ctor fn = 0x7f7a715436      ← fn 比 base 还小
   ```
   ⇒ `MethodInfo + 2*sizeof(void*)` **不是函数指针**。
   我在 `il2.c` 里注释断言"函数指针在 wrapper 后第二个字段"，这是**未经验证的
   猜测**（照抄 IL2CPP 源码印象），实测被否证。

**顺带否证**：用"所有入口都落在某可执行段内"反推基址，得到 **2855 个候选**
⇒ 该约束太松，**减法推导基址不可靠**。

⇒ run37 改为直接问内核：`fp2_list_modules()` 用 `dl_iterate_phdr` 列出
全部已加载模块的 base/span。**加载器是权威的，而且不会算错**。

**方法论**：地址归属不要用减法推，要用加载器问。此前连续三个假设
（fp2_mod_at 认游戏对象 / 减法反推基址 / MethodInfo 布局）都建立在
"我记得应该是这样"上，全部被实测否证。凡是无法从设备直接读出的量，
先想办法让它变成可直读的。

## 2026-10-05：run38-39 —— 拿到托管方法地址，转为活体观测

### run38：扫描 MethodInfo 定位函数指针（成功）

不再猜字段偏移，改为**扫描前 12 个指针字段，取第一个落在 libil2cpp
代码窗口内**的：

```c
const uintptr_t lo = (uintptr_t)il2_base + 0x540000;
const uintptr_t hi = (uintptr_t)il2_base + il2_size;
for (int i = 0; i < 12; i++) {
    const void *p = *(void *const *)((const char *)m + i * sizeof(void *));
    if ((uintptr_t)p >= lo && (uintptr_t)p < hi) return p;
}
```

**一次成功**：`form.BootForm` 全部 23 个方法都拿到可反汇编偏移，
范围 `0x896b5c..0x8a1adc`（窄区间，全在 il2cpp 段 `0x66f8cc..0x10d5f0c` 内）。

已核对段布局：il2cpp 段 `addr == off`，故偏移可直接当文件偏移喂给
`disasm_at.py`。`.ctor` 反汇编出标准序言
（`stp x22,x21,[sp,#-0x30]!` + `add x29,sp,#0x20`），**偏移语义确认无误**。

### 读码的发现：InitApp 很薄，ChangeState 是唯一状态入口

- `InitApp` 只有 **0x16c 字节**就结束（下一方法 `ChangeState` 在 0x8976c4），
  主体是 6 次 `0xa4d24c` 调用（参数 0x5000 / 0x1e000，像资源预留）。
- `ChangeState(int)` 末尾 `str w19, [x20, #0x100]` ⇒ **状态存在对象 +0x100**，
  它是状态机的唯一写入口。开头即 `cmp w19, #7` ⇒ 共 7 个状态。
- `Load`(0x89af88) 与 `ClassLoad`(0x89ff7c) 之间有 **20 KB** 空白。

### run39 的转向：从「读 20 KB 汇编」改为「让游戏自己说」

静态读 20 KB 剥离后的 ARM64 慢且脆。改为在设备上 hook `ChangeState`，
把状态转移序列打成日志 —— 一行一个状态，序列本身就回答
「哪次调用跨进了 STATE_ERROR」。

新增能力（都加在既有封装 `il2.c` 上，**不重新推导 ABI**）：

| 函数 | 用途 |
|---|---|
| `il2_method_addr()` | 单个方法的原生地址，复用 `il2_method_fn` 的扫描 |
| `il2_fields_foreach()` | 枚举字段（含静态），为读 `RESLOAD_SEB_*` 铺路 |

**`il2_fields_foreach` 的 `is_static` 判据**：取 `il2cpp_field_get_flags`
的 `0x10`（ECMA-335 的 `FIELD_ATTRIBUTE_STATIC`）。**不按 layout 猜** ——
静态字段必须走 `field_static_get_value`，传对象进去读的是垃圾。
这几个新符号一律**不走 `GET` 宏**（它遇缺失即整体 `return 0`，
会连带拖垮控制绑定）。

**hook 设施**（`fp2_hook_install`）：
- `ldr x16, #8 ; br x16 ; .quad replacement` —— AArch64 上 `b` 只有
  ±128 MB，跨模块够不着，这是标准解法。
- 16 字节 ⇒ 搬走目标前 4 条指令到 trampolim。**`ChangeState` 的序言是
  四个 `stp` + 一个 `add x29,sp`，全非 PC 相对**，可原样复制；
  若含 `adrp` 则换个地址就改了分支目标。
- 需 `mprotect` 把代码页设为 RWX（loader NX 默认 R+X）。

**方法论**：能直接问对象的，就别去猜它脑子里想的。
run31 的教训（栈上看不到托管帧）在这里升级成正面做法 ——
既然拿得到函数入口，就把入口变成提问的接口。

### run39a：两次否证，都很便宜

**否证一：安装点太晚（自己踩的坑）。**
最初把 `fp2_install_changestate_hook()` 挂在 `showDialog` 的 trace 分支里。
设备日志直接给出答案：

```
1813: Utility.showDialog(id=0, title="Error", msg="An error has occurred.
1815: hook: 0x7fa25cf6c4 -> 0x5571063780 (prologo copiado para 0x7faac56000)
1816: state: narrando ChangeState em 0x7fa25cf6c4
```

hook 装好了，**却一次都没被调用** —— 因为触发安装的那个 `showDialog`
本身就是我要观察的那个决定的后果。**仪器必须早于现象**，这条以前从未
真正被遵守（前几轮的 trace 全是事后取证）。

修正：`main.c` 在 `run_unity()` 开头、`initJni` 之后、以及渲染循环前 8 帧
各调一次（幂等，前几次会因 runtime 未起而退出）。

**否证二（关键）：装好之后，ChangeState 依然零调用。**
提前安装后，hook 落在日志第 **1412** 行（frame 1），`showDialog` 在
**1820** 行 —— 中间 400 行日志，**ChangeState 一次都没触发**。

这有两种解释，而必须区分：
- (a) 错误 100 的路径真的不经过 `BootForm.ChangeState`；
- (b) **hook 机制本身没生效**（比 (a)  banal 得多）。

**不接受"空结果"当结论。** 于是改成一次 hook 五个方法，其中
`Update`（Unity 每帧必调）当**阳性对照**：

| 方法 | 作用 |
|---|---|
| `Update` | **阳性对照** —— 它响 ⇒ 工具没坏 |
| `ChangeState` | 目标（状态写入，`str w19,[x20,#0x100]`）|
| `Init` / `ClassLoad` / `Load` | 自检路径 |

判定规则写死在代码里：**只有 Update 与 ChangeState 同时 hook 成功才算
"仪器就绪"**。少一个就打印"falta o controle positivo ou o alvo"，
而不是假装成功 —— 一个无法与"hook 没工作"区分的结果，比明说的失败更糟。

**方法论**：空结果必须先自证工具有效，再拿去否证对象。
阳性对照是这个自证的最短形式。

### run39b：阳性对照生效，抓到**两个我自己的 bug**

5/5 全部 hook 成功（`instrumentacao completa (5/5)`），偏移与 run38 逐一
吻合（`0x7f858c66c4` = base + `0x8976c4`），但**依然零调用** ——
连 `Update` 这个阳性对照都没响。按上一节的规则，这不能算结论，
先怀疑仪器。设备日志末尾给出了答案：

```
./Beastie Bay.sh: line 42:  3966 Segmentation fault
```

进程是**崩**的，不是"没走到"。两个 bug，都在 hook 代码里：

**Bug 1 —— trampolim 回跳用了 `b`，而距离超出范围。**
原注释写着"trampolim  numa reserva nova, entao a distancia e de poucos MB"，
**这个假设从一开始就是错的**：`mmap` 落点不受我们控制。实测

```
target      0x7f858c66c4
trampoline  0x7f8df4d000
delta       -141060412          (=-141 MB)
b 的范围    ±134217728          (=±128 MB)   → 溢出
```

回跳地址被截断成垃圾 ⇒ 跳到随机处 ⇒ SIGSEGV。
**修正**：回跳也用 `ldr/br/.quad`（无范围限制）。换更近的 trampolim
不是办法 —— 地址不由我们定。

**Bug 2 —— stub 末尾是 `ret`，从没进过 trampolim。**
stub 是被替换序列的 `br x16` 进来的，**`x30` 仍是调用者的返回地址**。
一个 `ret` 直接返回给调用者，**整个被 hook 方法体（包括 trampolim 要
重放的那部分）永远不执行**。
**修正**：stub 结尾改为 `ldr x16, =slot ; br x16` 跳进 trampolim。
这里也不能用 `b`（stub 在可执行文件里，trampolim 在 mmap 里，
同样是那 141 MB），故每个方法一个**独立的 trampolim 槽位变量**，
安装时写入。

**两个 bug 的共同点**：`b` 的 ±128 MB 我"知道"，但没意识到 mmap 的落点
不受控 ⇒ 距离可达任意大。知识清单不等于对距离的判断。
另一个共同点：Bug 2 里 stub 能被 `br` 到达、却用 `ret` 离开，
是典型的"签名对了、语义错了"—— 这也是 run33 那次 SIGSEGV 的形状。

**这才是阳性对照的价值**：如果只看"ChangeState 零调用"，
我很可能把结论写成"错误 100 不走 BootForm"—— 一个听起来合理、
实际上是被自己的 hook 崩掉而制造的假结论。**Update 同样零响，
才是指向"仪器坏了"的正确线索。**

### run39c：修编译 + 设备取证封装

**先修阻塞构建的三处（都写对了却编不过，等于没写）**：

1. `(uint32_t)(uint64_t)p >> 32` —— 先截 32 位再右移，高位必为 0，
   跳转目标会落到地址空间前 4 GB。GCC 的 `right shift count >= width`
   警告是对的。改为一处 `uint64_t` 中转，`>> 32` 后再截断。
2. `#sym` 出现在 `__asm__ __volatile__(...)` 的参数列表里 —— `#` 是
   **预处理器**运算符，在这个位置不会被处理，原样送到编译器 →
   `stray '#' in program`，并**连锁**打垮后面所有行（报错行号离真凶
   数百行）。修法：把需要 `#` 的文本在**外层**宏里拼成纯字符串字面量，
   再作为参数传进 `__asm__`（新增 `FP2_W_TAIL(sym)`）。
   **教训：asm 宏里凡是出现 `#`，一律在 asm 体外汇好。**
3. `ldr x16, =símbolo` 换成 `adrp x16, sym` + `ldr x16, [x16, #:lo12:sym]`。
   前者让汇编器建一个 **literal pool**，pool 可以落在段末尾，
   而 `ldr` literal 的 PC 相对范围只有 **±1 MB**；后者靠重定位定位，
   没有这个限制。stub 尾部 filler 用 `nop`，不需要凑 8 字节对齐。

修完构建通过：md5 `8846c0cc0b170ef171bdf1e1241396b5`，
BuildID `a77573863b8270d30ddaf0f41d7fbbcb64940afc`。

**设备取证：`tools/bb_ssh.py`（新）**

192.168.31.196 的 SSH 在本轮实测不稳定：`Test-NetConnection` 报
`tcp22=True` 但 `ping=False`（ICMP 被拦），原始 socket 有时 0.26 s 就
拿到 banner `OpenSSH_8.0p1 Ubuntu-6build1`、有时直接 connect 超时。
`paramiko` 连着失败 6 次、第 7 次才通。
**这不是网络问题**（握手成功时 TCP 只花 0.26 s），是设备 SSH 服务在
负载下丢连接 —— 采集时游戏正在跑、日志开着。

`bb_ssh.py` 把这件事一次性解决：分离 `timeout`（20 s，TCP→SYN 窗口）与
`banner_timeout`（40 s），失败重试 8 次、间隔 5 s，并把 stdout/stderr
**分开返回** —— 游戏的 `state:` 行走 stderr，混在一起就分不清
"游戏打印的"和"命令输出的"了。

用法：

```
python tools/bb_ssh.py '<comando remoto>'
python tools/bb_ssh.py -i port/beastiebay-nextos '<comando remoto>'
```

（`-i` 顺手把本地二进制传到 `/tmp/bb_newbin`，由 `r39run.sh` 原子替换。）

**顺带一个测量方法的教训**：`ping` 判活在这个网络里是**无效**的。
本轮判"设备离线"就是被 ping 骗了 —— 判活要用 **TCP 端口**，
`Test-NetConnection -Port 22` 才作数。本机 shell 工具还不回显
子进程输出（已知缺陷），得让命令 `Out-File` 写盘再读。

### run39d：阳性对照首次生效，仪器确认可用

**部署结果**（md5 `8846c0cc0b170ef171bdf1e1241396b5`）：

```
1412  hook: 0x7f8dd8a6c4 -> 0x557c794938   (ChangeState, tramp 0x7f96411000)
1414  hook: 0x7f8dd8a884 -> 0x557c794990   (Update,     tramp 0x7f96410000)
1416  hook: 0x7f8dd8a068 -> 0x557c7949e8   (Init,       tramp 0x7f8c08d000)
1418  hook: 0x7f8dd92f7c -> 0x557c794a40   (ClassLoad,  tramp 0x7f8c08c000)
1420  hook: 0x7f8dd8df88 -> 0x557c794a98   (Load,       tramp 0x7f8c08b000)
1422  state: instrumentacao completa (5/5)
```

**先纠正一处自己的误读**：日志里的 `#1` 是**事件序号**
（`++fp2_changestate_seq`），不是方法 id。所以 `#1 Init(...)` 里
`id == FP2_W_Init == 2`，**名字没有错位**。差点误判成"事件与名字不匹配"。

**阳性对照成立 —— 而且是被 `Init` 证明的，不是被 `Update`**：
`Init` 命中一次，回溯两帧：

```
1821  state: #1 Init(-1534339296) this=0x7f8c09bbe0 ?+0x0
1822  state:   #0 0x557c7a33a0 libil2cpp.so+0x483a0
1823  state:   #1 0x557c794a0c libil2cpp.so+0x39a0c
```

`0x557c794a0c` 正是 `stub_init`（`0x557c7949e8`）**+ 0x24**，
即 stub 里 `bl fp2_managed_event` 的**返回地址** ⇒ 我们的代码确实在跑。
（`?+0x0` 也是对的：`this` 是托管堆对象，本就不在 libil2cpp 段内。）
**这是 run33 以来第一次拿到"仪器自证有效"的硬证据。**

**同时冒出真矛盾**：`ChangeState` / `Update` / `ClassLoad` / `Load`
**全部零调用**，而静态读码说 `Init` 必然调用 `ChangeState`：

- `Init` 在 `0x8974c4` 有 `bl #0x8976c4`（`0x8976c4` = `ChangeState` 入口）；
- `Init` 全长（`0x897068`..`0x897554`，0x4f0 字节）**只有一个 `ret`**，
  且全部条件分支（41 条 `tbz/cbnz/b.hi`）都只是向后跳 0x10..0x20 字节
  做元数据初始化检查，**没有任何一条跳向出口**。

⇒ 静态上 `Init` 必须走到 `0x8974c4`。矛盾必须由运行时的字节来解释，
**不能靠继续读码猜**。

**顺带否证一条老路**：`tools/lit_ref.py`（新写，扫 `movz <Wd>, #imm16`）
搜 `#4390`（"An error has occurred."）与 `#6026`（"\n100"），
各自只命中一个地址 `0x1285e3c` / `0x1292acc` —— 但**这两个地址在 `.rodata`**
（`0x10d5f10`..`0x1233365`），不是代码。
**匹配到的是数据，不是指令。** 字符串不以裸 ASCII 存放，
索引也不作为立即数出现在方法体里 —— 这正是 `lit_find.py` 早就警告过的，
现在有了独立的第二个证据。字面量定位只能走**运行时反射**
（`il2_fields_foreach` / `il2_method_addr`），不能靠静态扫立即数。

**下一步（run39e）**：读回设备上 hook 的**活体字节**（`tools/r39d.sh`）。
三种不同错误会产生**完全相同**的症状"装上了、零调用"：
(a) `target` 的替换没生效（页仍是只读 / 事后被重写）；
(b) stub 进去了但回程（slot → trampolim → target+16）跳到垃圾；
(c) 逻辑走到了 `bl`，但被观测的方法不是我们按名字解析出来的那个。
**只看日志分不出 (a) 和 (b)**，读字节一次就定性。

### run39e：真正的机制找到了 —— 只渲染了 6 帧

第一次跑 r39d 时读到 `GAME NOT RUNNING`。这不是脚本 bug，是**新事实**：

```
1163  [fp2] frame 4 keep=1
1176  [fp2] frame 5 keep=1
1177  [fp2] frame 6 keep=0
1178  [fp2] Unity requested render-loop stop at frame 6
1180  [fp2] nativeFocusChanged(false) OK
1219  VIDEO: ... frame_proof=unsampled verdict=UNKNOWN reason=no-frames-sampled
```

**Unity 在第 6 帧主动要求停止渲染循环**，
随后走 `nativeFocusChanged(false)` → `nativeDone` 的正常收尾，
再由 `exit_deadline_thread`（`main.c:645`，**只等 1.5 秒**）兜底 `_exit(0)`。
所以日志末尾那句 `exit deadline expired` **不是崩溃、不是被杀**，
是设计上的收尾。crash 标记计数为 0。

**这一条解释了此前所有"零调用"**：

| 观测 | 零调用的真实原因 |
|---|---|
| `Update`（阳性对照）零响 | 只渲染了 6 帧，Update 最多被调 6 次，且首次调用发生在 hook 安装（frame 1）之后，但**日志限流 3 次且事件序号只记到 1** —— 说明它连一次都没进入 stub |
| `ChangeState` 零响 | 错误发生在 `Init` 内部，`Init` 在 `0x8974c4` 处 `bl ChangeState` 之前就走了 showDialog 分支并终止了主循环 |
| `ClassLoad` / `Load` 零响 | 同上，资源加载从未开始 |

**同时纠正一个我自己的误判**：中途我怀疑
`apk/lib/arm64-v8a/libil2cpp.so` 是 Freedom Planet 2 的库
（加载器横幅确实写着 "Freedom Planet 2 compatibility loader"，
而 `apk/` 目录里还留着 2021-12-01 的旧 APK 文件）。
**核对后确认这是虚惊**：两个 `libil2cpp.so` 的
大小（24316824）、`.text`/`il2cpp`/`.rodata` 段布局**逐一相同**，
`apk/lib/...` 早已被 Beastie Bay（开拓神秘岛，2026-10-05）的真实二进制覆盖，
`apk/` 只是保留了 APK 的目录骨架。**静态读码没有错。**

教训记下来：**"加载器横幅与项目名不符"是历史血缘，不是不一致**。
判据要看**二进制本身**（大小 + 段表），不能看目录名或字符串。

**方法论再记一条**：抓现场有时效性。进程没了，
`/proc/<pid>/mem` 就没有，hook 只存在于活体内存里 ——
所以"读日志"和"读字节"必须放在**同一次运行**里，
r39d 因此改成"启动 → 轮询到 `instrumentacao completa` 出现 → 立刻读 → 再退出"，
而不是固定 `sleep 45`（第一次就是这么写的，正好错过）。

**r39d 还踩了两个自己的坑，都记下来**：

- `sed 's/.*hook: \([0-9a-f]*\).*/\1/'` 在 `hook: 0x7f996616c4 ->` 上
  匹配到 `0` 就停在 `x` 上，**提取出的是 `0`**，
  于是 `dd skip=0` 读的是偏移 0，却照样打印出一整行"像模像样"的字。
  ⇒ **错得像模像样，比完全读不到更危险。**
  改用 `awk '{print $3}'`（字段位置固定），并把这条写进脚本注释。
- 在 Windows 上用 Python 改 `.sh` 文件，文本模式写入会**把 LF 变成 CRLF**，
  设备上直接 `syntax error: "|" unexpected`（`\r` 变成了 token 的一部分）。
  ⇒ **改 shell 脚本后必须检查换行符**（`r39d.sh` 中招，其余 55 个脚本无辜）。

### run39f：字节级证明 hook 无罪，根因是空引用异常

**先修仪器**：`sed` 换成 `awk`、CRLF 转回 LF 之后重跑，得到**干净的数据**：

```
target 0x7f996616c4 : 58000050 d61f0200 5b089938 00000055
target 0x7f99661884 : 58000050 d61f0200 5b089990 00000055
target 0x7f99661068 : 58000050 d61f0200 5b0899e8 00000055
target 0x7f99669f7c : 58000050 d61f0200 5b089a40 00000055
target 0x7f99664f88 : 58000050 d61f0200 5b089a98 00000055

tramp  0x7fa1ce8000 : a9bc5ff8 a90157f6 a9024ff4 a9037bfd 58000050 d61f0200 996616d4 0000007f
tramp  0x7fa1ce7000 : f81c0ff7 a90157f6 a9024ff4 a9037bfd 58000050 d61f0200 99661894 0000007f
tramp  0x7f98014000 : d10183ff f9000bf9 a9025ff8 a90357f6 58000050 d61f0200 99661078 0000007f
tramp  0x7f98013000 : a9bd57f6 a9014ff4 a9027bfd 910083fd 58000050 d61f0200 99669f8c 0000007f
tramp  0x7f98012000 : a9ba6ffc a90167fa a9025ff8 a90357f6 58000050 d61f0200 99664f98 0000007f
```

5/5 target 全部是 `ldr x16,#8 ; br x16 ; .quad stub`；
5/5 trampoline 的回跳字（`996616d4` / `99661894` / `99661078` /
`99669f8c` / `99664f98`）**逐一等于对应 target + 0x10**。
**hook 机制在字节层面完全正确，无可指摘。**
（`Init` 命中的回溯帧 `0x555b089a0c` = `stub_init(0x555b0899e8) + 0x24`，
正是 `bl fp2_managed_event` 的返回地址 —— 我们的代码确实在跑。）

**于是矛盾是真的**，而答案就写在 `Init` 的生成代码里：
`0x600888` 不是一个普通函数，它是 **IL2CPP 的抛异常辅助函数**：

```
0x600888  sub sp, sp, #0x20
0x60088c  stp x29, x30, [sp, #0x10]
0x600890  add x29, sp, #0x10
0x600894  mov x0, sp
0x600898  stp xzr, xzr, [sp]      ← 在栈上构造异常对象
0x60089c  bl  #0x6008a0            ← 再交给运行时
```

`0x60156c` 同构，且带两个字符串元数据参数（`adrp x1/x2`）——
那是带消息的异常构造。

**`BootForm::Init` 里有九处这个模式**，全部形如：

```
cbnz  xN, #pula          ← 非空就跳过
mov   x0, xzr
bl    #0x600888          ← 为空就抛异常，方法当场结束
pula:
```

| # | 指令 | 被检查的来源 |
|---|---|---|
| 1 | `0x897148` | `x20 = [x19, #0x78]` |
| 2 | `0x897164` | `x20 = [x19, #0x70]` |
| 3 | `0x897198` | `x21 = [x19, #0x70]` |
| 4 | `0x8971a8` | `x21 = [x21, #0xc0]` |
| 5 | `0x8971b8` | `x21 = [x21, #0x10]` |
| 6 | `0x897210` | `x20` |
| 7 | `0x8973e8` | `x20 = [x19, #0x78]` |
| 8 | `0x897408` | `x20 = [x19, #0x78]` |
| 9 | `0x8974a0` | `x20` |

**全部九处都排在 `0x8974c4`（`bl ChangeState`）之前。**
任何一处命中，`ChangeState` 就永远不会被调用 ——
这精确解释了 run39a 以来"ChangeState 零调用"这个顽固现象，
**与 hook 机制无关**。

**"错误 100" 的真面目**：游戏用一个**全局异常处理器**接住这个
NullReferenceException，弹出 `showDialog(0, "Error", "An error has occurred.\n100")`。
所以**错误 100 不是"某个状态码 100"，而是"启动时的一次空引用异常"**。

**为什么状态字段（`[x19, #0x100]`）从来没被读到**：
它由 `ChangeState` 末尾的 `str w19, [x20, #0x100]` 写入，
而 `ChangeState` 在九处检查之后。异常一起，全链路止步。

**下一步（run39g）**：`il2_fields_foreach` 已经写好但**从未被调用过**。
现在把它接到 `Init` 的第一次命中上：打印 `BootForm` 全部字段的
**名字 + 偏移 + 当前值**，直接把"某处抛异常"换成
"偏移 +0x78 上那个字段是 NULL"。
为此给 `il2_field_cb` 增加了 `offset` 与 `value` 两个参数
（并新增可选符号 `il2cpp_field_get_offset`）——
没有偏移，字段名和 `ldr x20, [x19, #0x78]` 是两句对不上的话。

---

## run39g：基类遍历 —— 三个关键槽位现名

### 做了什么

两处代码：

1. `il2.c` 的 `il2_fields_foreach` 在派生类字段走完之后，
   **再沿 `class_get_parent` 爬整条基类链**（`BootForm` → … → `Object`）。
2. 删掉一处**重复声明**：`class_get_parent` 在第 41 行已由 `GET` 宏加载，
   我又在第 63 行声明、第 124 行 `nx_lookup_in` 了一次 —— 编译会死在重复成员上。

> 教训（和 run39b 的 `b` 超范围同类）：
> **加符号前先 grep 现有的加载点。** `GET` 宏区已经加载过的符号，
> 再加一条 `nx_lookup_in` 不会更好，只会更脆。

### 关键修正：基类遍历必须放在 while 循环**之后**

第一版把 climb 写在 `while ((f = class_get_fields(k, &iter)))` 的**循环体内**。
那样派生类每有一个字段，基类就被重打一遍 —— 13 个字段 = 基类重复 13 次。
放到循环外，并 `for` 整条链，不确定目标字段是否在第一层就成立。

### 结果：13 个字段 + 21 个基类字段，槽位全部对上

```
[fp2] state: #1 Init(-1608538192) this=0x7f696ae850 ?+0x0
[fp2] campo: ---- BootForm @ 0x7f696ae850 ----
[fp2] campo:  this+0xb8  frame_          NULL  <<<
[fp2] campo:  this+0xbc  loop_           NULL  <<<
[fp2] campo:  this+0xc0  bowWait_        NULL  <<<
[fp2] campo:  this+0xc4  loadMax_        NULL  <<<
[fp2] campo:  this+0xc8  loadCnt_        NULL  <<<
[fp2] campo:  this+0xcc  focus_          NULL  <<<
[fp2] campo:  this+0xd0  interstitial_   NULL  <<<
[fp2] campo:  this+0xd8  regist_         NULL  <<<
[fp2] campo:  this+0xe0  error_          NULL  <<<
[fp2] campo:  this+0xe8  th_             NULL  <<<
[fp2] campo:  this+0xf0  e_              NULL  <<<
[fp2] campo:  this+0xf8  resLoad_        NULL  <<<
[fp2] campo:  this+0x100 state_          NULL  <<<
```

基类（`namespace="form"`，21 个）—— **三个关键槽位在这里现名**：

| 偏移 | 字段名 | 状态 |
|---|---|---|
| `+0x68` | `app_` | NULL |
| **`+0x70`** | **`apdat_`** | NULL |
| **`+0x78`** | **`canvas_`** | NULL |
| **`+0x80`** | **`formMan_`** | NULL |
| `+0x88` | `view_` | NULL |
| `+0x90` | `pad_` | NULL |
| `+0x98` | `update_` | NULL |
| `+0x9c` | `draw_` | NULL |
| `+0xa0` | `dialog_` | NULL |
| `+0xa8` | `result_` | NULL |
| `+0xac` | `arwFrame_` | NULL |
| `+0xb0` | `drawWindow_tmp` | NULL |

更基类：`+0x10 id_`、`+0x18 leftSoftLabel_` **ok**、
`+0x20 rightSoftLabel_` **ok**、`+0x28 softLabel3_`、
`+0x30 softLabel4_`、`+0x38 formProcMode_`、
`+0x40 formManager_` **ok**、`+0x48 continuousUpdate_` **ok**、
`+0x4c frameRate_` **ok**、`+0x50 updateFrameRate_` **ok**、
`+0x54 frameSkip_` **ok**、`+0x58 <Components>k__BackingField` **ok**、
`+0x60 updated_`。

> **`ok` 的六个字段是关键的对照**：它们证明转储**不是**整体失真。
> 一个 34 字段的转储如果全部 NULL，最省事的解释是"我读错地址了"；
> 六个非 NULL 的字段排除了这个解释 —— 读的是真实对象，
> NULL 是**真的** NULL。

### 但转储在 `Init` 入口拍下，全部 NULL 是预期的

这就把 run39f 的一个隐患说清楚了：字段在 `Init` **入口**还没赋值，
所以"全 NULL"本身**不能**证明哪一处 `cbnz` 触发。
它只完成了**命名**这一步 —— 而命名恰恰是之前缺的那一半：
没有名字，`apdat_` 和 `ldr x20, [x19, #0x78]` 是两句对不上的话。

### 离线读码：`+0x70/+0x78/+0x80` 是 `Init` 自己造的

反汇编 `BootForm::Init`（`0x897068`）的前 90 条指令，链条完整：

```
0x8970ac  mov  x0, x19
0x8970b4  bl   #0x66f8d4          ← 基类 .ctor（Form 的构造）
0x8970d8  mov  x0, xzr
0x8970dc  bl   #0x7e3568          ← 静态构造器 A
0x8970e0  str  x0, [x19, #0x70]   ← this->apdat_  = A
0x897104  mov  x0, xzr
0x897108  bl   #0x77d878          ← 静态构造器 B
0x89710c  str  x0, [x19, #0x78]   ← this->canvas_ = B
0x897130  mov  x0, xzr
0x897134  bl   #0x7b08a8          ← 静态构造器 C
0x897138  ldr  x20, [x19, #0x78]  ← x20 = canvas_
0x89713c  str  x0, [x19, #0x80]   ← this->formMan_ = C
0x897140  cbnz x20, #0x89714c     ← ★ 第一处检查
0x897144  mov  x0, xzr
0x897148  bl   #0x600888          ← ★ 抛异常
```

**`+0x70/+0x78/+0x80` 不是外部传入的，是 `Init` 自己在开头造的。**
三个 `0x7e3568 / 0x77d878 / 0x7b08a8` 都是标准 IL2CPP 静态构造器包装：

```
stp  x20, x19, [sp, #-0x20]!      ← 序言
stp  x29, x30, [sp, #0x10]
add  x29, sp, #0x10
adrp x19, #0x1767000              ← 指向 .data 的元数据初始化守卫
ldrb w8, [x19, #0x242]            ← "这个类的 .cctor 跑过没有"
tbnz w8, #0, <pula>              ← 跑过就跳过
...  bl #0x5d9554                 ← 没跑过：初始化元数据
mov  w8, #1
strb w8, [x19, #0x242]            ← 立旗
pula:
...  bl #0x6007f8                 ← 真正抛
```

**于是第一个失败点被压缩到一句话**：
`0x897140` 的 `cbnz x20` 测试的是 30 条指令前 `bl 0x77d878` 的返回值 ——
**如果 `0x77d878` 返回 NULL，异常在 `Init` 的第 0xD8 字节处就抛了。**

### 下一步（run39h）：不再靠推断，直接读出调用点

到这里为止，九处检查**仍然是靠读码挑一个**。游戏有九个候选，
而外部症状对九个**完全一样**。这是推断，不是测量。

**测量方法很便宜**：`0x600888` 是普通函数，`bl 0x600888` 会把**返回地址**写进 `x30`。
stub 顶替它进入时，入口的 `x30` **就是**那条 `bl` 的下一条指令地址 ——
**精确指出是哪一处 `cbnz` 触发的**，不依赖 `backtrace()`
（这里只有 2 帧，IL2CPP 生成代码没有 unwind info）。

stub **必须把控制权交回 trampolim**，让异常照常抛出。
一个吞掉异常的 hook 会让错误对话框消失 ——
那是**消掉症状**，不是解释症状。

**能不能按名字 hook？不能。** 新写的 `tools/throw_sym.py` 在
`bbmeta/libil2cpp.so` 的 `.dynsym` 里查 `0x600800..0x600950` 窗口：
**0 个符号**。导出的 `il2cpp_raise_exception`（`0x5f5c64`）是**另一条**函数：

```
0x5f5c64  mov  x1, xzr
0x5f5c68  mov  x2, xzr
0x5f5c6c  b    #0x6007f8          ← 尾跳到别处，不是 0x600888
```

所以只能按 RVA 定位。**而纯 RVA 正是 run33 SIGSEGV 的成因**。
折中：**RVA + 首 4 条指令的机器码校验**，四个字读自二进制：

```
0x600888  sub  sp, sp, #0x20          0xd10083ff
0x60088c  stp  x29, x30, [sp, #0x10]   0xa9017bfd
0x600890  add  x29, sp, #0x10          0x910043fd
0x600894  mov  x0, sp                  0x910003e0
```

`sub sp` / `stp x29,x30` / `add x29` 是普通 C 函数的序言；
紧随的 `mov x0, sp` 说明**异常对象在栈上构造**。
不匹配就**拒绝 hook** —— 不往一个"我们不知道是什么"的函数上写 16 字节。

### run39h 的第一个坑：第六个 trampoline 槽位链接失败

新 stub 需要第六个槽位，声明和前五个一样用宏：

```c
#define FP2_TRAMP(sym) static void *fp2_watch_tramp_##sym
FP2_TRAMP(changestate);  /* ... */  FP2_TRAMP(xthrow);   /* 新增 */
```

链接器却报：

```
undefined reference to `fp2_watch_tramp_xthrow'
relocation R_AARCH64_ADR_PREL_PG_HI21 ... can not be used when making a shared object
```

**先排除了八种假设，一个一个用最小复现文件否掉**
（`tools/repro_throw_sym1..8.c`）：

| 假设 | 复现 | 结果 |
|---|---|---|
| `naked` 被 GCC 在 `-O2` 忽略 | — | 五个能工作的 stub 有同样警告，排除 |
| 指针需要 `volatile` | repro 2 | volatile 槽位反而是 `b`，排除 |
| 是 `-fPIE` 的问题 | repro 4 | `-fno-pie` 结果相同，排除 |
| 声明顺序 / 字典序 | repro 6、7 | repro 7 反转后 `U` 落到 `zzz`，不符合字典序规则，排除 |
| 传参形式（宏参数 vs 文本内联） | repro 7 | 排除 |
| `typedef` 同步断言 | repro 8 | 排除 |
| `visibility("hidden")` | repro 3 | 排除 |
| 声明位置不在宏组里 | — | 移进 `FP2_TRAMP` 组后仍 `U`，排除 |

走到第八步还排不掉，问题就说明**诊断的位置不对**。
`nm` 只告诉你"未定义"，不告诉你"为什么这个和那五个不一样"。

### 决定性的一步：`readelf -r`

```bash
aarch64-linux-gnu-readelf -r src/jni.o | grep -i tramp
```

- 五个能工作的槽位：**完全没有重定位记录**
- 出问题的第六个：`R_AARCH64_ADR_PRE` + `R_AARCH64_LDST64_`

同一个文件里，GCC 对六个同形声明给出了**两种不同的处理**。
既然内容无关、位置无关、顺序无关，差别就只剩**声明属性**。

### 答案：缺 `__attribute__((used))`

`tools/repro_throw_sym9.c` 在同一个文件里放四种声明，一次测完：

| 声明 | `nm` 结果 | 重定位 |
|---|---|---|
| `static void *slot_plain_static;` | `U` | 有 |
| `static void *slot_hidden_static __attribute__((hidden));` | `U` | 有 |
| `void *slot_hidden_global __attribute__((hidden));` | `C` | 有 |
| `static void *slot_used_static __attribute__((used));` | **`b`** | **无** |

**只有 `used` 产出无重定位的本地定义。**

原因：这些槽位**只被一个 `__asm__` 引用**。GCC 不会把
`__asm__` 里的符号名当成 C 层面的使用，于是认为这个变量可以丢弃；
在 `-fPIE` 下它就发出一个可抢占引用（需要在 PIE 中可被重定位到别的模块），
而 shared object 里的可重定位引用是危险的。加上 `used` 强制发射定义，
引用随之变成本地。

**应用到全部六个槽位。** 那五个本来也输出 `b`，
但走的是一条**代码里没有任何地方写下来**的路径 ——
而与第六个的比较证明这条路径并不可靠。

**验证**（一条命令）：

```bash
aarch64-linux-gnu-nm src/jni.o | grep fp2_watch_tramp
# 期望：六个 b，零个 U
aarch64-linux-gnu-readelf -r src/jni.o | grep -i tramp
# 期望：空
```

> 教训一：**`nm` 的 `U`/`b` 不足以定位问题，得看 `readelf -r` 的重定位。**
> 报错说"危险重定位"，答案就藏在重定位表里，可第一反应往往是去查符号名。
>
> 教训二：**一个只被 inline asm 引用的 `static` 变量，在 PIE 下需要 `used`。**
> 这不是 AArch64 特有的问题，五个"碰巧能工作"的槽位才是真正的隐患 ——
> 它们靠的是一个没人写下来的编译器行为。

### run39h 实机结果：hook 成功，第一次读出调用点

二进制 `3bb2c87b`（BuildID `57fec2ce...`）部署到 192.168.31.196。

**校验先过**：

```
[fp2] throw: prologo de 0x600888 confere (sub sp/stp/add/mov x0,sp)
[fp2] throw: auxiliar de excecao observado em 0x7f9f21e888 (tramp 0x7f8df9e000)
```

**然后出现两次调用**：

```
[fp2] throw: #1 chamada em libil2cpp.so+0x0
[fp2] throw: #2 chamada em libil2cpp.so+0x0
```

**偏移是 0。** 但这已经拿到两个事实：

1. **`0x600888` 确实是错误 100 的抛出点** —— 两次调用都紧邻
   `showDialog(100)`，此前离线推断的九个候选里，
   能被同一个辅助函数连续命中两次的路径浮出来了。
2. **stub 逻辑写错了，不是 hook 错了。**

### run39h 的第二个坑：stub 里保存了 x30，却没有把它交给 C

`bl fp2_throw_event` 传给 C 函数的是 **x0**。
而 x0 在这个入口是**被抛异常对象的指针**（一个堆地址，
在 `libil2cpp.so` 的 `base` 之下），不是返回地址。

`fp2_throw_event` 里算偏移的判断是：

```c
if (mod && (char *)x30 >= (char *)mod->base)
    off = ...;
```

条件不成立 → `off` 保持 0 → 两次都打印 `+0x0`。

修法是补一条 `mov x0, x19`：

```c
"mov  x19, x30
"          /* x19 = endereco de retorno */ "mov  x0, x19
"           /* x0 = o que o C le           */ "bl   fp2_throw_event
"                        ```

**这个坑对另外五个 stub 是隐形的。** 它们在入口就有
`self` 在 x0 里，正好是 C 函数要的东西，所以问题不出现 ——
**只有当"传给 C 的东西不是函数入口的 x0"时才会暴露。**

> 教训：naked stub 里，**保存到 callee-saved 寄存器的值不会自己到达 C 函数**。
> 要显式搬到 x0。这一点在与"入口参数刚好在 x0"的 stub 对照时看不出来，
> 只能靠读汇编发现。

### run39h 实测结果：抛出点**不在** `BootForm::Init`

修好上面那个 `mov x0, x21` 之后，偏移读出来了：

```
[fp2] throw: prologo de 0x600888 confere (sub sp/stp/add/mov x0,sp)
[fp2] throw: auxiliar de excecao observado em 0x7fa4162888
[fp2] throw: #1 chamada em libil2cpp.so+0xa4a674
[fp2] throw: #2 chamada em libil2cpp.so+0xa4a674
```

**`0xa4a674`，不是预测的 `0x897148`。**

`BootForm::Init` 在 `0x897068`，两者相距 **0x1B3F2C**。
**此前"错误 100 来自 `Init` 的九处 `cbnz`"这个推断，被测量推翻。**

#### 抛出点的代码

```
0xa4a664  ldr      x20, [x19, #0x18]     ; 取字段
0xa4a668  cbnz     x20, #0xa4a674        ; 判空
0xa4a66c  mov      x0, xzr
0xa4a670  bl       #0x600888             ; 抛异常
0xa4a674  mov      x0, x20               ; ← x30 读到的就是这个
```

`x30` 读数与 `bl` 的下一条指令**逐字节吻合** ⇒ 测量方法本身成立，
读数可信，只是被测对象和原先的假设不是同一个。

#### `0x600888` 是什么：`NullReferenceException`

它的实现里有两个 `.rodata` 字符串指针：

```
0x1115be5 → "System"
0x11b1efb → "NullReferenceException"
```

即 `il2cpp_codegen_raise_null_reference_exception()`。
它不在 `.dynsym`（`tools/throw_sym.py` 实测窗口内 0 符号），
所以按 RVA + 首 4 指令机器码定位是对的。

**结论：错误 100 是一个空引用。**

#### 这个函数不是托管方法

`0xa4a59c` 处的 `stp x29, x30` 不是函数入口：真实的 `stp x29, x30` 在
`0xa4a408`，而 `0xa4a59c` 是同一函数内部的第二个序言
（`mov x19, x0` 出现在那里，`x19` 就是 `this`）。

两条独立证据说明它**不在托管方法表里**：

| 判据 | 结果 |
|---|---|
| `tools/find_method_index.py` 查 43373 项方法指针表 | **无此地址** |
| 全量扫描所有 `bl` 指令找指向 `0xa4a59c` 的调用 | **0 处** |
| 同工具查已确认的 `BootForm::Init`（`0x897068`） | **命中 slot 27066**，邻居正是 run39 打印的 `.ctor`/`ToString`/`Update` |

第三条是对照实验：工具本身是对的。

⇒ **这是 IL2CPP 运行时代码，不是游戏自己的方法。**
`x19`（`this`）因此**未必是 `BootForm`**。

#### 剩下的两种可能

`Init` 入口的字段转储（run39g）里 `+0x18` 是基类的 `leftSoftLabel_`，
当时**非 NULL**。但抛出发生在 `Init` **之后**。

1. `self` 就是 `BootForm`，`leftSoftLabel_` 在 `Init` 过程中被置空
2. `self` 是另一个对象，`+0x18` 一直是 NULL

**离线无法区分**（方法表里没有这个地址）。
⇒ run39i：把 `x19` 一起传出来，问反射。**结果见下一节。**

### run39i：根因锁定 —— `kairo.unity.io.RecordStore.storage_` 是 NULL

run39h 回答了"在哪抛"。剩下的问题是"谁空的"，而**离线读码回答不了**：
抛出点不在托管方法表里，没有类名可查。

于是把 stub 改成同时传 `x30`（返回地址）与 `x19`（`this`），
再用 `il2_class_of()` 问反射。答案一行就出来了：

```
[fp2] throw: #1 chamada em libil2cpp.so+0xa4a674  self=0x7f75a272a0  self+0x18=(nil)
[fp2] campo: ---- kairo.unity.io.RecordStore @ 0x7f75a272a0 ----
[fp2] campo:  this+0x10  rsId_           ok
[fp2] campo:  this+0x18  storage_        NULL  <<<   ← 就是它
[fp2] campo:  this+0x20  encryptionKey_  NULL  <<<
```

**`RecordStore` 是存档后端。** `rsId_`（存档 ID）已就位，
但真正干活的 `storage_` 和 `encryptionKey_` 都是 NULL。

#### 完整因果链（全部实测，无推断环节）

```
Utility.existPreference / getPreference / putPreference / removePreference
        │  全部绑定到 j_Utility_rest —— existPreference 恒 false，
        │  getPreference 恒 null，putPreference 空实现
        ▼
RecordStore 建不出 storage_（和 encryptionKey_）
        ▼
storage_ == NULL
        ▼
0xa4a664  ldr x20, [x19, #0x18]   ; storage_
0xa4a668  cbnz x20, ...
0xa4a670  bl 0x600888             ; NullReferenceException
        ▼
游戏启动自检的 try/catch 捕获 → Utility.showDialog(0, "Error",
                                        "An error has occurred.\n100", null)
```

日志里的直接佐证：游戏解析了这些 MethodID
（`getPreferenceKeys` / `existPreference` / `getPreference` /
`putPreference` / `removePreference` / `saveToSd`），
但**一次都没有真正调用**——因为它们返回的是空实现，
游戏走到 `RecordStore` 就已经拿不到东西了。

#### 这一条同时解释了 run25 与 run39h 的冲突

`tools/meta_dump.py` 记录的 run25 结论是"错误 100 不是异常，
是游戏自检失败后主动调 `showDialog`"。run39h 测到
`NullReferenceException`。**两者都对**：

游戏的启动自检是 `try/catch`。空引用被游戏自己捕获，
再由它调用 `showDialog` 显示错误 100。

**推论（对验收很重要）**：修好 `Preference` 之后，
错误对话框**不会**消失——它本来就不是故障源。
**游戏会继续往下跑**。验收标准必须是"进入游戏主流程"，
而不是"看不到 100"。

#### 硬编码类名的教训（同一个错误，第二次犯）

run39g 的 `fp2_dump_bootform_fields()` 里写死了 `il2_class_by_name("BootForm")`。
run39i 把它改成 `il2_class_of(self)` 拿**对象自己的类**。

如果当时没改，这次日志会打出一份 `BootForm` 的字段表，
`+0x18` 显示 `leftSoftLabel_` 非空，看起来"这个对象没问题"——
**把"读错了对象"误报成"对象是好的"**。
方向反了，但一样是错的。

> 规则：**诊断代码永远不要假定对象的类型**。
> 拿得到就问反射；拿不到就明说"不知道"，不要用假设填空。


### run40：让转储说出值，并把方法表也打出来

run39i 只回答了"谁空的"。剩下"谁该填"，那是**代码**问题，不是数据问题。

两处改动：
- `fp2_dump_fields_cb` 从打印 `ok` 改成打印**指针**。`ok` 只回答
  "是不是空的"，把地址扔了，而地址才是能跨轮比较的东西。
- 字段转储之后追加**方法表**（`il2_methods_foreach`）。方法名在元数据里，
  地址在指针表里，`nm` 两个都拿不全；反射两个都有。

结果：`RecordStore` 的方法名 + RVA 全出来了，其中

```
.ctor            0xa49e5c / 0xa49ed0
SetPreferenceMode 0xa4ad38   <- 0 个调用点，从未被调用
Setup            0xa4adf8
GetNumRecords    0xa4a58c
OpenRecordStore  0xa4e63c / 0xa4e6a4
```

**抛出点 `0xa4a674` 落在 `GetNumRecords`（0xa4a58c）与
`SetPreferenceMode`（0xa4ad38）之间** —— 抛异常的是
`RecordStore.GetNumRecords()`，不是之前的任何猜测。

### run41：全局对象 `T`

扫描 RecordStore 代码区（0xa49e5c..0xa4ef40）里所有写 `[xN,#0x18]`
的指令，得四处；其中 `.ctor#1` 与 `Setup` 打的是**同一个槽**：

```
.ctor   adrp x8,#0x1644000; ldr x8,[x8,#0xf60]; ldr x8,[x8]
        ldr x8,[x8,#0xb8];  ldr x8,[x8,#0x18];  str x8,[x19,#0x18]
Setup   同上取到 +0xb8；str <0xa4a3fc 的返回值>,[x9,#0x18]
```

即：存在全局 `T`，**`Setup` 是唯一的写入者**，`.ctor` 只是拷贝。

run41 第一版**少解一级引用**（漏了 `ldr x8,[x8]`），读到 `(p+0xb8)`
那个恰好为 0 的字节，报出"全局对象从未构造"——错误结论来自读错对象。
run41b 修正后：

```
raiz: p=0x7facf4e0c0  s=*p=0x55ab650080  T=(s+0xb8)=0x7fab609cc0
raiz: T+0x13=154  T+0x18=(nil)  T+0x50=0x7f9a0d9c80
```

`T` 存在，`T+0x18` 是 NULL。`T+0x50` 已被赋值，而写 `+0x50` 的那条
指令（`0x772f14`）紧挨在 `bl Setup` **之前** —— 初始化流程走到了那儿。

> 教训：转储代码里的解引用级数必须逐条对照汇编数。
> 少一级不会报错，只会给你一个"看起来很合理"的假结论。

### run42：主动调用 `Setup()`

既然 `Setup` 是唯一写入者，自己调它既验证又修复：

```
setup: classe kairo.unity.io.RecordStore
setup: Setup() retornou (nil)
raiz: T+0x18=(nil)          <- 没填上
```

`Setup` **能被调用**（反射找到了静态 `Setup/0`），但执行后槽仍是空的。

### run43：探针装在写入之后，一次都没触发

hook `0xa4ae74`（`str x8,[x9,#0x18]` 的下一条，此时 `x8` 还是 factory
返回值）。探针装上，但**从不触发** —— `Setup` 根本没走到那一行。

这排除了"factory 返回 NULL"，因为读码说它对 `w0=4` 走跳转表 case 3、
分配后返回指针，只有 `w0=0` 或 `w0>4` 才到 `mov x22,xzr`。

`Setup` 提前离开的最可能位置是 `0xa4ae44 bl 0x5e7c3c`（类初始化）。

### run44：`usePreference_` —— 两条独立线索指向同一个字节

静态字段的 slot 列表（run40）给出：

```
rootStorage_     slot 0x18
usePreference_   slot 0x20
```

而 `GetNumRecords` 读的正是这两个偏移：

```
0xa4a62c  ldr  x8, [x8, #0xb8]    ; T（静态块）
0xa4a630  ldrb w8, [x8, #0x20]    ; usePreference_
0xa4a634  cbz  w8, #0xa4a664      ; 为 0 -> 走 storage_ 那条路
0xa4a664  ldr  x20, [x19, #0x18]  ; storage_（rootStorage_ 的拷贝）
0xa4a668  cbnz x20, ...
0xa4a670  bl   0x600888           ; NullReferenceException
```

**`usePreference_` 非 0 时，代码根本不碰 `storage_`。**

这同时解释了此前的"preference 方法一次都没被调用"：那从来不是证据，
游戏只是**没走到会去问的那一支**。

Kairosoft 的插件带了完整的 SharedPreferences 契约
（`existPreference` / `getPreference` / `putPreference` /
`removePreference`），本 port 也实现了 —— 走 preference 分支才是
Android 上的本意。

于是 run44 的实验最小：**把静态块 `T+0x20` 这一字节置 1**，打印前后值。
改的是一个静态字节，不是代码，前后值都落日志，可逆。

### run45：翻掉一个字节，异常从 2 次降到 1 次

run44 死在 SIGILL（signum 4）。原因是**入口探针本身**：

```
0xa4adf8  str  x19, [sp, #-0x20]!
0xa4adfc  stp  x29, x30, [sp, #0x10]
0xa4ae00  add  x29, sp, #0x10
0xa4ae04  adrp x19, #0x1767000   <- PC 相对
```

trampoline 把这四条搬到别处执行，而 `adrp` 是 PC 相对的 —— 在新地址上
`x19` 拿到的是**错的页**，紧接着的 `strb w8, [x19, #0xe8a]` 就写到了
不该写的地方。

> 铁律：**不要 hook 前四条指令里含 `adrp` / `adr` / 任何分支的地址。**
> 现有五个方法 stub 是碰巧安全，不是设计安全。

run45 撤掉两个探针，把置位挪到 throw 时刻（那时静态块必定已存在）：

```
usepref: usePreference_ 0 -> 1  (rootStorage_ T+0x18=(nil))
```

**旧值确实是 0** —— 假设被验证。而且 `throw:` 从 2 次降到 1 次。

### run46：在第一次 `GetNumRecords` 之前置位

run45 置位在 throw 处理里，也就是**第一次已经崩完**之后。run44 测过
静态块在 Config.init 时还没分配，所以单个早期点不够 —— 改为**沿途反复
尝试**：`getAppWidth/Height` → `getScaleRatio` → throw 兜底。

幂等，且**块为 NULL 时不能记为 done**，否则第一次空转会静默关掉后面
所有机会。

结果：

```
=== throw count ===
2
[fp2] throw: prologo de 0x600888 confere
[fp2] throw: auxiliar de excecao observado em 0x7f74e01888
```

两行**都是 hook 安装信息，没有一条 `throw: #1`**。空引用异常消失了。

#### 但错误 100 还在 —— 这本身就是结论

```
[fp2/sdk] Utility.showDialog(id=0, title="Error", msg="An error has occurred.
```

这印证了 run39i 的推论：对话框**不是故障源**，修掉空引用它也不会消失。
同时也说明自检**不止一个条件**：空引用只是第一个暴露出来的，后面还有
一个。日志里直接给出了下一个嫌疑：

```
[fp2] jni: Utility.putPreference() -> null (no such service on this device)
```

游戏在写存档，而 `putPreference` 是空实现。初始化数据存不下去，自检
自然还是失败。

#### 顺带的进展

- 游戏从"第 2 帧就退出"变成**跑满 45 秒**，加载资源、跑主循环
- `existPreference` / `getPreference` / `putPreference` / `removePreference`
  的 MethodID 全部被解析 —— 说明 preference 分支真的被走到了，
  此前"一次都没调用"是因为根本没走到那一支

### 下一步

实现真实的 preference 存储（内存 + 落盘），让 `existPreference` /
`getPreference` / `putPreference` / `removePreference` 有真实语义，
而不是恒 false / null / 空。
