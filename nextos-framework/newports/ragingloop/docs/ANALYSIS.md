# Raging Loop（人狼村之谜）Android → AArch64 Linux 移植分析

维护工作文档。所有结论都带**取证来源**（文件、字节偏移、日志行、命令输出），
未经实机验证的推测一律标注为「未验证」。续工先读这份，再读
`newports/60parsecs/docs/ANALYSIS.md` 的通用工程约束（两游戏共用同一套纪律）。

- 端口目录：`newports/ragingloop/`
- 输入（维护者提供，仓库外不上传）：`newports/ragingloop/apk/`（解包目录 +
  原始 `人狼村之谜ragingloop.apk` 754.7 MB）
- 测试设备：**ROCKNIX**，地址每次任务由维护者现场给出（`DEVICE_HOST=<ip>`，不写进仓库），
  合并式布局（`.sh` 与资源同在 `roms/ports/`）
- 语言：游戏脚本正文为**简体中文**（UTF-8），引擎内 UI 标签为日文

---

## 1. 引擎定性（阶段 A，全部实测）

**这不是 Unity 游戏。** 证据：

| 检查 | 结果 |
|---|---|
| APK 内 `lib/<abi>/` | 每个 ABI **只有一个** `libbridge.so`（arm64-v8a 1,727,760 B） |
| `libmain.so` / `libunity.so` / `libil2cpp.so` / `libmono.so` | **不存在** |
| `assets/bin/Data/` | **不存在** |
| `libbridge.so` 的 `DT_NEEDED` | 仅 `liblog.so`、`libc.so`、`libm.so`、`libdl.so` |
| `libbridge.so` 的 `nm -D` 导出 | 只有 `JNI_OnLoad` + 27 个 `.datadiv_decode*`（字符串混淆）+ 静态链入的 libc++；**`Java_*` 导出 = 0** |
| `libbridge.so` 未定义符号 | 66 个，全是 libc/bionic/`__android_log_print`；**无 EGL/GLES/SDL/OpenSL/AAudio** |

⇒ `libbridge.so` 是 `eagle/simple/sdks/bridge/*` 那套第三方 SDK 桥（classes3.dex 里
`Leagle/simple/sdks/bridge/{cdxz,flcj,gngx,gnsm,lmgx,zlcj}/Main;`），**不是游戏引擎**。

**引擎在 Java 层。** 证据（`tools/dex_scan.py`，输出 `logs/dex-scan.txt`）：

- `classes.dex`：strings=10396 / types=2004 / classes=1318 / methods=11580；
  包 `kemco/ragingloop/a` 有 157 个类（游戏本体），`classes2.dex` 有
  `com/haihai/SplashActivity`、`com/haihai/RestoreGame`（中文化移植方的启动与资源释放）
- 引擎用的 Android API（type 引用）：`Landroid/opengl/GLSurfaceView;`、
  `Ljavax/microedition/khronos/opengles/GL10;`（**GLES 1.x 固定管线**）、
  `Landroid/media/MediaPlayer;` + `Landroid/widget/VideoView;`（过场影片）、
  `Landroid/media/SoundPool;`（短音效）、`Landroid/content/res/AssetManager;`、
  `Landroid/content/SharedPreferences;`
- ⇒ 渲染、合成、文字、音频调度**全在 Java**；native 侧只提供 SDK 桥。

**结论：不存在「复用 .so + 假 JNI」这条路。** 60parsecs/bad2bad 那套 Unity loader 对本游戏
的唯一可复用部分是**工程纪律与工具链**（容器编译、原子部署、日志取证、阶段门禁），
不是运行时。可选路线只有三条：

1. 在设备上跑 Java 引擎（需要 Android framework 替身：GLSurfaceView/AssetManager/
   SharedPreferences/MediaPlayer/SoundPool/WebView…）→ 工程量与风险最高，且 1 GB 内存下 JVM 开销大。
2. 反编译并改写 Java → Kotlin/C 的机械翻译 → 依赖外部工具，产物不可控。
3. **自研原生 ADV 解释器**，直接吃游戏自己的资源与脚本 → 只在脚本是**明文、指令集封闭**时才可行。

选 3，依据见 §2。

## 2. 脚本与资源格式（阶段 B，核心证据）

### 2.1 脚本是明文指令语言

`assets/script/` 共 36 文件（`.txt` 27、`.csv` 8、`.DS_Store`）；
`scenario01_start.txt` … `scenario08_bad.txt` 为主线路径（16 KB … 1.30 MB），
另有 `extra01..05`、`gameover.txt`、`keylist.txt`、`credit.txt`、`gallery.txt`、
`download.txt`、`sounddef.txt`、`Clist00..07.txt`、`C00_01.csv..C07_01.csv`。

一行一条 `#Cmd(args)`，散文行为**裸文本**。全量普查（`tools/script_census.py` →
`logs/script-census.txt`）：

```
files=27  total_lines=258180  command_lines=201523  text_lines=56022
```

**指令集封闭且很小**（≈55 条，参数个数固定），完整计数见 `logs/script-census.txt`。
主要族（参数取值集合来自 `tools/map_check.py` + `logs/args-sets.txt`，n=出现次数）：

| 族 | 指令 | 实测参数域 |
|---|---|---|
| 立绘 | `ImgBustAdv(slot,bustId,x,y,a4,a5,a6)` | slot∈{1..7,9}；bustId={1..22,43..48,50,61,86}（31 个）；x∈[-270,912]；y∈[-559,470]；a4∈{0:8722, 15:2457}；**a5=a6=0 在 11177/11179 次调用中成立**（仅 2 行例外） |
| 表情 | `ImgFaceAdv(slot,brow,eye,mouth,acce,emo)` | brow 0..8；eye 0..15；mouth 0..8 加特值 40；acce 0..3；emo∈{-1,1..4} |
| 立绘几何 | `ImgScale(slot,pct)` pct∈{50..150}；`ImgBustXY(slot,ms,x,y,flag)`（492 次里只有 5 组 (x,y,flag)：(0,0,0)×388、(440,34,0)×50、(490,130,0)×29、(181,168,0)×26、(142,-30,0)×3；slot 分布 8:488/7:7/6:1 ⇒ 它是**全屏叠加层的绝对定位+补间**）；`ImgBustOffAdv(slot)`；`ImgAlpha(slot,50\|100)`；`ImgAnim(slot,a,b,ms,n)`（85 次，未实现，理由见 §2.10）。脚本里**确实有**
`#MoveTo`（4 次，见 §2.10 的纠正），但它不在 dex 解析器的 42 个指令名里 ⇒ 原版自己也忽略它 |
| 道具 | `ImgItem(slot,1..8,0\|1)` | 角色附件开关 |
| 背景 | `ImgBack(id)` id∈{1..111}∪{501..549}；`ImgBackOff()`；`BgColor(r,g,b)` | |
| 转场 | `WipeIn/WipeOut(mode)` mode∈{0,1,2}；`Flash(0\|1)`；`Quake(0)` | |
| 文本 | `MesName(说话人名)`、`MesSta(voiceId, speakerId)`、`MesWin(-1\|0\|1)`、`MesType(0\|1)`、`MesColor(0\|1)`、`MesEnd()`；裸文本行内 `%coN…%coe` 颜色标记 | voiceId 7579 个；speakerId 1..19 |
| 音频 | `Bgm(n)` n 1..57；`BgmIn/BgmOut/BgmStop`；`Se(n)` n 1..19；`Vib()` | |
| 流程 | `L(label)`、`Jump`、`JumpC(sel,label)`、`JumpF(file,label)`、`If(op,val)`、`Else`、`EndIf`、`Ret(n)`、`End()`、`Wait(frame)`、`Save()`、`Select`、`SelectLimited`、`Movie(name)`、`Staff()`、`data(day)` | |
| 变量 | `FlagCON/FlagCOFF(1..147)`、`FlagSON/FlagSOFF(11,12)`、`CmpC(n)`、`CmpS(n)`、`Analytics(...)` | |

### 2.2 id → 文件的映射已被**字节码里的常量表**证实

Java 引擎把资源索引表写成 `String[]` 字面量 ⇒ **数组顺序就是 id 顺序**。
`tools/dex_conststrings.py` 按 Dalvik 指令宽度表遍历 `code_item`，把
`Lkemco/ragingloop/b/f;.<clinit>` 的 578 条 `const-string` 按执行序取出
（`logs/table-f-clinit.txt`，分段见 `logs/table-slices.txt`）：

```
  0..110   background/bg_001 .. bg_111
111..159   background/event_01 .. event_49
160..192   chapter_00..chapter_14 / chapter_12d / chapter_12d2  (与日文标题成对)
194..202   unlock_01..05（与标题成对）
204..222   dead_list_01..10（与标题成对）
224..245   01/body_01 .. 22/body_01        ← 每角色 1 号服装
246..253   03,04,05,06,07,08,10,21 的 body_02
     254   06/body_03
255..301   popup/item_001..047
   ...     NN/K_brow、NN/K_eye、NN/K_mouth、NN/red、NN/black、NN/shadow、NN/acce_*
570..577   emotion/01..08
```

由此**闭合**三条最关键的映射：

1. **背景**：`ImgBack(id)`，`id≤500 → bg_%03d.jpg`（1..111 连续，条目数 111 与
   实测 id 上界 111 吻合）；`id≥501 → event_%02d.jpg`（501..549 共 49 个，
   表内 event 条目恰好 49 个）。
2. **立绘**：`bustId = 角色号 + 40×(服装号-1)`。
   31 个实测 bustId 中 1..22 → `NN/body_01`；43,44,45,46,47,48,50,61 →
   角色 03/04/05/06/07/08/10/21 的 `body_02`（表内 body_02 正好这 8 个角色）；
   86 → 角色 06 的 `body_03`。**九个小众 id 与表内 9 个非首选 body 一一对上，无剩余** ⇒ 公式确认。
3. **语音**：`MesSta(voiceId, speakerId)` 的 7579 个 voiceId 与
   `voice/NNNNN_NN.ogg` **100% 命中，缺失集合为空**（`logs/map-check.txt`）。

音频编号表 `assets/script/sounddef.txt`：`KIND,EXTERNAL,<name>,<index>`，
BGM 32 条 + SE 19 条；BGM 与 SE 各自从 1 编号（存在同名 index，属两个独立命名空间），
脚本用到的 31 个 Bgm id / 12 个 Se id **全部在表内**。

4. **表情层的锚点 = 立绘画布原点 (0,0)**（已闭合，此前是阶段 B 的唯一悬案）。
   `ImgFaceAdv(slot,brow,eye,mouth,acce,emo)` **不带任何坐标**，而同一角色的
   `K_brow/K_eye/K_mouth`（或 `K_face`）与 `black/red` 共享一张**远小于立绘**的画布
   （如角色 08：画布 389×292，`body_01` 714×778）。两种候选锚点用图像学方法判定：
   `tools/face_anchor_probe.py` 对 20 个角色目录做「部件 alpha 并集框」与
   「立绘上半部肤色像素框」的 IoU，**原点假设 17/20 优于水平居中假设**
   （角色 08 为 0.489 vs 0.000，角色 17 为 0.689 vs 0.020）；
   再用 `tools/composite_check.py` 把 `body_01 + brow + eye` 按 (0,0) 叠出
   `work/single_07.png`、`work/single_08.png`，**眉眼精确落在脸部原有位置**。
   ⇒ 结论：部件 PNG 自身的透明留白已编码了位置，引擎只需按 (0,0) 叠加，
   **不存在逐角色偏移表**。这也解释了为何部件画布尺寸逐角色不同（它是立绘左上角
   那块「头部区域」的裁剪）。

### 2.3 媒体与体积（`logs/assets-inventory.txt`）

| 项 | 数量 | 体积 | 容器 magic（实测） |
|---|---|---|---|
| `.ogg` | 7630 | 417.2 MB | `OggS`（全部 7630 个一致） |
| ├ `voice/` | 7581 | — | 主线台词配音 |
| └ `sound/` | 49 | — | BGM + SE |
| `.jpg` | 261 | 213.0 MB | 背景 207（`bg_*` 142 + `event_*` 65）+ thumb 52 |
| `.png` | 968 | 21.9 MB | 立绘部件、UI、popup |
| `.mp4` | 3 | 58.3 MB | `OP.mp4` 31.9 MB / `ED.mp4` 25.1 MB / `LOOP.mp4` |
| `.txt` | 27 | 5.4 MB | 脚本，UTF-8 |

