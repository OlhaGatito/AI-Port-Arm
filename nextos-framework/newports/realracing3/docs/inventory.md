# 迁移清单与来源凭证

本文件记录 `port/` 与设备上 payload 的来源，供审计用。仅记录标识信息（数量、字节、摘要），不含任何 Firemint/EA 文件内容。

## 1. 参考代码

| 项 | 值 |
| --- | --- |
| 来源仓库 | `E:\Programs\NextOs-Ports\realracing3-arm-linux` |
| 迁入时 commit | `296b38b881fd02f592e9bbe9ef0977122a32f38f`（2026-09-21，"Update Real Racing 3 port.json"） |
| 迁入方式 | `git ls-files -z \| tar --null -T - -cf - \| tar -xf -`，即只取该 commit 的受版本控制文件，不带其工作区杂物与 `.git` |
| 落地位置 | `newports/realracing3/port/`（237 个文件） |
| 迁入后本项目的改动 | `port/Makefile`（CXXSTD 探测）、`port/tools/collect_libs.sh`（dpkg 归属查找）、`port/Dockerfile.build-armhf-zen2`（新增） |

署名保持 NextOS，`LICENSE` 与 `NOTICE.md` 未改动。

## 2. 游戏数据（owner 提供，仅本机与测试设备持有）

| 项 | 值 |
| --- | --- |
| 本地目录 | `newports/realracing3/apk/`（gitignore，永不入库） |
| 文件数 | 11656 |
| 总字节 | 2451751887（2.28 GiB） |
| 设备位置 | `/storage/roms/ports/realracing3/` |
| 传输方式 | `tools/sync_payload.py`，tar 流经 SSH exec 通道，增量分批 |
| 校验 | 远端清单比对 11656 个文件、0 个不一致；随机 6 个文件 sha256 全部 MATCH |

关键文件摘要（sha256 前 16 位，用于确认设备上的是同一份 2.7.0）：

| 文件 | 字节 | sha256(16) |
| --- | --- | --- |
| `lib/armeabi-v7a/libRealRacing3.so` | 11150536 | `dcad5752d22ea3df` |
| `lib/armeabi-v7a/libfmodex.so` | 1045372 | `9ddba4c527ef2877` |
| `dataoffsets.txt` | 1805 | `ff48f15259046775` |
| `asset_list_base.txt` | 265306 | `1ee1c9b8cdb8e5a9` |

`libRealRacing3.so` 的字节数与 `Real Racing 3.sh` 里的 `EXPECTED_SIZE=11150536` 一致，即 2.7.0 原版，非重签名或地区变体。

资产桶实测存在三个：`assets_480x320/`、`assets_960x640/`、`assets_2048x1536/`。这与 `dataoffsets.txt` 的 `#SEC:res:4`（480×320 / 768×576 / 960×640 / 1024×768）不是同一套，见 ANALYSIS.md R7。

## 3. 构建产物

| 项 | 值 |
| --- | --- |
| 构建镜像 | `rr3-build:armhf-zen2`，由 `portmaster-builder:aarch64-armhf-zen2` 加装 armhf 开发库后 `docker commit` 得到 |
| 编译器 | `arm-linux-gnueabihf-g++` (Ubuntu 9.4.0)，`-std=gnu++2a -O2 -g` |
| 产物 | `port/build/realracing3`，**11752696 字节**，ELF 32-bit ARM EABI5 **PIE**（`Type: DYN`、`Machine: ARM`、entry `0x19671`），interpreter `/lib/ld-linux-armhf.so.3`，sha256(16) `227104069febdff3`。此即**首次出画面那一次实机运行所用的二进制**（`logs/run-5.txt:17`、`run-6.txt:17` 的 `loader: 11752696 bytes` 与之相符） |
| GLIBC 上限 | `max GLIBC_2.29`，`check_glibc_floor: all clear, everything fits glibc <= 2.31`（容器内对**上述新产物**重跑，非沿用旧结论；`port/tools/check_glibc_floor.sh`） |
| 随包库 | `libbsd.so.0`(73KB) `libbz2.so.1.0`(48KB) `libcrypto.so.1.1`(1648KB) `libgcc_s.so.1`(97KB) `libstdc++.so.6`(1244KB) `libz.so.1`(73KB) `libzip.so.5`(62KB)，全部 ≤ GLIBC_2.29 |

设备 `/usr/lib32` 实测：有 `libSDL2-2.0.so.0.3200.10`、`libz.so.1.3.1`、`libstdc++.so.6.0.33`、`libgcc_s`、`libbz2`；**没有** `libzip.so.5`、`libbsd.so.0`、`libcrypto.so.1.1`。

因此本次上机只上传后三个。理由：启动脚本把 `libs.armhf` 放在 `LD_LIBRARY_PATH` 最前面，若把构建镜像里更旧的 `libz.so.1`(1.2.11) 与 `libstdc++.so.6`(6.0.28) 也放上去，会盖掉设备上更新的版本 —— 那是降级，不是兜底。发布 zip 时是否仍按 `collect_libs.sh` 全量打包，属于 PortMaster 发布阶段的问题，本轮不裁定。

## 4. 实机调试回路（本轮建立，四个工具互不重叠）

| 工具 | 何时用 | 关键行为 |
| --- | --- | --- |
| `tools/device_probe.py` | 只读探测设备（OS/GL/内存/磁盘/进程），不改不删 | `--cmd` 可追加任意只读命令；输出落 `logs/device-probe-*.txt` |
| `tools/deploy_port.py` | 构建后上传 | 默认 `--no-libs`（见 §3 的降级理由），并回读目录与启动器头部自校验 |
| `tools/device_run.py` | 跑一次、拿完整日志 | 远端 `timeout` 包裹，日志走 `log.txt`（launcher 自己重定向），落 `logs/run-N.txt` |
| `tools/device_capture.py` | **判据 = 屏幕本身** | 后台起游戏 → 等日志出现指定帧 → 取图 → 回传 PNG → 收尾 kill；`--attach` 只拍不启动（配合 `device_run` 用）。两条取图通路：`--source game`（默认）走端口自己的控制通道回读 `glReadPixels`，启动时自动导出 `REALRACING3_CONTROL_DIR`，**两个驱动下都出图**；`--source compositor` 走 `grim`，在 vendor Mali 直接 scanout 的机器上只能抓到整屏黑 |

`device_capture.py` 存在的理由就是本项目那条铁律：`exit 0`、帧计数、声音都不算画面，只有抓到的像素算。它与 `device_run.py` 分开，因为「改一个环境变量再看屏」的循环比重传 11 MB 加载器频繁得多。换驱动复测（PLAN §11）之后，主判据从 `grim` 改成端口自己的回读——同一张整屏黑在两条通路上含义相反，只有回读能区分「引擎正在淡出」与「抓不到」。

## 5. 尚未记录

- 设备端运行日志、存档、任何截图：不入库（`.gitignore:27-32` 已覆盖 `apk/ gamedata/ deploy/ logs/ home/ lib/`）。
- 测试设备地址与口令：不写入任何跟踪文件。`tools/` 四个工具一律只接受 `--host`/`DEVICE_HOST` 与 `--password`/`DEVICE_PASS`，缺失即报错退出；原先内置的 CFW 默认口令表已删除，只保留「哪个 CFW 用哪个用户名」这一条惯例（`tools/device_probe.py:22-26`）。
