# 60 Parsecs! 移植 — 推进计划（PLAN）

> 状态文档，最后更新：2026-10-03（音频听感确认通过，进入输入/存档/性能逐项验收）
> 目标设备：EmuELEC 4.7（ee47 / NGP45H，aarch64 / Mali-G52 GLES 3.2 / 1024×768 / 2GB 内存（实测 /proc/meminfo，早前 1GB 说法过时）/ 分裂布局）
> 测试机：192.168.31.192（2026-10-03 起更换；旧机 192.168.31.218）（roms/ports 放游戏资源，roms/ports_scripts 放 .sh 启动脚本；同 ee47/1024×768，二进制 md5 需与本地核对）
> 移植路线：NextOS V5 自研 loader（不用 BENDER）

---

## 里程碑状态

- **"先确保可以进入游戏画面"（用户首要目标）：✅ 达成（2026-10-03 run31）**
  splash → 菜单（`rgb_non_black=100.0%`）→ 游戏场景（截图：飞船舱内/终端/地球仪/星空窗）。
- 残留非阻断告警：`SpriteAtlasManager.atlasRequested … April`（角色美术缺失）、
  `There is no RG.Parsecs.Common.GameManager instantiated on scene`（时序告警）。

---

## 0. 资源加载链（game.apk）

| 环节 | 状态 | 说明 |
|---|---|---|
| 生成 | ✅ | `tools/make_game_apk.py` 把 `apk/` 解包物的 `assets/bin/Data/*` 打成 STORED zip → `gamedata/60parsecs/game.apk`（105MB） |
| 加载 | ✅ | `./60parsecs-nextos "$GAMEDIR"` → `sf_apkdir=$GAMEDIR`；`jni.c:asset_pack_dir()` 优先返回 `$GAMEDIR/game.apk` |
| bank 落盘 | ✅ | `*.bank` 已部署到设备 `/roms/ports/60parsecs/assets/`，run31 全部 `loadBankFile -> 0`（§7.9） |
| 部署 | ✅ | 设备侧二进制为 BuildID `57b7eaf3…`（§7.9/§7.10/§7.11/§7.12 修复保留，§7.13 R3→Space 绑定**已回退**），sha256 `aa99b854…` 校验一致，前端 emustation 已恢复 active |

---

## 进展对照（17 阶段）

- **#1–#11 ✅**：Unity 6 / 架构确认、5 个 *.so 分析、Java/JNI 依赖、最小 loader、native 全装载、Unity 初始化、窗口建立（KMSDRM 1024×768 + Mali-G52 GLES 3.2）
- **#12 ✅**：splash → 菜单 → 游戏场景，run31 frame probe 100% 非黑 + 截图证实
- **#13 ⚠️ 输入**：GPTK 通路在跑，未逐项验证游戏内响应
- **#14 ✅ 音频**：Studio 初始化已通（§7.7+§7.9），5 bank 全部加载成功；AAudio 出声闭环（§7.6）；§7.8 杂音（run32/33 自动化复测）+ §7.10 设备互踩 + §7.12 静音挤占两处断续根因修复（run35/37 实证）；**维护者实机听感确认正常（2026-10-03）**
- **#15 ⏳ 存档 / #16 ⏳ 性能（~31fps） / #17 ⚠️ 可玩性逐项验证**（阻塞已解除，未开始逐项验收）

**已闭环根因（run16–31 实证）**：
7.1 Swappy 单例 NULL→SIGSEGV；7.2 `pc=0`（process_vm_readv）；7.3 `strlen(NULL)`；
7.6 AAudio 门面签名错误（(60) 根因）→ 出声；7.7 ERR_INTERNAL（libfmod 的 `JNI_OnLoad`
从未被调，main.c 显式补调 → `Initialize->0`）；**7.9 黑屏第二根因**：`jar:` 空 sourceDir
→ bank 全 404 → `bus:/Master` 缺失 → `BusNotFoundException` 杀死 `CompleteInitialization`
协程 → 菜单不加载；修复 = `audio.c:sf_fmod_preload_banks()` 在 Initialize 成功后经
`nx_lookup_in` 取 `FMOD_Studio_System_LoadBankFile` 原生同步预加载全部 `*.bank`。
7.8 杂音（float→s16 写入长度用错）已修并复测通过（run32/33 + 听感确认）；
7.10 断续（FMOD 实例互踩共享 SDL 设备）与 7.12 断续第二根因（静音块挤占队列）已修并听感确认；
7.11 触控点击落点：信箱映射（§7.11/7.11b）被实机推翻，§7.11c 定论 Unity 指针
    合同 = 全表面线性，已回退并实证（run42）。

---

## 推进清单（按优先级，可勾选跟踪）

### P0 — 进入游戏画面（✅ 全部完成，2026-10-03 run31）
- [x] §7.7 `ERR_INTERNAL`：libfmod `JNI_OnLoad` 未调 → main.c 显式调用，`Initialize->0`/`IsValid->1`
- [x] §7.9 黑屏主因：`jar:` 空 sourceDir → BankLoadException → BusNotFoundException → 协程 abort → `audio.c` native 预加载 bank 修复
- [x] 验收达成：5 bank `loadBankFile -> 0`、`BusNotFoundException` 消失、`rgb_non_black=100.0%`、截图游戏场景
- [x] 部署缺口补齐：`*.bank` 落盘 `/roms/ports/60parsecs/assets/`，重建二进制（`b417822d…`）原子替换