**背景 CG 尺寸实测全部 1280×720**（206 个 + 1 个 1280×950）
⇒ 游戏原生渲染目标 = **1280×720**（不是设备屏的 4:3）。

立绘部件尺寸按角色而异（371×256、415×241、568×911…），
UI 元素为 40×40/60×60/100×30/180×70/220×90/320×80 等小图。

`RestoreToGame/` 是移植方（`com/haihai/RestoreGame`）用的**首启资源释放包**：
`external_data/files/{voice,sound,*.mp4}` + `data/shared_prefs/*.xml`。
原生引擎把这些当外部文件读；我们直接按路径读，不需要释放步骤。

`data/shared_prefs/RL.xml` 键（进度/设置的权威载体）：
`ACTIVATE BGM_VOLUME VIBRATION GLOBAL_NUM LABEL_NUM EPISODE_01..03 VOICE_NUM
LOW_TEXTURE FIRST_CHECK REVIEW_CHECK EPISODE_APPENDIX BUTTON_HIDE`；
`RL_SYSTEM.xml` 记录 `voice.zip/sound.zip/FILE_PATH/IS_EXTERNAL`（原版是下载制，
本包已内置）；`com.haihai.restore.xml` 记录 `start_count/restore_flag`。
⇒ 存档 = shared_prefs 的 int/string 集合 + 缩略图 `autosaveNN.png`。

### 2.4 立绘垂直锚点规则（阶段 13a 闭合，实机报告的「眼睛与人脸不匹配」根因之一）

`#ImgBustAdv(slot,bustId,x,y,a4,0,0)` 的 `x` 恒为**身体画布左边缘**，而 `y` 的含义由
第 5 个参数决定：

| a4 | 语义 | 全语料 y 分布 |
|---|---|---|
| 0 | y = 身体画布的**垂直中心** | 72..470（p10 444，中位 450） |
| 15 | y = 身体画布的**顶边** | -559..295（p10 36，中位 158） |

判据（`work/tmp_final_anchor.py`，按「头部是否落在消息框 `BOX_Y=496` 之上」逐条打分）：
双锚点规则整体命中 **99.4%**（两组各自 99.7% / 99.0%），而任何单一锚点（顶/中心/底）
最高只有 **79.0%** ⇒ 只有这个二分能解释数据。

推论与实现约束（`port/src/main.c: render_bust`）：
- 每帧从存储的锚点重算画布原点 `oy = a4 ? y : y - h*scale/200`，**不能**把缩放做成"移动原点"，
  否则紧跟其后的 `#ImgScale` 会把头推离锚点。
- 身体、`brow/eye/mouth`（或 chibi 的 `N_face`）、`acce_*` 都是**同一张身体画布的裁剪**，
  一律画在 `(slot.x, oy)`、各自像素尺寸 × `scale/100`（§2.2 第 4 条的 (0,0) 叠加规则）。

**部件索引覆盖审计**（`work/tmp_facecover.py` + `work/tmp_oob.py`，逐文件重置活动槽位、
`WipeOut`/`ImgBustOffAdv` 清槽，避免跨场景串状态）：
- chibi 角色 **13/14/15/17/21** 只有 `N_face.png`（整脸裁剪），且脚本里
  `brow==eye==mouth` **恒等**（例：ch13 的 288/113/15/61/22 次分别对应索引 0..4）；
  用到的索引全部有对应文件 ⇒ `res_part()` 用 `eye → N_face.png` 一条回退即可，**不需要钳制**。
- 角色 **18/19/22** 完全没有部件文件（脸直接画在 body 上），脚本只请求索引 0 ⇒ 该层不画是正确行为。
- 全语料真正越界的只有 **5 行**：ch05 `mouth 8` 与 `mouth 40`、ch11/ch12 `mouth 6`。
  资产不存在 ⇒ 与原版一样"该层缺失"，**不做钳制**（钳到最大可用索引会画出错误表情，比不画更糟）。

### 2.5 文字节奏与 OGG 声道（实机反馈「文字过快 / 声音快进」的两条根因）

- **打字机是玩家设置，不是脚本指令**。payload 里的原版偏好
  `save/shared_prefs/RL.xml` 带 `MESSAGE_SPEED=2`、`AUTO_SPEED=2`（同一目录还有
  `BGM_VOLUME`、`EPISODE_01..03`、`GLOBAL_NUM/LABEL_NUM/VOICE_NUM` 等进度位），全语料
  没有任何"每字延时"指令 ⇒ 端口把它做成运行期开关 `RL_TEXT_CPS`（默认 **20 字/秒**）。
  实现约束：切割只能落在字符或 `%` 标记的边界上（`main.c: markup_len`），否则
  `%co2` 会被露出半个标记；`#MesSta` 清文、`#MesEnd` 等待、按键先补全当前句再翻页。
- **声音快进 = 单声道被当立体声消费**。`ov_read()` 按文件**自身**声道返回字节，不做上混：
  `voice/` 7581 个里 **7568 个单声道**，`sound/` 49 个里 14 个单声道（实测头部统计）。
  旧代码把每帧固定算成 4 字节 ⇒ 单声道流读进 2N 帧只输出 N 帧，**正好 2 倍速**（音高一起升高，
  与用户描述一致）。修法：`Stream.ch` 取自 `ov_info()`，`bpf = ch>1 ? 4 : 2`，输出侧把单声道
  复制到左右两路（`audio.c: mix_stream` 与 SE 池 `decode_all`）。`RL_VERBOSE=1` 时每次开流
  打印 `audio open ch=.. rate=..`，可直接在日志里核对。

### 2.6 存档与续玩（阶段 15）

脚本侧只有 **4 处 `#Save()`**（`scenario01_start.txt:451/464`、`scenario05_rikako.txt:20262/31355`）
⇒ 它是**检查点**；真正的存档菜单在 Java 前端，脚本指令集里没有对应指令，端口拿不到菜单语义。
掌机没有键盘菜单，玩家说的"能存档"= 下次进来还在原地，所以：

| 时机 | 行为 |
|---|---|
| `#Save()` | 立刻写快照 |
| `#MesEnd` | 距上次写入 >20 s 才写（exFAT 写入是这里的成本项） |
| 任何退出路径（SELECT / ESC / `#End` / SDL_QUIT） | 再写一次 |
| 启动 | 自动读回；`RL_NEW` 强制新开局，`RL_SCRIPT`/`RL_NOSAVE`（无头工装）既不读也不写 |
| `#Select` 挂起时 | **不写**，避免把存档落在一个尚未决定的分支上 |

快照包含续玩所需的全部机器状态，纯文本 `key value`，先写 `.tmp` 再 `rename`（exFAT 上不会留半截文件）：
脚本栈 `(fname, pc)`、`C/S` 标志稀疏表、当前背景（存 root **相对**路径，目录迁移或大小写差异后仍可解析）、
色底、每个活动槽位的 `(bustId,x,y,scale,alpha,anchor,brow,eye,mouth,emo,acce)`（`acce` 由
`#ImgItem` 设置，不入档会让续玩后的角色缺掉道具）、说话人名、正文、
`mesvis/mescolor/fade/fade_to`、`wait/await/cmp`。读回时逐帧 `load_script` 后覆盖 `pc`，越界 `pc` 退回该脚本开头。

- 存档目录是 `$RL_ROOT/saves`，**不是 `save/`**：payload 里 `save/shared_prefs/` 是原版 Android 偏好，
  端口数据不能与它混在一起。
- `RlSlot.item[]`（`#ImgItem` 的附件槽）目前没有任何绘制路径引用 ⇒ 不入档；实现 `#ImgItem` 渲染时在该行补字段即可。

### 2.7 原版菜单与设置界面（用户报告「原版有菜单，端口还没有」后的取证）

**菜单不是 Android 控件，是 Java 引擎在同一块 GLSurfaceView 上贴图画的。**
证据：`opt_*` / `cfg_*` / `menu_*` / `chrt_*` / `b10_*` 这些名字以**裸字符串常量**出现在
`classes.dex` 字符串池（`opt_` 50 个、`cfg_` 32、`chrt_` 25、`skip_01/02`、`auto_01/02`、
`b_save_*`、`b_load_*`、`menu_settings`、`menu_bar`），而 `apk/res/layout/` 里没有任何对应布局；
贴图本体在 `assets/` 根目录（501 个文件，已随 payload 部署到设备）。

**游戏内菜单 = `cfg_L00..L12`**，13 条 890×90 的整行按钮，`_off`/`_on` 两态，图标烘焙在行左端：

| 贴图 | 原文 | 端口本轮 |
|---|---|---|
| cfg_L00 | ページ送り | 并入「选项设置」的文本推送速度 |
| cfg_L01 | オート進行ON/OFF | ✅ 自动推进 |
| cfg_L02 | スキップ開始 | ✅ 快进跳过 |
| cfg_L03 | ウィンドウ非表示 | ✅ 隐藏文本框 |
| cfg_L04 | ログ表示 | ⏳ 未做（要历史缓冲） |
| cfg_L05 | チャート（分支图，`chrt_*`） | ⏳ 未做 |
| cfg_L06 / L07 | セーブ / ロード | ✅ 保存进度（读档=启动自动续玩） |
| cfg_L08 | 次の選択肢へ | ✅ 跳到下个选项 |
| cfg_L09 | オプション | ✅ 选项设置页 |
| cfg_L10 | ヘルプ | ⏳ 未做 |
| cfg_L11 | タイトルへ戻る | ✅ 退出游戏（本端口无标题屏） |
| cfg_L12 | メニュー | = 本菜单自身 |

**设置页 = 4 个分类**（`opt_tl01..04`：文本 / 显示 / 全部 / 角色语音），行标签是
`opt_sys_l01..24`（340×50）与 `opt_sou_l01..03,15,16`，行底 `opt_w_00_off/on`（870×140，
选中态蓝色描边），分组条 `opt_w_01`（870×70 蓝色渐变），背景 `menu_back.jpg`。

**取值控件两类**（实测几何）：

- **五档滑条** `opt_bar_01/02/03`（460×80，凹槽实测 x=80..379 共 300 px）+ 滑块 `bar.png`（20×40）。
  三个滑条两端文字分别是 小/大、慢/快、浅/深 ⇒ 就是**字号 / 速度 / 浓度**三档控件。
- **开关与多态按钮** `b10_a`(ON/OFF) `b10_b`(上/下) `b10_c`(高/低) `b10_d`(仅已读/全部)
  `b10_g`(少/多) `b10_h/i/j`(无/中/大) `b10_n`(全屏/窗口)，以及圆点单选 `cfg_e01a`(未选)/`cfg_e01b`(已选)。

**设置项的载体与取值域已闭合**：`save/shared_prefs/RL.xml` 里
`MESSAGE_SPEED`（文本推送速度）、`AUTO_SPEED`（文本自动推进速度）、`OPACITY`（文本框透明度）、
`BGM_VOLUME`、`SE_VOLUME` 全是 int 且**出厂值一律为 2**；bool 项 `ALL_SKIP VIBRATION BUTTON_HIDE
BUTTON_ARRANGE LOW_TEXTURE RUBY_MODE SYSTEM_SOUND OPENING`，另有 `KEYCONFIG_NUM`（按键配置号）。
"出厂值 2" 与 "五档滑条中间档" 互相印证 ⇒ **取值域 0..4，2 = 中**。

**语音既有全局项也有逐角色项**（本轮更正：先前只看到 `opt_cha_l01..18` 就推断"没有全局语音音量"，
dex 字符串表里其实有 `VOICE_ALL`）：`opt_cha_l01..18`（380×140，角色头像 + 中文名，第 18 条是「其他」）
构成**角色语音**页，逐角色给档（与 `b10_h/i/j` 的 无/中/大 吻合）；
`#MesSta(voiceId, speakerId)` 的 speakerId 1..19 是它的索引来源。键名、行序对照表与推导过程见 §2.11。

