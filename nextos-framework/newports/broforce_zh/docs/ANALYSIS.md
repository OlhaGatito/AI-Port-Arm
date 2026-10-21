# Broforce 全中文版 — Android → ARM Linux (NextOS) 移植分析

Game: Broforce v2.0 全中文版（BILIBILI YSD2K repack，VERSION ID 3150-202312141701）。
Target: EE4.7 aarch64 手掌机（Mali-G52，KMSDRM），测试机 192.168.31.192。
基线：**完全复用英文版 broforce 移植**（`newports/broforce`，英文版经验见其 docs/ANALYSIS.md）。

## 1. APK 勘察（步骤 1-7，已验证）

- 引擎：**Unity 2017.4.40f1 (6e14067f8a9a)**，IL2CPP，Release —— 与英文版**完全同版本**。
  - zh libunity.so 与 en **同尺寸不同 md5**（19250288 B，重建差异）；libmain 同尺寸不同 md5。
  - 版本串实证：两包 libunity 内均只含 `2017.4.40f1`。
- 架构：arm64-v8a + armeabi-v7a 双目录，取 arm64-v8a。
- **lib 集差异（关键）**：zh 干净三件套 `libmain/libunity/libil2cpp`；
  **没有** en 包的 `libstub.so`/`libAPKVISION.so`（repack 站壳/签名校验层）——
  en 移植本来就把它们排除，zh 天然无需处理。
- dex 加载链：zh classes.dex 只 loadLibrary("main")（无 APKVISION/stub 引用），
  与 en 移植的加载决策一致（libmain dlopen libunity → libil2cpp）。
- 包名/Activity：`com.vocies61.broforce` + `com.unity3d.player.UnityPlayerActivity`
  —— **与 en 完全一致**，loader 身份零改动。
- 数据：`assets/bin/Data` 两侧文件名列表**完全一致**（609 项），
  但 **179 个文件内容不同**（共 17.7 MB，中文翻译嵌在数据文件内）；
  差异清单固化在 `tools/zh_diff_files.txt`。en 独有 `AVConfig.json`（语音聊天配置，zh 无，不需要）。
- libil2cpp.so：zh 40065952 B vs en 40008816 B（+57 KB，中文化补丁在游戏代码内）。

## 2. 移植决策与部署（步骤 8-10）

- **loader = fork 英文版 port**（Greak 骨架，英文物化）：
  - `src/input.c`：GPVIRT 信箱 `/tmp/broforce-gp` → `/tmp/broforce-zh-gp`（避免双版本互扰）；
  - `src/nx_elf.c`：日志前缀 `[broforce]` → `[broforce_zh]`；
  - `build.sh`：OUTPUT → `broforce-zh-nextos`；
  - GPTK 端口名保持 `broforce`（内嵌配置自洽）；包名保持不变。
- 构建：portmaster-builder:aarch64-latest，门禁通过
  （GLIBC_2.30，NEEDED 仅 SDL2/z/dl/m/pthread/libc/ld-linux），
  BuildID `739a3827`，sha256 `bb2a12be…`。
- **部署（设备上克隆底座 + 增量覆盖，避免 354MB 重传）**：
  1. `cp -a /roms/ports/broforce /roms/ports/broforce_zh`（含 lib/assets/fonts/home/conf/cache）；
  2. SFTP 批量覆盖 179 个差异 assets（10s）+ zh 三件套（.new + mv 原子替换，sha256 校验一致）；
  3. `/roms/ports_scripts/Broforce_zh.sh`（GAMEDIR → broforce_zh，其余与英文版脚本一致，
     含 libmali 前缀逻辑与平台探测）；
  4. 删除 zh 目录里克隆来的 `broforce-payload.tgz`。
- 工具：`tools/batch_upload.py`（批量 SFTP）、`tmp_prep_payload.sh`、`tmp_finalize.sh`。

## 3. 启动链路实录（步骤 11-13，一次点火通过）

- **坑**：首启 FATAL "SDL window failed: Can't load EGL/GL library" ——
  手工启动脚本没带 `/usr/lib/libmali` 前缀，SDL 拿到 GLVND 空壳。
  对照设备上英文版跑通 log（`LD_LIBRARY_PATH=/usr/lib/libmali:...`）定位。
  正式 `Broforce_zh.sh` 自带该前缀条件分支，不受影响。
- 点火实录（45s/75s 两次运行）：
  - `JNI_OnLoad(libmain) -> 0x10006` → `NativeLoader.load: libunity -> libil2cpp`
    → `initJni OK` → `UnityMain started`；
  - Mali-G52 GLES 3.2，`GLSL ES 3 accepted`，**ESSL 300 直通驱动**
    （英文版 §5.6 洋红修复随代码继承，`ok=0` 计数 = 0，无洋红风险）；
  - 1800+ 帧，`frame proof verdict=OK 29.2%`；
  - FMOD → ALSA 24000Hz×2 就绪（`fmodGetInfo/fmodProcess resolvidos`）；
  - 手柄 `recognised=1 usable=1 proven=True movement=True`；
  - `asset_pack_dir -> /roms/ports/broforce_zh`（split APK 路径应答正确）；
- **主菜单截图证实中文渲染正常**：开始/选项/关卡编辑器/自定义战役 +
  虚拟手柄按键 + BILIBILI 水印（`logs/bfzh-menu.png`）。

## 4. 状态对照（用户步骤清单）

| 步骤 | 状态 |
| --- | --- |
| 1-2 Unity 版本/架构 | ✅ 2017.4.40f1 / arm64-v8a |
| 3-6 libmain/libunity/其他 so 分析 | ✅ 复用英文版结论 + zh 差异核对 |
| 7 Java/JNI 依赖 | ✅ dex 只 load main；包名同英文版 |
| 8 最接近案例 | ✅ 英文版 broforce 移植本体 |
| 9 最小 loader | ✅ fork + 身份改造，BuildID 739a3827 |
| 10 native 加载 | ✅ main→unity→il2cpp 链路 OK |
| 11 Unity 初始化 | ✅ UnityMain/player loop |
| 12 窗口 | ✅ KMSDRM 1024x768 GLES3 |
| 13 显示画面 | ✅ 主菜单中文 UI 截图证实 |
| 14 输入 | ⏳ 手柄已识别（proven=True），待实机手感验收 |
| 15 音频 | ⏳ FMOD→ALSA 就绪，待听感验收 |
| 16 存档 | ⏳ home/ 布局已就位，待跨启动验证 |
| 17 性能 | ⏳ medium 质量档自动应用，待实测 |

## 5. 下一步

1. 维护者从 PortMaster 列表选「Broforce_zh.sh」实测：中文文本、手柄、声音。
2. 存档跨启动（`conf/`+`home/` 读写）。
3. 性能基线（2D 像素游戏预期无压力；`[sf/mem]` 观察）。