### P1 — 音频与可玩性验证
- [x] §7.8 杂音自动化复测（run32/33，2026-10-03）：AudioTrack 路径真实 s16 样本流动
  （peak 1737~4476，正常音频电平），全程无满幅垃圾值 → 杂音源已消除；
  AAudio 门面实例全程静音属预期（本作音频全走 Studio→AudioTrack）
- [x] **声音断续修复（§7.10+§7.12，run35/37 实证）**：①两个 FMOD 实例互关共享设备
  → 设备进程级复用（失败 5138→0）；②静音 AAudio 实例挤占共享队列 → 全静音块
  不入队（run37 连续 7 窗口全部有声）；**维护者实机听感确认正常（2026-10-03）**
- [x] **屏显按键点击位置（§7.11→§7.11b→§7.11c，三轮收敛）**：
  §7.11 曾按"16:9 信箱"改内容带映射 + §7.11b 统一绘制/注入——均被实机复测推翻；
  **§7.11c 定论**：Unity 指针合同 = Screen 像素 = 全表面 1024×768（EventSystem
  直读全表面坐标，Overlay UI 按全表面锚定——资源取证：语言按钮 anchor
  [0.946-0.988]×[0.892-0.980] 为屏幕比例）。已删除信箱变换，注入与绘制恢复
  **全表面线性**（PC 鼠标 1:1 行为）；run42 实证：tap:640,680→512,725 ✓、
  箭头可达 y=4（信箱映射下限 96px）✓；黑边可达性如需处理另用光标钳制
- [ ] **splash 冻结 / 点击无法进入游戏（§7.13，已回退，待重新取证）**：splash 序列
  只认 Android KeyCode，逐码实证仅 Space(62) 有效（Enter/Esc/Back/Tab 无效），
  自动播放需 ~4.4 分钟故体感卡死。曾修复 = R3 光标点击广播 KEYCODE_SPACE，
  但**维护者实机物理验收判定该按键绑定无效，2026-10-03 已回退**
  （input.c 恢复 R3 点击只注入触摸）。遗留疑点：SSH 合成注入能推进
  （run46/48/49）而物理 R3 不推进，物理输入→Unity 管线链路待另行取证。
  **UI 完整性核查：各帧渲染正常、无洋红，MenuScene/OptionsManager 早已
  加载，卡的是序列推进不是渲染**
- [x] **无人工干预复核（run48/49，排除 run47 混入手动点击）**：纯 SSH 注入会话
  180s/205s 基线探针同值（logo splash 冻结实证），仅注入 R3×3 后推进；
  触发式抓帧 A/B/C 证实 logo→避难所场景（主菜单背景），字节级 md5 全不同。
  **教训：覆盖率指纹撞车（logo 与避难所场景同为 37.2-37.3%），探针数值不能
  唯一标识画面状态，判定推进以截图为准**（详见 §7.13）
- [ ] 主菜单（100% 背景）内逐按钮点击验证（§7.11c 全表面映射下的实际手感，
  需维护者实机确认）
- [ ] 输入对照（#13）：已对照 BENDER 参考（60秒差距工程）——其 GPTK 为 kill-only
  （不注入，原生手柄直通），UNITY_KEY_MAP 仅 FN+Y 可选覆盖层；本移植默认同为
  原生直通（A=96/B=97/X=99/Y=100/D-pad 直通 + 左摇杆语义），结构一致；
  **待维护者反馈具体失效按键后定向修复**
- [ ] 存档跨启动验证（#15）：`home/` 读写保留
- [ ] 性能基线（#16）：菜单/场景帧率与 `[sf/mem]` 曲线

### P2 — 稳定性 / 收尾
- [ ] **内存监控（P2 降级）**：实测 2GB（MemTotal 2011932 kB，无 swap），OOM 风险低于早前判断（§10）；仍监控 `[sf/mem]` 作为基线
- [ ] greak 继承项复核（§8.7）：`quality.c`/`present_fill.c`/`display_contract` 4:3 vs 16:9 实机复核
- [ ] 残留告警：`April` atlas 缺失、GameManager singleton 时序告警
- [ ] 清理：JNI 日志限流（`executeMainThreadJobs` 刷屏）、`String.getBytes` 截断、run19 未复现并发 SIGSEGV 监控；取证埋点回收（`SF_EGLTRACE`/`AlertDialog`/`[sf/capture]`）、audio.c 头部 "Blasphemous" 注释订正；bender 产物（459MB）是否删除

---

## 环境 / 凭据
- 测试机：192.168.31.218，EmuELEC 4.7（ee47），root/emuelec（EE_PASS 可覆盖）
- 资源目录分裂：`roms/ports/<game>/` 放游戏资源（含 game.apk、二进制、lib、assets），`roms/ports_scripts/` 放 `.sh` 启动脚本
- 部署方式：覆盖二进制走 `.new` + `mv` 原子替换 + sha256 校验；先两轮 `kill -9` 确认进程归零再替换
- Git：单一事实源 `D:\ProgramData\nextos-framework`，origin SSH（Gitee），master 分支本地提交，明确说"提交/推送"才推送