其余 UI 归属：`b02a..b02k`（读取/记录/覆盖/跳转/阅读/返回标题/结束/初始化/取消）= 存读档对话框；
`tb01..tb05`（阅读/设定/额外内容/收藏/结束）= 标题主菜单；`b20/b21` = GALLERY / ENDLIST；
`f_title/f_select/f_maintxt/f_bar`（450×440 等）= ログ 回想日志面板。

**中文补丁只翻了部分 UI**：`opt_sys_l02..09/13..15/17/22/24`、`opt_sou_l02/03/15/16`、
`opt_tl01..04`、`opt_cha_*`、`b02*`、`b10_*`、`tb0*` 已是中文，而 **`cfg_L00..12`（游戏内菜单 13 条）、
`opt_sys_l01/10/11/16/19`、`opt_sou_l01` 仍是日文** ⇒ 端口的菜单改成「**原贴图做行底 + 我们自己的
中文字体画标签**」，直接用 `cfg_L*` 会让中文玩家看到日文；设置页的滑条两端文字（小/大、慢/快、浅/深）
用原图，因为那三对字就是设计本身。

**端口实现（本轮，`main.c` 的 options/menu 块）**：

- 打开方式 **L1**（原版是屏幕上的触摸按钮，掌机没有触点；SELECT 保留为直接退出，
  避免叠加层出问题时玩家被关在里面）。
- 菜单 8 条：继续游戏 / 选项设置 / 自动推进 / 快进跳过 / 跳到下个选项 / 隐藏文本框 / 保存进度 / 退出游戏。
- 设置页 6 行，出厂档一律 idx=2（= 上一轮用户实机认可的观感：20 字/s、34 px、0xC8 底）：

| 行 | 控件 | 五档取值 |
|---|---|---|
| 文本推送速度 | `opt_bar_02` 慢/快 | 8 / 14 / 20 / 28 / 40 字每秒 |
| 文本自动推进速度 | `opt_bar_02` | 4000 / 3000 / 2000 / 1200 / 667 ms |
| 字体大小 | `opt_bar_01` 小/大 | 28 / 31 / 34 / 38 / 42 px（名牌同步 −4 px） |
| 文本框浓度 | `opt_bar_03` 浅/深 | alpha 0x50 / 0x8C / 0xC8 / 0xE4 / 0xF4 |
| 音乐音量 | `opt_bar_01` 小/大 | 0 / .30 / .60 / .85 / 1.00 增益 |
| 音效音量 | `opt_bar_01` 小/大 | 同上（语音不受这两行影响，见上） |

- 设置写 `saves/ragingloop.cfg`（**与存档分开**，对应原版"偏好与进度分家"）；改一档即落盘，
  退出时也落盘。菜单打开期间 `tick()` 不执行 ⇒ 游戏暂停，BGM 继续（与原版一致）。
- 「跳到下个选项」= 强制快进直到 `#Select` 出现，兜底 600 s 放弃并记日志。

**无头验证（`RL_MENU=1/2` + `RL_SNAP_DIR`，实机设备跑）**：

- 两页截图均**零 `MISS ui`** ⇒ 用到的 6 张原贴图（`opt_w_00_off/on`、`opt_bar_01/02/03`、`bar.png`）全部解析成功。
- 取值域闭环：把 cfg 写成 `opt 0 4 / 1 0 / 2 4 / 3 0 / 4 0 / 5 4`，日志回显
  `opt: cps=40 auto=120 font=42 shade=50 bgm=0 se=4` ⇒ 解析→应用→绘图一条链正确；
  `flags 1 1 0` 后菜单第 3/4 行确实显示 `ON`，即偏好与开关都能跨进程往返。
- 滑条几何用截图像素定量核对：凹槽在贴图内 x=80..379（300 px），句柄按 `bx+80+idx*70` 画 ⇒
  idx=0 占 80..99、idx=4 占 360..379，**两端都内缩在凹槽内且左右对称**，不需要再调步进。
- 本轮修掉一个集成缺陷：「保存进度」原先只写档不关菜单，而菜单期间 `tick()` 冻结 ⇒
  "已保存"提示既不倒计时也看不见。改为存完即 `menu_close()`。

### 2.8 六个未移植子系统的取证（用户列出「回想日志、章节图表、多槽存读档、按键配置、按角色语音、标题主菜单」后）

**标题主菜单**：`title_back.jpg` **1280×720**（正好等于端口内部分辨率，无需换算）、`logo_large.png` 640×266、
`title_talisman_flash.png` 330×240（符咒闪光）。五个发光文字按钮 `tb01..tb05`（240×70，`_off`/`_on`，
`tb03` 另有 `_gray`）实测内容 = **阅读 / 设定 / 额外内容 / 收藏 / 结束**。`tb03_gray` 证明原版自己就把
未解锁项画灰，所以端口把没移植的项画灰是**忠实还原**而不是占位糊弄。

**章节图表是数据驱动的**（这一条改变了可行性判断）：`assets/script/C00_01.csv … C07_01.csv` 共 8 份
（81 B ~ 4.7 KB）。一行 = 图的一**行**，行内以逗号分**多列**，每列一个节点记录：

```
日期|脚本文件|中文标题|结局旗标|类型|出度|b
5月12日(月)|scenario02_victim|Ｑ．是否在茅厕过夜|L03L08L19L20|select|2|1
```

- 类型 ∈ `normal` / `select` / `end`；第 4 列是 `-1`、`L04`、或拼接的 `L03L08L19L20`，结局节点用 `K01/K02/K03`
  ——它是**结局/旗标标签**，不是"前驱节点"（前驱由行网格给出，见 §2.13）。
- 紧跟其后的数字行（`1,` / `5,3,`）是这一行的**连线**信息：每条出边一个样式码，详见 §2.13。
- 首尾的 `prev|C01_01|scenario01_start|Ｑ．唤醒记忆` / `next|C02_01|scenario03_chiemi|明暗` 是**跨章翻页**链接。
- ⇒ 图表可完整还原；**并且这份表同时给出每个场景的中文标题与游戏内日期**，正是存档槽与回想日志要显示的
  内容（对应 dex 里的 `LAST_LABEL_TITLE`）。做存档/回想时应复用它，不要另造数据。

**按键配置的定义域已完整拿到**：dex 常量 `KEY_MENU_{AUTO,CHART,CLICK,HELP,HIDE,JUMP,LOAD,LOG,OPEN,OPTION,SAVE,SKIP,TITLE}`
**正好 13 个**，与快捷菜单 13 行 `cfg_L00..L12` 一一对应；偏好键为 `KEYCONFIG_MENU_<n>` / `KEYCONFIG_KEYCODE_<n>`，
`KEYCONFIG_NUM` 出厂 `0` = 未自定义。⇒ 原版语义是"给 13 个菜单动作各绑一个 Android keycode"。掌机无键盘，
这一项**价值最低**，排在最后。（最终按"保留能力、换掉载体"移植成手柄键位绑定，见 §2.14。）

**存档槽**：`assets/autosave01..04.png`（**60×60**）+ dex 前缀 `SLOT_` / `_SLOT_` + `b_save_{off,on,gray}` +
`b02a..b02k`（存读档对话框按钮，已确认含 读取/记录/覆盖/跳转/阅读/返回标题/结束/初始化/取消）。
⇒ 原版有 **4 个自动存档位**，外加手动槽；`autosaveNN.png` 是槽位图标而非缩略图。

**引擎内部代号**：dex 里的日志标签 `Togabito SaveData` / `Togabito:Message`（Togabito = 罪人）。
以后在原始 APK 里定位某个子系统，先 grep 这两个标签附近的字符串。

**按角色语音**：`opt_cha_l01..18`（380×140，立绘 + 中文名，#18 = 其他）就是它的 UI；取值键不在出厂
`RL.xml` 里 ⇒ 出厂未自定义，端口用 `#MesSta` 的 speakerId(1..19) 作索引、按 1..18→行、19→其他 落盘即可。

**出厂偏好实样**：`assets/RestoreToGame/data/shared_prefs/RL.xml` 是**随包附带的出厂 pref**（不是玩家数据），
逐项可查：`MESSAGE_SPEED/AUTO_SPEED/OPACITY/BGM_VOLUME/SE_VOLUME` 全 = 2、`KEYCONFIG_NUM` = 0、
`RUBY_MODE` = true、`VIBRATION` = true、`OPENING` = true、`FIRST_CHECK` = true、`VERSION` = 112、
`EPISODE_01..05`/`EPISODE_APPENDIX` 全 false、`VOICE_NUM/CHAPTER_NUM/LABEL_NUM/GLOBAL_NUM` 全 0。
⇒ 收藏（`VOICE_NUM` 等）是**解锁计数器**，端口没有对应内容可解锁，故标题页把「收藏」画灰是正确行为。

**回想日志面板是"空框 + 自绘文字"**：`f_title_frame.png` 450×640、`f_maintxt.png` 440×530、
`f_bar.png` 40×440 / `f_bar_select.png` 40×540、`f_select.png` 450×540、`f_title.png` 450×440。
把六张图并排看，**全部只是带细亮边框的深色半透明矩形，没有任何烤进去的文字** ⇒ 与 `cfg_L*`/`opt_bar_*`
不同，日志面板可以直接拿原图当框、文字全由端口自绘。宽度 450+440+40=930，放进 1280 有富余。

**`tb0N_on` / `tb0N_off` 的语义与直觉相反（实测像素）**：`_off` 平均亮度 137~148，`_on` 只有 99~101，
`_on` 反而**更暗**（但饱和度更高：98.7 → 112.7）。原因是原版标题页是**触摸驱动**的，
`_off` 是静止外观、`_on` 是按下瞬间的外观，手机上**根本不需要"焦点"这一态**。
⇒ 掌机用方向键导航时必须自己造焦点：端口用**不透明度**承载游标（选中行 100%，未选 45%，
不可用 25%），实测截图里游标一眼可辨。`tb03_gray.png`（饱和度 0、纯灰、alpha 像素反而更多）
是原版自带的"未解锁"外观，端口把它用于「额外内容」，与原版语义一致。

**`title_talisman_flash.png` 端口不画，这是有依据的决定**：该 330×240 图是符咒的发光版，
理论上应与 `title_back.jpg` 里烤进去的符咒重合。但归一化互相关会被右侧大片亮色头发带偏
（峰值 0.904 落在 x=624，肉眼一看就是错的），改用**零均值 NCC** 后峰值只有 0.228 且贴在搜索框边界上
⇒ 位置**无法从素材推出**。错位叠加会画出一对"重影符咒"，是明显缺陷；少一个装饰性脉冲不是。
所以端口只画 `title_back.jpg`，不猜这个坐标。

### 2.9 回想日志（#23）：取证 → 实现 → 设备自测结果

**素材归属这条仍是推断，不是 dex 证据**。`logs/dex-strings.txt` 里 grep 不到 `f_select` /
`f_maintxt` / `backlog` / `LogView` 任何一项（那份表主要是 AppCompat 资源名，引擎自己的 view
类名不在其中）。所以「`f_*` 这套 = 回想日志面板」的依据只有两条：① 它是全 APK 里**唯一一组
纯空框**（六张图并排看只有细亮边框的深色半透明矩形，没有任何烤进去的文字，见 §2.8），
② 宽度 450+40+440=930 与"左侧列表 + 滚动条 + 右侧正文"的三栏结构吻合。
⇒ 若实机进面板后发现原版其实是两栏或带缩略图，这一节要按实况改写。

**分组口径是从脚本里读出来的，不是猜的**：引擎用 `#L(name)` 标记一个场景节点的开始
（`main.c` 里 `g_log_node` 记录"最近走过的 `#L()`"），连续共享同一 node 的消息 = 一组 =
列表里的一行。两条已验证的推论：
- `#L()` 的标题串与 `C0x_yy.csv` 的**中文标题列完全一致** ⇒ 端口不需要新翻译就能列出与原版
  同一套场景名（这是 §2.8 "复用图表数据、不另造"那条的实际兑现）。
- 同一 node 被走两次会出**两组**（回放/回溯时），因为它们是剧情的不同段落 ⇒ 分组键是
  "第几次访问"而不是 node 名本身，`grp_build()` 按 `first/n` 连续段构造，天然满足。

**面板几何全部由 `f_*` 原图尺寸反推，不靠肉眼调数**：`f_select` 450×540 落在 (175,90)，
`f_bar_select` 40×540 落在 (625,90)，`f_maintxt` 440×530 落在 (665,95)（比左右两栏矮 10 px，
垂直居中）。1280 内部宽度下 175 + 930 + 175 恰好对称，故 `LGR_X=175`、`LGT_X=665`、`LG_Y=90`。
正文区不再量一遍：`LGT_HDR_H=79` / `LGT_BOT=449` 是 `f_maintxt` 自身两条发丝线的 y 位置，
行高 `LGT_PX=26`、行距 +12，字宽 `440-48=392`。列表 13 行 × 40 px = 520，塞进 540 的框。
`LGT_PX` 与设置页的"字体大小"**故意不联动**——回看面板是固定版式，跟着字号缩会撞破那两个发丝线。

**落盘格式与存进度共用一套转义**（`RLOG 1` 头 + `node `/`name `/`text ` 三行一条，
`esc_write`/`esc_read`，先写 `.tmp` 再 `rename`）：
- 文件是 `saves/ragingloop.log`，与 `ragingloop.save`/`ragingloop.cfg` 同目录。
- **只在显式保存 / 返回标题 / 退出时写**，不做逐条即时写。原因：自动存档节流是
  `SAVE_STEP_MS=20000`，而面板记录满仓可到 ~300 KB，写 1 GB RAM + exFAT 的机器上这个频率会
  把帧时间抖出来。存档动作本身就是玩家认为"这里值得记"的时刻。
- `g_log_node` 在读档时从最后一行恢复 ⇒ 续玩的第一条消息接在**同一组**里，不会凭空多出一组。

**设备无头自测结果（用下面的三个环境变量入口跑的探针脚本；脚本本体是 `tools/tmp_*`，
按仓库约定不进版本库）**：

| 判据 | 实测 |
|---|---|
| 面板渲染缺图 | `MISS=0`（每次运行） |
| 分页内容随页变化 | `group 1: 33 msgs 16 pages`，抽 1/16 与 16/16 两页像素不同 |
| 空状态 | 显示"还没有可以回想的记录"，提示行在左下（本轮修前误画成标题位） |
| 落盘/读回 | 字节级一致：`wc -c` 2577/2577，`diff` 空，设备端 python3 逐字节走查首个差异在第 2577 字节（= 文件尾）。busybox **没有 `cmp`**，别把"cmp 报 no such file"当成不一致 |
| 续玩不重复记账 | 退出前 23 条 → 读档后仍 23 条 |
| `RL_NOSAVE` | 生效，测试没往 `saves/` 留文件 |
| 返回标题安全 | `RL_RESET=120` → `BGM stop` → `back to title` → 再跑 60 帧 rc=0、无 segfault |
| 玩家进度 | 每轮前后 `saves/ragingloop.save` md5 恒为 `2e7295554d3179ab247d101a60383f5a`，`saves/` 文件数不变 |

**为面板加的两个无头入口**（dummy 驱动喂不进按键，没有它们这些状态只能靠玩家手测）：
`RL_BACKLOG=<帧>` 在该帧拉起面板，`RL_LOG_AT=<组>.<页>` 直接跳到指定组/页截图；
`RL_RESET=<帧>` 在该帧执行 `game_reset()`。注意截图帧必须取 `RL_BACKLOG + 1`——
主循环顺序是 `handle_pad → RL_BACKLOG 检查 → tick → render → g_frames++ → save_snapshot`，
同帧号拿到的是面板出现**之前**的画面（这个坑踩过两次，表现为两张 shot 完全一样）。

**尚未闭环的一条**：面板内的方向键导航（上下选段 / 左右翻组 / A 下一页 / B 返回）**无法由我自测**，
按交接约定这一项属于玩家实机判断。所以下面这几处只做了逻辑推演，未经真实事件验证：
`log_nav` 的 `hx` 一次跳 `LG_ROWS` 组（当作翻页条用）、游标只在 `log_nav` 里 clamp 而
`draw_log` 再把越界的 `g_log_pg` 拉回最后一页（这样列表滚动和正文分页能各自独立收敛）。

### 2.10 纹理上限实测（#19）与脸部叠加层取证（#17）

**#19 结论：LRU 早已落地（`img.c` 的哨兵双向链表 + `evict_until()`），本轮只是把"需要 LRU"
这条过期判断改成实测数据。** 无头跑 `scenario02_victim` 2400 帧，三档上限对照：

| 上限 | evicts | 结束时缓存 | 2400 帧耗时 | 缺图/解码失败 |
|---|---|---|---|---|
| 48 MB（默认） | 6 | 50.27 / 50.33 MB | 185 s | 0 / 0 |
| `RL_TEXCAP=8` | 2807 | 7.42 / 8.39 MB | 255 s | 0 / 0 |
| `RL_TEXCAP=4` | 6384 | 3.21 / 4.19 MB | 413 s | 0 / 0 |

⇒ 上限**严格守得住**（三档都没越界），淘汰有代价但默认档几乎不触发（6 次/2400 帧），
所以 48 MB 是正确工作点，不必上调：设备只有 ~597 MB 可用内存，缓存再大一格就是拿 OOM 风险
换 0 次淘汰。注意这 13.0 fps 是 dummy 驱动 + 逐帧解码的**无头开销**，与实机 30.5 fps 不同源，
不要拿它当帧率证据；三档之间的**相对**耗时才是淘汰成本的度量。

**#17 取证推翻了我之前的一处对应关系（这是真 bug，不是缺功能）**：
`#ImgFaceAdv(slot,brow,eye,mouth,p4,emo)` 的 **p4 不是附件索引，而是脸部叠加层选择子**。
三条独立证据：
1. **常量表里只有这三个名字**。角色目录下的 `red.png / black.png / shadow.png` 是**无下标的单文件**
   （不像 `N_brow` 那样成组），而 dex 常量表 `Lkemco/ragingloop/b/f;.<clinit>` 的叠加层区间
   （636..662，635 是空串）里就只出现这三个名字 ⇒ 只能按下标选中，且值域必然是 1..3。
   实测 p4 值域正是 `{0,1,2,3}`。
2. **画布同尺寸 ⇒ 属脸部层**。02 的 `red/black` 与其 `K_brow/K_eye/K_mouth` 同为 272×265、
   03 为 298×290、04 为 415×241、08 为 389×292，而 `acce_*` 尺寸各异 ⇒ 叠加层沿用 §2.4 的
   (0,0) 锚点规则，与附件不是一类东西。
3. **附件那条路被 p4 挤掉了**。`#ImgItem(slot,k,on)` 的 k 才是附件序号，且 21/22 个角色的
   `max(k)` 恰等于该角色 `acce_*` 文件数（01→5、02→3、03→4、06→8、16→6、22→4），
   而 `on∈{0,1}` 是**每个 k 独立的开关** ⇒ 单值字段根本表达不了（关一个会把另一个也清掉）。
   端口此前把 ImgFaceAdv 的 p4 和 ImgItem 都写进同一个 `sl->acce`，等于
   ① 该画红脸/阴黑脸/影子的 49+ 处全画成了不相干的附件，② 每次表情更新都覆盖玩家的附件状态。

**p4 的取值映射（按脚本语境判定，非猜）**：`1 = red`、`2 = shadow`、`3 = black`。
- `2` **只能是 shadow**：全 22 个角色里只有 03 号目录带 `shadow.png`，而 p4=2 恰好只在 03 号身上出现
  （`scenario02_victim.txt:317/457/462/500/648`）。
- `1` 落在发火/羞愤的台词上（`extra02_kiyorika.txt:774`「急火攻心！！」、`extra01_haruchie.txt:245`
  「这台淫荡的引擎！」）⇒ red（涨红）。
- `3` 落在沉脸/无语的反应上（`scenario02_victim.txt:1120`「她好像很不耐烦的样子／一脸怒气值+20点的表情」、
  `:1087`「……好快」）⇒ black（阴沉）。
  ⚠ 1↔3 这一对**没有存在性证据**（带叠加层的角色一律同时有 `red` 和 `black`，只有 11/14 只有 black
  且他们 p4 从未非零），靠的是语境；若实机看到"发火时脸发黑"就是这俩反了，改 `overs[]` 一行即可。

**新旧二进制逐帧像素对照（本轮已做，#17 由此从"应该对"变成"实测对"）**：
样本帧 = `scenario02_victim` 第 730 帧，槽 7 = `#ImgBustAdv(7,43,373,-4,15,0,0)`（43 ⇒ 03 号角色
2 号变体）+ `#ImgFaceAdv(7,1,2,0,3,3)` + `#ImgItem(7,4,1)` + `#ImgItem(7,1,1)`，即 p4=3 与两个附件
同时在场的最坏情况。03 号目录实有 `acce_01 / acce_02_tabaco0 / acce_03_tabaco1234 / acce_04 /
black / red / shadow`。

| 侧 | 画出来的东西 |
|---|---|
| 旧（单 `acce` 字段） | 只有 `acce_01`：`ImgItem(…,4,1)` 先写 4，紧接着 `ImgItem(…,1,1)` 覆盖成 1，
  帽子（`acce_04`）被后一条指令吃掉；`black.png` 永远不画 |
| 新（`item[9]` + `faceover`） | `acce_04`（遮阳帽）+ `acce_01` 同时在场，头顶再叠一层 `black.png` 阴翳 |

定量：`diff px=15066 (1.635 %) meanDelta=60.9 max=251 bbox=(480,98)-(660,243)`，
700/712/730/758 四帧**数值完全一致** ⇒ 差异是一整层静态叠加，不是文字逐字显现的抖动；
bbox 恰好落在头部（180×145），没有溢出到背景或文本框。
⚠ 方法学坑：BMP 是无损定长（1280×720×4+122 = 3686522 B），**新旧文件大小相同不构成"画面相同"的证据**，
必须逐像素比（`tools/tmp_diff.py`，取 `ImageChops.difference` 的 bbox/直方图，别用双层 Python 循环）。

晋级与续档：`ragingloop.bin` → `ragingloop`（sha256 `0a54449e…`，旧的留作 `ragingloop.prev`）。
玩家原有档位是 **`RLSAVE 1`**（`2 frame / 4 c / bg / color / name / text / view / state`，无 slot 行），
`RL_TITLE=0` 强制走剧情路径后确认 `save: resumed scenario02_victim.txt:5124 depth=2` +
`log: 177 entries resumed`，退出时自动写成 `RLSAVE 2`（v1→v2 迁移路径实测可用）；
测试前后玩家档位 md5 复原为 `e33a62219ecc9d0ce3fe78dcacbdfb51`。

**`acce_NN_tear<数字集>` / `_tabaco<数字集>` 的选取键仍未定案**（本轮把 tear 继续挂在 mouth 上）。
可判定的量都相同：带 tear 文件的角色里 brow 数与 mouth 数**处处相等**（02 7/7、03 5/5、04 9/9、
06 8/8、10 6/6），数字集（如 06 的 `acce_04_tear012346` + `acce_05_tear5` + `acce_06_tear7` 并集 0..7）
同时等于这两个数 ⇒ 素材层面无法区分 mouth 还是 brow。要定案只能找一幕 **brow 与 mouth 取值分属
不同侧**的帧（brow∈数字集而 mouth∉，或反之）实机对照，或反汇编脸部绘制函数。

**`#ImgAnim(slot,a,b,ms,n)` 保持不实现，理由已换成正确的那条**：它只在 `gameover.txt`（69 次）
与 `extra05_mitsuji.txt`、以及主线 14 处出现；84/85 次的参数是 `(7|6,2,2,100,10)`，
`a==b` **恒成立**（单帧退化循环，不是多帧动画），且被调用的槽位永远是 18 号或 22 号角色目录——
而这两个目录**只有 `body_01` + `acce_01..04`，没有任何脸部部件**，前置序列是
`ImgBustAdv → ImgScale → ImgFaceAdv(0,0,0,0,-1)`。⇒ 它极可能是"给没有表情层的角色做一段
短促抖动/闪烁"，效果无法从素材推出。唯一特例 `gameover.txt:2297 (7,3,3,1,4)` 紧跟
`#MoveTo(7,10,320,144)` + `#Wait(10)`，更像配合位移的抖动。硬凑一个循环就是造假。

**顺带纠正两处过期文档**：
- `#MoveTo` **确实在脚本里**（4 次：`gameover.txt:2295,4723`、`scenario07_last.txt:36601,36604`），
  §2.2 之前写"普查 0 次"是错的。但 dex 解析器的指令名表只有 42 个 `#Token`，
  **不含 `#MoveTo / #WipeIn / #WipeOut / #Flash / #Movie`** ⇒ Android 版引擎自己也不认，
  静默忽略。端口把它们当 no-op 是**忠实还原**，理由要按这条改写。
- `logs/table-f-clinit.txt` 那份常量表**不可用于下标定位**（`dex_conststrings.py` 的 Dalvik
  宽度表有误：`const-string 0x1A` 应为 2 单位、字段指令 0x4D-0x68 为 2、`invoke 0x6E-0x78` 为 3；
  改正后全 dex 精确对齐率 4287/7985 → 6842/7985，真实表 **747 条**而不是 578 条）。
  端口不受影响——`res.c` 走的是**命名规则**（`assets/%02d/…`）而不是表下标。

### 2.11 设置页四段结构与逐角色语音（#24 取证）

**设置页不是"一屏滑杆"，而是四段**，段名直接烤在素材里（`opt_tl01..04`，460×70，逐张目视确认）：
**文本 / 显示 / 全部 / 角色语音**。行素材按段分三类，尺寸就是分类依据：
`opt_sys_l01..24`（340×50，出厂在 08/12/20/21/23 处缺号，实有 22 张）、
`opt_sou_l01..03/15/16`（340×50，5 张）、`opt_cha_l01..18`（**380×140**，比其他段高一档，
因为**立绘头像和中文名字已经烤进图里** ⇒ 这一段不需要端口的字体）。

**每行的取值控件是 3 态而不是 5 档滑杆**：`b10_a..g/n_on` 全是 320×80，唯独
`b10_h/i/j_on` 是 150×80 ⇒ 它们不和其他菜单按钮同族；三张图上的字分别是
**无 / 中 / 大**（本轮目视确认，§2.7 的推测成立）。

**偏好键名从 dex 字符串表里挖到了**（`tools/tmp_dexgrep.py 'VOLUME|VOICE|_CHA|CHA_'`）：
`VOICE_ALL` + `VOICE_CH1..VOICE_CH18`（正好 18 行）。⚠ 别把 `VOICE_NUM` 算进来：
dex 里另有一族 `VOICE_KEY_ / VOICE_VALUE_ / VOICE_NUM`，与 `LABEL_*`、`GLOBAL_*`、`TIPS_*`、
`CHAPTER_NUM` 并列 ⇒ 那是**收藏**的"键名数组 + 计数"，与音量无关，§2.7 判它是解锁计数器仍然对。
出厂 `RL.xml` 里没有 `VOICE_ALL/VOICE_CH*` ⇒ 玩家没改过就不写盘，与"出厂未自定义"一致。

**说话人编号与语音文件是一一对应的**：`#MesSta(voiceId, speakerId)` 的 voiceId 形如
`NNNNN_KK`，其后缀 `KK` 就是 speakerId —— 全语料 7581 条带说话人的语音行里 **7579 条相符**
（`tools/tmp_voiceid.py`，只有 4 条串号 + 1 条 `_a` 例外）。⇒ 后缀与 speakerId 是同一套 1..19 编号。

**speakerId → 人名**（`tools/tmp_speaker.py`：取该行之前最后一次 `#MesName`）干净无歧义：
1 房石阳明、2 咩子、3 ？？(70)/美佐峰美辻(24)、4 芹泽千枝实、5 回末李花子、6 卷岛春、
7 织部泰长、8 酿田近望、9 织部义次、10 织部香织、11 能里清之介、12 室匠、13 卷岛宽造、
14 山胁多惠、15 狼爷爷、16 马宫久子、17 桥本雄大、18/19 恒为 ？？。
**而设置页的行序是素材自己的顺序，不是编号顺序**：
1 房石阳明、2 马宫久子、3 桥本雄大、4 织部香织、5 织部义次、6 卷岛春、7 织部泰长、8 酿田近望、
9 芹泽千枝实、10 能里清之介、11 卷岛宽造、12 狼爷爷、13 咩子、14 回末李花子、15 室匠、
16 山胁多惠、**17 便利店员**、18 其他。

**第 17 行"便利店员"和 sid 3 是同一个人**，这一步差点推错，三条证据钉死：
① `scenario02_victim.txt:455` 上屏的是 `#ImgBustAdv(4,43,…)`，43 ⇒ 素材目录 **03**（遮阳帽+脏辫那张脸）；
② 紧接着 `:464 #MesName(？？)` + `:465 #MesSta(00005_03,3)` 说「……欢迎」⇒ 店员的语音就是 `_03`/sid3；
③ 他自己的番外 `extra05_mitsuji.txt:39` 用 `10052_03,3` 说话 ⇒ 美佐峰美辻 = 同一个 sid3。
即 **素材目录 03 = sid 3 = 美佐峰美辻 = 设置页上按职业标注的"便利店员"**，
他前期在剧本里只挂 `？？`，所以 70 次无名 / 24 次有名。
由此 `g_sid_row[1..19] = 1,13,17,9,14,6,7,8,5,4,10,15,11,16,12,2,3,18,18`：
18 行**每行恰好被用到一次**，sid 18/19 两个未署名声音共用"其他"。

**端口侧的三处判断（都写进代码注释，便于日后改）**：
- `中` 是出厂默认 ⇒ 增益必须等于 1.0，`大` 只能是增益（取 1.4，混音端已有 `clamp16` 兜底）；
  原版的实际曲线**无法从素材推出**，这是判断不是还原。
- `语音音量`（`VOICE_ALL`）复用现有 5 档 `g_opt_gain`，与音乐/音效同表 ⇒ 设置页从 6 行变 7 行。
- 角色语音页做成"7 条滑杆下面的一行入口 → 独立一页"，**竖向 5 行窗口 + 滚动**；
  **没有**复刻原版的四段式标签页，那是 #21 的活。
- 实测边界：本轮只验证到"增益算得对、界面画得出、配置存得下"，**听感必须实机复核**。

**布局第一版做错了，改成了列表**：先按 3 列 × 6 行把 18 张图版铺满（0.66 档，按钮 75×40
叠在图版右下角）。截图证明这条路不成立——`opt_cha_l*` 是**列表行**素材，380×140 里
头像占左 45 %、**整名（含末字）一直排到右边缘**，没有任何预留空位，所以叠上去的
无/中/大按钮会**盖住名字的最后一个字**（`房石 阳明` 只剩 `房石 阳`）。而且网格让上下键
在行间斜着跳，而左右已被"改档"占用。⇒ 改为原版语义：**单列 5 行窗口滚动**，
图版 320×118（0.843 档）+ 按钮 126×67 并排在**右侧**，光标框 `opt_w_00_on` 套整行，
右上角 `n / 18` 计数；上下换行、左右改档、退出时 `opt_save()`。

**验证（`tools/tmp_voice.sh` + `tmp_voice2.sh` + `tmp_promote24.sh`，全部先 `cp -p` 备份
saves、测完还原并 md5 比对，三次都 `RESTORE-OK`）**：
- 资源：`MISS=0`（18 张 `opt_cha_l*` + 3 张 `b10_h/i/j_on` 全部解析）。
- 配置回环：写一份可见 5 行 = 无/中/大/无/中 的 `vo` 行，截图后按 `draw_menu()` 的布局常量
  裁出 5 个按钮区做 sha1 聚类（`tools/tmp_btnscan.py`）⇒ 行 2/5（同为 `中`）**哈希完全相同**，
  行 3（`大`）自成一簇，行 1/4（同为 `无`）哈希不同——差异只在光标框：行 1 裁区角像素
  `(96,96,96)`（`opt_w_00_on` 的描边）而其余行是 `(0,0,0)`。⇒ **三档三种图、与 cfg 逐行对应**，
  `vo i v` 确实驱动了画出来的按钮。
- 运行时查表：`RL_VERBOSE` 周期行新增 `voice=rN`（上一条语音用的行号）。跑 `scenario02_victim`
  1200 帧，采到 `voice=r1`（pc 271，来自 `:126 #MesSta(00004_01,1)` ⇒ sid 1 → 行 1）与
  `voice=r17`（pc 928/1380/1590，`:465` 之后 ⇒ sid 3 → 行 17 "便利店员"）。
  **`g_sid_row` 的两端（1 和 17）都在真机上跑通了**，包括那条最容易推错的身份合并。
- 提升与兼容：`ragingloop` 现为 sha256 `08e87505…`（992192 B），回滚档 `ragingloop.prev`
  是 #17（976184 B）。用玩家**已有的**存档跑 `RL_TITLE=0`：`save: resumed
  scenario02_victim.txt:5124 depth=2`、`log: 177 entries resumed`、
  `opt: cps=20 auto=60 font=38 shade=c8 bgm=1 se=1 vo=2`、`decode failed=0`。
  老 cfg 没有 `vo` 行 ⇒ 18 行全部落到默认 `中`（增益 1.0，等于不改原版听感），
  写盘后才补齐成 `7 opt + 18 vo`。**听感仍需实机复核**（无头只能证明增益接进了混音链）。

**本轮顺带修好的取证工具**：`tools/dex_scan.py` 以前在 import 时就跑 `main()`，
导致 `tools/dex_strings.py` 只要 `from dex_scan import parse` 就会先拿 argv[1] 当文件名崩掉
（一直没被发现是因为它从没被 import 过）。加了 `__main__` 守卫，并新增
`tmp_dexgrep.py`（按正则扫三个 dex 的字符串池）、`tmp_speaker.py`、`tmp_voiceid.py`。
`tools/run_remote.py` 以文本方式读脚本，Windows 默认 GBK 解码**任何带中文注释的脚本都会抛
`UnicodeDecodeError`** ⇒ 改成显式 `encoding="utf-8"` 并编码成 bytes 再发给 paramiko。

### 2.12 多槽存读档（#25）：槽模型、三处只在截图里暴露的缺陷、实测结果

取证部分见 §2.8（`autosave01..04.png` 60×60 ⇒ **4 个自动档**；`b02a/b/c` = 读取/记录/覆盖）。
**手动槽数量 10 是判断，不是取证**：存档 UI 在 Java 侧且被混淆成单字母类，出厂 `RL.xml` 里没有任何
槽位键（原版用 `File.listFiles()` 列目录），所以 APK 里查不到"原版给几个槽"。取 10 = 一页放得下、
编号 01..10 与 `chrt_sp_no` 同风格。改这个数字只需要动 `SAVE_MAN_N`。

**槽模型**（`main.c` 的 `slot_leaf/slot_label/slot_full/auto_pick`）：
- 磁盘上 `saves/` 里并存 15 个文件：`ragingloop.save`（**实时档，游戏启动就续它**，名字沿用历史）
  + `auto_1..4.save` + `slot_01..10.save`。列表 14 行 = 4 自动 + 10 手动，实时档**不占行**。
- 每次 `#Save()` 检查点：先原子重写实时档，再把字节副本**轮换**进"第一个空的自动档，否则 mtime 最旧的那个"
  （`auto_pick`，因此**不需要持久化轮换下标**，exFAT 上也不多写一次）。
- 存档头升到 `RLSAVE 2`（v1 只读兼容仍在），并新增 **`prev` 字段**：预览文本**不能**复用 `text`，
  因为 `text` 是"续档时要回填进文本框的话"，把兜底预览写进 `text` 会让读档后框里冒出一句旧台词。
  未知键在读取时天然被忽略（`save_load_leaf` 是 else-if 链），所以加字段不破坏兼容。

**菜单接入**：快捷列表从 9 行变 10 行（新增 `读取进度`，`MENU_N` 由枚举自动跟随），
`保存进度`/`读取进度` 进的是**同一页**、只差 `g_slot_mode`；游标上下循环 14 行，A 键按模式分派，
所有拒绝（"自动档由游戏写入"、"槽是空的"、"写入失败"）都只在本页底部给一行提示，不弹框。

**三处缺陷只有截图能发现，日志全绿**（`work/slot3_0.png` 是第一版）：
1. **预览行会空**：存档落在无对白的节拍上时 `g_v.text` 为空 ⇒ 行里只剩日期/脚本名。
   修法 = `prev` 兜底取回想日志最后一条（`log_push` 本来就跳过空文本，所以它必然是"上一句台词"）。
2. **对白框从菜单遮罩下透出来**：`BOX_Y=496`、`BOX_H=190`，而槽位第 6 行在 `y=522..602`，
   遮罩只有 `0xB4`（71 % 黑）⇒ 框里三行大字直接压在最后两行上，读不了。
   修法 = **菜单页打开时不画对白框**（`render_frame` 的框条件加 `!g_menu`）。
   这是**端口判断**：原版存读档 UI 是 Java 视图，我们看不到它是否保留底下的框。
3. **非游标行没有底板** + **预览里的 `\n` 会溢到下一行**：`opt_w_00_off` 只画在游标行时，
   其余行的文字直接贴在场景上失去对比；`esc_read` 会把存档里的 `\\n` 还原成真换行，
   一句两行的台词就把第二行画到了下一行的位置。修法 = 每行都画 off 底板（与快捷列表一致）、
   `preview_cut` 去 `%xx` 时一并丢掉 `\n`。

**验证（`tools/tmp_slots2.sh` / `tmp_slots3.sh` / `tmp_promote25.sh`，每次都先 `cp -p` 备份 saves、
按"测试前目录清单"精确删除本轮新建的槽文件、还原后 md5 比对，全部 `RESTORE-OK`）**：
- 按帧写槽（新增 `RL_SAVE_AT=<帧>`，因为循环前写的档 `pc` 恒为 0，只能证明文件格式）：
  第 250 帧写 `slot_01` ⇒ 日志 `save (slot-test) at scenario02_victim.txt:237`，
  **文件里**也是 `frame scenario02_victim.txt 237`；换一份完全不同的开场脚本
  （`RL_SCRIPT=scenario01_start RL_NEW=1`）再 `RL_LOAD_SLOT=4` ⇒
  `save: resumed scenario02_victim.txt:237 depth=1`。**跨剧本读回同一个 pc**。
- 自动档轮换：1201 帧长跑里 309→`auto_2`、799→`auto_3`、1094→`auto_4`、1467→`auto_1`，
  四档内容互不相同（`prev` 分别是"我下意识地忍住…/迎客的铃声很陌生/我立刻就怂了/「找到了」"）。
- 三个手动槽 pc 139/502/1121，其中后两个 `text` 为空而 `prev` 有词 ⇒ 兜底路径被真实覆盖到。
- 两种模式的整页截图 `MISS=0`，`work/slot4_0.png`（保存）与 `slot4_1.png`（读取）复核：
  每行有底板、自动档带 `autosave0N` 徽标、手动槽在保存模式显示 `覆盖`（已用）或 `记录`（空槽）、
  读取模式逐行 `读取`、对白框不再串进来。
- 提升：`ragingloop` = sha256 `48fc4eaf…`（1017408 B），回滚档 `ragingloop.prev` 是本轮第一个 #25 构建。
  用玩家**已有**存档连跑两次：`save: resumed scenario02_victim.txt:12040 depth=2`、
  `log: 250 entries resumed`、`auto: rotated into auto_1.save`、`decode failed=0`、30.0 fps，
  测完 saves 三件字节还原（`73cec761… / 1f820d30… / 268764ef…`）。
- **仍未验证**：无头 `dummy` 驱动发不出 A/方向键，所以页面上的**交互**（`menu_nav` 的 `g_menu == 4`
  分支：换行、确认写入、确认读取、B 返回）只做到代码审查 + 旁路旋钮等价验证，需实机按一遍。

### 2.13 章节图表（#26）：CSV 语义、连线贴图、以及一次"cfg 没有 seen 行"的假故障

**数据就在 APK 里**：`assets/script/C00_01.csv … C07_01.csv` 八张表，共 **254 个节点**
（每章 1 / 2 / 25 / 38 / 32 / 67 / 74 / 15）。单元格 7 个 `|` 字段：

| 字段 | 含义 | 实测取值域 |
|---|---|---|
| `date` | 剧中日期 | `5月11日(日)` 之类；**全表只有 C00 第一格是字面量 `none`** |
| `script` | 场景脚本 | 8 个 `scenarioNN_*` 之一 |
| `title` | 节点标题 | 与脚本里的 `#L(标题)` **逐字相同**，254 个**全局唯一** ⇒ 可作为点亮键 |
| `flagset` | 结局/旗标 | `-1`×222，其余 31 种 `L04`/`K03`/串联 `L03L08L19L20` |
| `type` | 节点种类 | `normal` 196 / `select` 29 / `end` 29 |
| `out` | **声明的出度** | `1`×199 / `0`×30 / `2`×21 / `3`×4 |
| `b` | 未定性，未使用 | `0`×85 / `1`×169 |

一行里的逗号 = **并行泳道**（同排多块板）；纯数字行不是节点。

**那条"未解码的数字行"现已解码**：它是**每条出边一个连线样式码**，长度等于该行各节点 `out` 之和
（201 行中 198 行吻合，3 行多出一个码，正是那 3 处"声明了跳行的边"）。码值 `1`=直连、`5`=首枝、
`3`=末枝、`2`/`4`=中间枝，对应 `chrt_line01..08` 这 8 张 60×60 折线贴图（实测 `line01` 墨迹
`x25..35` 满高＝竖、`line02` 墨迹 `y25..35` 满宽＝横，`line03..08` 是六种角/叉）。
**它只指名贴图、不指名目标**，所以拓扑仍由网格给出。`prev|`/`next|` 的 4 字段行是章节边界，
`next|` 用 title 点名下一章的入口节点。

**链接规则换过一次**：第一版按"我这一格里、下一行落在我的列区间内的单元格"取子节点，
与 CSV 自己声明的 `out` 在 **10/254** 个节点上不一致；改成"下一行的单元格从左到右依次发放，
每个父节点按 `out` 领取"后降到 **6**，且**没有任何节点失去父节点**。6 个已逐个解释：3 个是章末
（子节点在下一张 CSV，由 `next|` 行命名）、3 个是声明了跳行的边。这个数字进了启动日志，成为常驻自检：
`chart: 254 nodes over 8 chapters, degree short 6`。

**点亮与持久化**：`#L(标题)` 执行处调 `chart_mark(当前脚本名, 标题)` → `g_seen` 位图；
`ragingloop.cfg` 新增 `seen <字序号> <8位十六进制>`，只写非零字（新配置文件和原来一样小）。
`opt_load` 对 `seen` 用 **OR 合并**而不是覆盖，因为 `game_begin()` 里"从回想日志回填"先跑：
读档时把日志里出现过的节点先点亮（`chart: 4 nodes backfilled from the backlog`），
再并进 cfg 的历史位。回填只能覆盖最近 256 条（`LOG_MAX=256`、满则一次丢 128），更早的靠 cfg。

**页面**：快捷菜单第 8 项"章节图表" → `g_menu == 5`；左右换章、上下换节点、A/B 返回；
游标用 `opt_w_00_on`，"我在这里"用 `chrt_nowpoint01`（按当前 `#L` 标题 + 当前脚本名匹配，
每帧重算，因为图表打开时脚本仍可能走过一个 `#L`）。

**三处缺陷只有截图能发现**（`work/snap2/chart2_*.png` 第一版、`work/snap3/chart5_*.png` 第二版）：
1. `chrt_l00_off/on` 是 370×100，**左下角烤进了"▶ 详细分支"页签**（墨迹到 x≈130）。按 4:1 拉成
   370×76 之后日期正好压在页签上 ⇒ 底板保持源比例（280×76），日期右对齐到空出的右下。
2. `chrt_line.png` 是 440×30 的**横**条，压进 30 px 宽的竖段渲染成"=="。第二版先换成实心条，
   第三版再换成 `chrt_line01/02` 的 **1:1 平铺**（`chart_tile()`：60×60 无缝、按长度裁 src、
   已看过的边用 color-mod 染成 `chrt_greenline` 那个蓝 `(32,128,224)`，未看的用原色压到 alpha 80）。
   顺带：`chrt_greenline` 名字是绿的、画出来是蓝的。
3. C00 首节点的日期列字面值就是 `none`，被原样画了出来 ⇒ 与空串同等处理，不再显示。

**验证（`tools/tmp_chart3.sh` → `tmp_chart4.sh` → `work/chart5_run.sh`，每轮都先 `cp -p` 备份 saves、
按测试前目录清单精确删除本轮新建文件、还原后 md5 比对，三轮全部 `RESTORE-OK`）**：
- 解析：`254 nodes over 8 chapters`、`degree short 6`（与本地模型逐位一致）。
- 点亮：续玩家真实进度 → 回填 4 → 长跑再点亮 2 ⇒ cfg 出现 `seen 0 00ae8000`（**恰好 6 个位**）。
- 往返：重启后 `seen-lines-after-restart=1`、`chart: 6 nodes backfilled`，位没丢。
- 画面：8 章 + 11 行快捷菜单快照全部 `MISS=0`；`chart5_0` 证明 `none` 已消失，
  `chart5_2` 证明泳道分叉的连线粗细与原版一致，`chart5_5`（67 节点）证明滚动/裁剪不越界。
- **一次假故障值得记下**：第二轮报告"cfg 里没有 seen 行"，其实是**在还原步骤之后**才去读的 cfg
  ——读到的正是玩家那份干净配置。第三轮把读取挪到还原之前，一次通过。教训：**取证读盘的时刻
  必须在还原之前**，否则测的是备份而不是被测系统。
- 附带修掉一个真缺陷：`RL_NOSAVE` 原本把**读档**也一起关掉（`save_load_leaf` / `log_read_disk`
  都在 `!g_save_ok` 时直接 return），于是"只读冒烟"实际开的是新局，还会留下 `auto_*.save`。
  现在 `RL_NOSAVE` 只关写（`g_write_ok`），读档路径不再受它影响。
- **未做**：页签"▶ 详细分支"（原版逐节点展开子分支）、从图表 A 键直接进入该场景（我们的 A 只关页）、
  `chrt_savepoint*`/`chrt_sp_no*`（存档点书签徽标）、`chrt_element*`（图例）。
- 提升：`ragingloop` = md5 `d6a05ea7…`（1054784 B），回滚档 `ragingloop.prev` = `5cdfa379…`（#25 构建）。
  提升后冒烟：`save: resumed scenario02_victim.txt:12040 depth=2`、`degree short 6`、`MISS=0`、
  **30.0 fps**、`PROMOTE-CLEAN`（saves 三件字节未变）。
- **仍未验证**：无头 `dummy` 驱动发不出方向键/A，页面上的**交互**（`g_menu == 5` 分支：换章、
  换节点、滚动跟随、B 返回）只做到代码审查 + `RL_CHAPTER`/`RL_CHART_NODE` 旁路等价验证，需实机按一遍。

### 2.14 按键配置（#27）：把「键盘 keycode」换成「手柄键位」，以及一次「上传了旧构建」的自测故障

**原版这一页的美术补齐**（`apk/assets/`，之前只记录了 dex 常量）：

| 贴图 | 尺寸 | 实测内容 |
|---|---|---|
| `key_w_00_off` / `_on` | 420×100 | 该页**专用**行底板：圆角深灰渐变 + 细亮边；`_on` 多一圈蓝色辉光 ⇒ 焦点由底板承载 |
| `b_key_off` / `_on` / `_gray` | 90×90 | 行尾**键帽**，上面烤着钥匙图标和字面 "KEY"；`_on` 饱和蓝、`_off` 蓝灰、`_gray` 纯灰 |
| `keycursor_1..4` | 30×30 | 四枚**左上角括号**，亮度依次递减 ⇒ 游标脉冲帧，不是四个方向 |
| `cfg_e01a` / `_b` / `_c` | 40×40 / 40×40 / 240×40 | 单选点（空心 / 实心青点）与一枚深色胶囊 |

⇒ 原版按键配置页 = 一列 `key_w_00` 行底板，每行右端一枚 `b_key` 键帽，焦点行加辉光 + 键帽四角括号。

**存储模型与原版不同，这是刻意的**。原版是**稀疏对表**：`KEYCONFIG_NUM` 给出条数，
每条 `KEYCONFIG_MENU_<n>`（13 个 `KEY_MENU_*` 动作名之一）+ `KEYCONFIG_KEYCODE_<n>`
（Android `KeyEvent` 码），出厂 `NUM=0` = 一个都没绑。端口用 `key <动作行> <键值>` **直接赋值**、
只写非零行：与出厂"全未绑定"等价，且**解绑能存活**（不写即无，不需要 `key 3 0` 这种占位行）。
读回用赋值而不是 OR，`opt_load()` 全程只在启动时跑一次，不会把用户刚清掉的绑定又合回去。

**载体替换的理由**：掌机没有键盘，Android keycode 这一层没有可还原的对象。端口保留**能力**
（给菜单动作绑一个键），换掉**载体**（绑到手柄键位）。可绑 6 个：A/B/X/Y/R1/START。
`L1`/`SELECT` 不提供 —— 它们是内置的"开菜单 / 退出"逃生键，被绑走就可能把玩家困在叠加层里；
`继续游戏` 不提供 —— 正文界面本来就没有菜单可关。

**四条语义**（都写进了代码注释）：

1. 绑定只在**正文界面**生效：菜单、回想日志、标题页、`#Select` 选项期间一律不触发（f 步实测打印
   `RL_FIRE: skipped, story view not active`）。
2. 命中的键**吞掉**本次按下 —— 把动作绑到 A 上，A 就只触发该动作，不会再顺手推进对白。
3. 同一个键绑多个动作时，按快捷菜单行序取第一个（b 步意外验证：`回想日志` 与 `返回标题` 同绑
   START，先命中 `回想日志`）。
4. 越界键值被范围检查丢弃（b 步 `10=7` 里的 7 超出 `KEY_V_N` ⇒ 页面显示"无"，不画不触发）。

**设置页要塞进两行入口，几何先量后改**：`opt_bar_0*` 是 460×80，但**墨迹只占 y22..63**，
所以行距从 80 压到 74、起始 y 从 40 抬到 22 之后，6 px 的重叠全在透明边里；
两行入口（`角色语音` / `按键配置`）落位后最后一行底板底边 672，与提示行 682 不相撞。
按键配置页 `g_menu == 6` 用 10 行 `key_w_00` 底板（660×60），行尾键帽按"已绑=`b_key_on`、
无=`b_key_gray`"取图，**键名画在键帽左侧**而不是压在键帽上 —— "KEY" 是烤进图里的，压字会打架。

**无头怎么测一条"按下手柄键"的路径**：新增两个测试旋钮，与既有 `RL_MENU`/`RL_SAVE_AT` 同族 ——
`RL_BIND="动作行=键值,…"` 预置绑定，`RL_FIRE="键值@帧"` 在运行中按下某个可绑键。关键是
`RL_FIRE` 调的是**和手柄边沿同一个** `key_fire_slot()`，并带同一套前置门，所以它测的是真路径而不是测试替身。

**一次自测故障值得记下（流程缺陷，不是代码缺陷）**：第一轮 c/d/e/f 四步**一条 `RL_FIRE` 日志都没有**。
根因是**上传的产物属于上一轮构建**：`build_key1` 的完成通知先到，我在 `build_key2` 仍在跑的时候
按当时 `port/build-aarch64/ragingloop` 的 md5 把它传了上去，而那个二进制里还没有 `RL_BIND`/`RL_FIRE`。
判据应当是：**产物 mtime 必须晚于 `main.c`，并且要等对应那一次构建的退出通知**；
第二轮上传 `75b55ddf…`（1058168 B，mtime 09:35 > main.c 09:31）后一次通过。

**验证（`work/key1_run.sh`，7 步全过，快照 `work/key1_snap/key_{opt,page,load,chart}.png`）**：
整轮跑在**临时根** `/tmp/rlkey`（`assets` 用软链指回 `$GAMEDIR`），玩家自己的 `saves/` 全程没被打开过，
收尾 `REAL-SAVES-CLEAN`（前后 md5 逐字节一致）+ 无新文件 —— 这一条比"备份/还原"更稳，因为它不需要还原。
- c 步：`RL_FIRE: pressing X at frame 40` → `key: X -> 自动推进`，cfg 落出 `key 2 3` 与 `flags 1 0 0`。
- d 步：**不带 `RL_BIND`** 重启，同一份 cfg 读回，页面上 `自动推进` 仍显示 X ⇒ 写/读两端都闭环。
- e 步：正文界面按 START 直接打开章节图表（`key_chart.png` 里 8 个章签、`1 / 1 已见` 齐全）
  ⇒ `menu_act()` 在菜单关闭状态下可安全开页。
- 四张快照 `MISS=0`；`key_opt.png` 证明 9 行设置页 + 提示行排版无重叠。
- **未做**：原版键帽上"实际绑了哪个键"的字符（那是 Android keycode 的字面，掌机无对应物）；
  `keycursor_1..4` 的四角括号脉冲动画未用（端口用底板辉光承载焦点）。
- 提升：`ragingloop` = md5 `75b55ddf…`（1058168 B），回滚档 `ragingloop.prev` = `d6a05ea7…`（#26 构建）。
  提升后冒烟：`save: resumed scenario02_victim.txt:12040 depth=2`、`degree short 6`、`MISS=0`、
  `PROMOTE-CLEAN`，并且玩家 cfg 里 `^key` 行数为 **0** ⇒ 没绑定过的玩家，行为与上一版逐字一致。
- **仍未验证**：真手柄上"按下 X 就触发"这一跳。`dummy` 驱动下 SDL 仍然开了手柄
  （日志 `pad='retrogame_joypad'`），但无头进程收不到按下，所以边沿检测那 6 行只做到代码审查；
  需实机进"选项设置 → 按键配置"绑一个键再按一遍。

## 3. 目标环境（阶段 C，实测 `logs/target-probe.txt`）
| 项 | 实测 | 对本移植的意义 |
|---|---|---|
| 系统/内核 | ROCKNIX `R36Max2`，Linux 6.12.79，aarch64 | 与 brief 假设的 Mali-450/armhf **不符** |
| SoC/GPU | Mali **Bifrost G31**；`/usr/lib/libmali-bifrost-g31-g24p0-wayland-gbm.so → libmali.so.1`；glvnd 有 `50_mesa.json`；`libEGL/libGLESv1_CM/libGLESv2/libGL` 齐 | GLES1 可用，但**不必**照搬 GLES1 固定管线 |
| 屏幕 | `/sys/class/drm/card0-DSI-1/modes = 1024x768` | 4:3 面板 + 16:9 游戏 ⇒ 内容区 **1024×576 居中**，上下各 96 px 黑边 |
| 内存 | MemTotal 1005276 kB，MemAvailable 596692 kB | 纹理缓存必须有上限（见 §5） |
| 存储 | `/storage` mmcblk1p2 7.3G ext4；`/storage/roms` mmcblk1p3 **40.7G，24.6G 可用** | 放得下 ~700 MB 资源 |
| 布局 | `roms/ports/` 内 `.sh` 与端口目录混放；**无 `ports_scripts`** | 合并式（与 ee47 相反） |
| SDL2 | 2.32.10（`libSDL2-2.0.so.0.3200.10`）+ `SDL2_image/ttf/gfx/mixer` | 可依赖系统 SDL2 |
| 字体 | `/usr/share/fonts` 只有 DejaVu/Liberation（**无 CJK**）；**PortMaster 自带** `resources/NotoSansSC-Regular.ttf`（另有 TC/JP/KR/HK） | 中文渲染靠 PortMaster 的 NotoSansSC，或随端口带一份（OFL） |
| 解码库 | 设备 `/usr/lib` 有 `libogg.so.0` / `libvorbis.so.0`（`libvorbisfile.so.3` 亦在），**无** `libpng` | ⇒ 实测：二进制动态链 `libvorbisfile.so.3`，实机 `ldd` 全解析；早期"必须静态编进二进制"的判断已被实机推翻 |
| 音频 | PipeWire + pipewire-pulse + `libasound` | SDL2 的 alsa/pulse 驱动均可用 |
| 前端 | `essway.service`（EmulationStation-with-Sway）运行中 | 启动脚本须按 PortMaster 惯例让出/收回显示 |
| 显示后端 | **sway / Wayland**：前端为端口导出 `SDL_VIDEODRIVER=wayland`、`WAYLAND_DISPLAY=wayland-1`、`MALI_WAYLAND_AFBC=0`；实机日志 `sdl 2.32.10 driver=wayland` | ⇒ 启动脚本**不得**再强制 `kmsdrm`（早期版本这么写过，与实机矛盾，已删）。`pm_platform_helper()`（在 `mod_ROCKNIX.txt` 里）只做退出对话框与 `portmaster_sway_fullscreen.sh`，**不拉起游戏**，真正的启动行是我们自己那一条 |
| 开发工具 | 设备上有 `python3/gdb/curl/wget/tar/unzip`；**无 gcc/make/cmake/git/ffmpeg** | 一切编译在容器内 |
| glibc | 2.40 | 我们的门禁（GLIBC ≤ 2.29）远低于设备，兼容 |
| `roms` 文件系统 | `/dev/mmcblk1p3` **exfat**，`fmask=0022,dmask=0022,iocharset=utf8`（`logs/fs-facts.txt`） | ①**大小写不敏感** ⇒ 资源名不得依赖大小写区分；②**不支持符号链接** ⇒ 字体/库只能真复制，不能 `ln -s`；③簇 384 KB ⇒ 8902 个文件实占 1.4 GB（逻辑 675 MB），空间仍充足；④`fmask=0022` 使普通文件天然带 x 位，`.sh` 可直接执行（已实测 `./t.sh` → `exec-ok`） |

编译容器（`portmaster-builder:aarch64-latest`，`logs/`… 见 `tools/probe_container.sh` 输出）
具备：SDL2 头（2.0.10）、`libogg.a libvorbis.a libvorbisfile.a libpng.a libjpeg.a libfreetype.a`、
`zlib.h`、`aarch64-linux-gnu-gcc 9.4.0`。

## 4. 路线决定（以及为什么）

**做原生 C 语言 ADV 解释器（路线 3），不复用 Unity loader。**

理由（按证据强度排序）：

1. 没有任何 native 引擎可复用：APK 唯一的 `.so` 是 SDK 桥（§1）。
2. 脚本是明文、指令集封闭（55 条、参数定长）、且 id→文件的映射已被字节码常量表
   **逐条闭合**（§2.2）。这是自研解释器的充分前提；若脚本是编译型字节码就不会选这条路。
3. 渲染是 2D 图层合成 + 文字 + 音频，**没有 3D、没有物理、没有 shader 依赖**
   （Java 侧只引用 `GL10`），因此不需要复刻引擎内部结构，只需复刻**可观测行为**。
4. 1 GB 内存下，JVM 路线要么装 Android framework 替身（工程量不可控），要么放弃；
   原生 SDL 进程 RSS 可控在 ~150 MB。
5. 复用面：NextOS 里对本游戏有用的是 `framework/nxcompat`（`nxcompat_video.h`
   的内容区/宽高比决策、`nxcompat_settings.h`、`nxcompat_system_font.h`、
   `nxcompat_language.h`）与 `framework/nxinput`（GPTK 手柄映射），
   以及 ff4/castleofillusion 那种「vendored stb 单头文件」的既有做法。
   **不引入 60parsecs 的 ELF loader/JNI 假层**，因为本游戏没有客端 `.so` 要加载。

技术选择（已定，理由随附）：

- **SDL2 + SDL_Renderer**，不用 GLES1 固定管线：本游戏是 ≤6 层 2D 合成，
  SDL_Renderer 自动处理 EGL/viewport/缩放，且设备 SDL2 完整； GLES1 复刻只增加风险不增加能力。
- **OGG 用容器里的静态 libvorbisfile**，SE/语音/BGM 全部流式解码 + 自建混音环
  （SDL_Audio 单设备，参照 `ports/ff4-nextos` 的教训：一个 device、一把 mutex）。
  选静态而非 SDL2_mixer：设备 `libSDL2_mixer` 是否带 ogg 后端未验证，且不可控。
- **图像用 vendored `stb_image.h`**（本仓库 ff4/castleofillusion 已有同源文件）：
  同时吃 PNG 与 JPEG，零设备依赖。
- **中文用 vendored `stb_truetype.h` + 自绘字形图集**，字体优先
  `/storage/roms/ports/PortMaster/resources/NotoSansSC-Regular.ttf`，
  可用 `RL_FONT` 覆盖。选 stb_truetype 而非 SDL2_ttf：字形度量与 %co 颜色标记
  的行内排版要自己控制，SDL2_ttf 只提供整串渲染。
- **渲染分辨率固定 1280×720 离屏纹理**（= 素材原生尺寸），再按 `nxcompat_video`
  的内容区规则 letterbox 到 1024×768。避免逐元素重算坐标。
- **纹理缓存 LRU + 上限**（默认 ≤ 48 MB），因为单张 CG 就 3.6 MB RGBA。
- **过场影片（3 个 mp4）先做占位**：设备无 ffmpeg，mp4 解码不在第一轮关键路径上
  （脚本里 `#Movie` 只有 7 处）。占位期间按播放时长黑屏 + 音频，不阻塞流程。

## 5. 阶段状态（对齐用户给的 17 阶段；本游戏按 §4 重解释各阶段含义）

| # | 阶段 | 本游戏对应 | 状态 | 证据 |
|---|---|---|---|---|
| 1 | 确认 Unity 版本 | 引擎定性 | ✅ 非 Unity，Java 层自研 ADV 引擎 | §1 |
| 2 | 确认架构 | 输入 ABI | ✅ arm64-v8a（APK 含 5 个 ABI，仅 bridge） | §1 |
| 3 | 分析 libmain.so | 不存在 | ✅ 已证明不存在 | `ls lib/*/` |
| 4 | 分析 libunity.so | 不存在 | ✅ 同上 | 同上 |
| 5 | 分析其他 *.so | `libbridge.so` | ✅ SDK 桥，零 GL/音频依赖、零 `Java_*` | §1 |
| 6 | Java/JNI 依赖 | 引擎 API 面 | ✅ GLSurfaceView/GL10/MediaPlayer/VideoView/SoundPool/AssetManager/SharedPreferences/WebView | `logs/dex-scan.txt` |
| 7 | 选最近案例 | NextOS 案例 | ✅ 无同类 VN；借 `nxcompat`(video/settings/font) + `nxinput` + ff4 的 stb/audio 做法；Unity loader 不适用 | §4 |
| 8 | 建立最小 loader | 原生解释器骨架 | ✅ `port/src/{main,res,txt,audio}.c`，容器内 aarch64 编译通过 | `port/build-aarch64/ragingloop` |
| 9 | native 成功加载 | 二进制在设备起进程 | ✅ 设备 `ldd` 零缺失，进程常驻 | `work/device_log.txt` |
| 10 | 初始化 | SDL/资源/脚本装载 | ✅ `sdl 2.32.10 driver=wayland`、`audio 44100Hz 2ch 1024spp`、sounddef 51 条 | 同上 |
| 11 | 创建窗口 | 1024×768 letterbox 1024×576 | ✅ `renderer=opengles2 maxtexture=16383` | 同上 |
| 12 | 显示画面 | 背景+立绘+文字 | ✅ 实机有画面、立绘五官对齐（用户实机复核）；无头截图逐场景自检（§2.4）；文字已改为逐字显现（§2.5），**节奏待用户实机复核**；`ImgFaceAdv` p4 脸部叠加层与 `ImgItem` 独立附件已修，新旧二进制逐帧像素对照见 §2.10；**章节图表页（#26）已通**：254 节点/8 章、连线用原版 `chrt_line01/02` 贴图、已见位进 cfg 并可跨会话往返（§2.13）| `work/rlsv5/all17.png`、`work/rlgo/`、`work/rlh/`、`work/rlrev/`、`work/snap/n730_*.png`、`work/snap3/chart5_*.png` |
| 13 | 输入 | 手柄推进/选项 | ⏳ 推进/SELECT 退出已实测；选项 `#Select` 未实机验证；**按键配置（#27）已实现**：10 个菜单动作可绑 A/B/X/Y/R1/START，绑定进 cfg 并可跨会话往返，无头用 `RL_BIND`/`RL_FIRE` 走同一条 `key_fire_slot()` 路径验通（§2.14），**真手柄按下待实机** | 用户报告"按 B 回到上一条"、`work/key1_snap/`、§2.14 |
| 14 | 音频 | BGM/SE/语音 | ✅ 语音"快进"根因已定位并修复（§2.5 单声道 `ov_read`），设备日志 `audio open ch=1 rate=44100` 证实按真实声道解码；**听感待用户实机复核**。逐角色语音（#24）已接进混音链：`#MesSta` 的 speakerId 经 `g_sid_row` 查到设置页行号，取该行的 无/中/大 增益，与全局 `VOICE_ALL` 相乘（§2.11 实测 `voice=r1`/`voice=r17`）。`BgmIn/BgmOut` 淡入淡出与 SE 槽未核 | `work/rlh`、`work/voice_test2.txt`、§2.11 |
| 15 | 存档 | 检查点快照 + 续玩 + 多槽 | ✅ 设备无头实测：`save (autosave)`/`save (quit)` 写出 `$RL_ROOT/saves/ragingloop.save`，重启后 `save: resumed scenario01_start.txt:20 depth=1` 并继续推进；回想日志 `ragingloop.log` 落盘/读回字节级一致且续玩不重复记账（§2.9）；**多槽存读档（#25）已通**：4 自动档轮换 + 10 手动槽，按帧写槽→跨剧本读回同一 pc（§2.12）；**面板上的存读与回看体验、以及槽页的方向键/A 交互待用户复核** | 本轮 `/tmp/rlsv1.log`、`/tmp/rlsv2.log`（§2.6）、§2.9 表、`work/slots2_test.txt`、`work/slot4_{0,1}.png` |
| 16 | 性能 | 30/60 FPS、纹理上限 | ✅ 实机 **30.5 fps**（4665 帧 / 153 s）已达标；纹理 LRU **已在**（`img.c` 哨兵双向链表 + `evict_until`），三档上限守得住且默认 48 MB 档 2400 帧只淘汰 6 次（§2.10 表），故不上调 | `exit after 4665 frames in 153s (30.5 fps), cache=50112388B`、§2.10 |
| 17 | 可玩 | 进主线并能存读 | ⏳ 存/读通路已通（§2.6），主线推进到 `#Select` 之后的分支尚未长跑验证 | — |

**不跨阶段宣布成功。** 阶段 12 的判据是「设备屏上的真实像素」，不是「函数返回 0」。

## 6. 已知风险与未闭环

1. **中文文本渲染的字体授权**：PortMaster 自带 NotoSansSC 可用，但端口若随包带字体
   需附 OFL 声明；发布策略待定（先按运行时查找，不打包）。
2. **`#ImgItem` 的第二义**：参数域（slot, 1..8, 0|1）与「角色附件」吻合，
   但表内另有 `popup/item_001..047` 与 45 张 `albumTmb`。当前判断 `popup/` 属
   画廊/Extra 菜单（Java UI 侧），不由主线脚本寻址 —— **未验证**，实机对照可证。
3. ~~`ImgFaceAdv` 的 mouth 特值 40、chibi 整脸命名、越界索引~~ **已核**（§2.4 的索引覆盖审计）：
   chibi 用 `N_face.png` 且 `brow==eye==mouth` 恒等；越界只有 5 行，按"该层不画"处理。
   仍未验证的只剩 `acce_*_tearNN/tabacoNN` 后缀的选取规则与 `emo=-1` 的语义。
4. **`#CmpC/#CmpS/#If/#FlagC*` 的确切求值模型**（C 与 S 两类变量、比较缓存槽）：
   指令计数与配对关系已统计（`If`/`EndIf` 各 975，`Else` 27），但语义细节
   **未闭环**；第一版按「Cmp* 取号 → If 比较常量 → 命中则跳转/继续」实现，
   之后用实际分支走向验证。
5. **`#Movie` 的 mp4 解码**：设备无 ffmpeg。占位实现后需要单独决定（内置极简解码 vs
   静态捆绑 vs 保留黑屏）。
6. **标题/系统菜单是 Java UI**（根目录 `opt_*`/`cfg_*`/`bN_on`/`keycursor_*` 等
   素材）：1:1 复刻成本高，端口按"同一批素材 + 自己排版"逐项还原。已通：快捷菜单、设置页
   （7 项滑条）、逐角色语音页、存读档槽页、回想日志、章节图表、按键配置、标题页
   （§2.7、§2.9、§2.11–§2.14）。仍缺：`额外内容`/`收藏` 两块画廊（原版 `popup/item_001..047`
   + 45 张 `albumTmb`，端口把标题页这两项画灰）、图表页"▶ 详细分支"页签、
   按键配置页键帽上的实际键名（Android keycode 字面，掌机无对应物）。
7. 原始下载制（`RL_SYSTEM.xml` 的 `voice.zip/sound.zip`）说明原版把语音当 DLC 下载；
   本包已内置 ⇒ 我们**不需要**联网，但 `ACTIVATE`/`FIRST_CHECK` 之类标志要在存档层自己给默认值。
8. ~~exfat 大小写/目录名~~ **已核**：`roms` 为 exfat、大小写不敏感、无符号链接（§3 表）。
   资源树内部不存在仅靠大小写区分的同名文件（部署后 8902 文件全部可读，
   `logs/fix-location.txt`）。加载器仍按精确大小写路径打开，以便将来放 ext4 也能跑。
9. **Windows Git-Bash 会把 `/storage/...` 这类容器/设备绝对路径改写成
   `D:/Programs/Git/storage/...`**：首次部署因此把 675 MB 解到 `/storage/D:/Programs/Git/…`
   （已在设备上 `mv` 归位并清掉空壳）。以后所有传设备绝对路径的工具调用都要带
   `MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*'`，或在 Python 侧校验路径以 `/` 开头。

## 7. 工具与取证（本目录内）

```bash
# APK 侧（主机，纯 Python，无需额外安装）
python tools/dex_scan.py apk/classes.dex apk/classes2.dex apk/classes3.dex
python tools/dex_strings.py apk/classes.dex
python tools/dex_conststrings.py apk/classes.dex "background/bg_001" --dump
python tools/table_slices.py logs/table-f-clinit.txt logs/table-slices.txt
python tools/script_census.py apk/assets/script
python tools/map_check.py apk
python tools/assets_inventory.py apk/assets

# 设备侧（IP 一律运行时给出）
DEVICE_HOST=<ip> python tools/run_remote.py tools/probe_target.sh rocknix
DEVICE_HOST=<ip> python tools/put_file.py <local> <remote-dir> <name>

# 无头截图判据（量化，不靠肉眼）
python tools/reveal_check.py <dir with shot*.bmp>   # 逐帧统计文字框亮像素：逐字显现应递增后平台化

# 容器侧
MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' docker run --rm --platform linux/arm64 \
  -v <abs port dir>:/src -w /src \
  ghcr.io/monkeyx-net/portmaster-build-templates/portmaster-builder:aarch64-latest \
  bash /src/tools/probe_container.sh
```

Windows 侧两个必踩的坑（沿用了 60parsecs 的结论）：`python3` 是 Store 别名，一律用 `python`；
docker/objdump 需要 `MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*'`，本地路径给 `D:/...`。
`tools/run_remote.py` 现在**拒绝**在没有 `DEVICE_HOST` 时运行（旧默认 IP 已删除），
避免从历史文本里翻设备地址。

诊断 env（规划中，随阶段推进实现）：`RL_VERBOSE` `RL_SCRIPT=<file>` `RL_LINE=<n>`
`RL_FONT=<path>` `RL_TEXCAP=<MB>` `RL_NO_AUDIO` `RL_NO_MOVIE` `RL_FPS`。
