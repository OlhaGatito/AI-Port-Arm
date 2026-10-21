# Beastie Bay（开拓神秘岛）— 移植分析（阶段 A→C，2026-10-04）

> 所有结论来自本机文件实测（pyelftools / 字节级读取）与设备实测，无假设填充。

## 1. 游戏身份（实测，勿再猜）

| 项 | 值 |
|----|----|
| Android 包名 | `net.kairosoft.android.mujin_en`（manifest 实测；APK 文件名《开拓神秘岛》为中文发行名） |
| 厂商 | **Kairosoft**（开罗社，kairo.android.plugin 自家插件；2D 像素经营模拟） |
| 引擎 | **Unity 2018.4.24f1**（globalgamemanagers 头部实测），**IL2CPP metadata v24**（sanity 0xfab11baf） |
| ABI | 仅 `lib/arm64-v8a` |
| native 库 | **仅 3 个**：libmain.so(6KB) / libunity.so(9.8MB) / libil2cpp.so(24.3MB)——**零第三方插件** |
| Java 层 | kairo.android.plugin(Utility/ad/Config) + AdMob/Tapjoy/计费(广告中性化即可) + com.cm.androidforunity(Payment/Web) |
| 数据 | assets/bin 70MB（散装 1240 文件，无 data.unity3d）、assets/js+com 共 65KB |
| 广告/内购 | BILLING + AdMob + Tapjoy —— 全部可中性化 |

## 2. Native 对象（pyelftools 实测）

| 文件 | NEEDED | 决策 |
|------|--------|------|
| libmain.so | liblog, **libstdc++**, libm, libdl, libc | ✅ 标准 split loader（JNI_OnLoad → dlopen libunity） |
| libunity.so | **libmain.so, libc, libm, libandroid, libdl, liblog, libGLESv2, libstdc++, libz, libEGL** | ✅ 2018 代特征：直接链接 libGLESv2（非仅 eglGetProcAddress）；ALooper×2 + ANativeWindow×6 + ASensor×14 + egl×19 |
| libil2cpp.so | liblog, **libstdc++**, libm, libdl, libc | ✅ 188 未定义，`_ctype_`/`environ` 已在骨架 bionic 表 |

2018 代差异（相对 ddgaiden/2021.3）：
1. **libunity 直接 NEEDED libmain.so** —— 装载顺序已满足（main 先于 unity）。
2. **直接 NEEDED libGLESv2.so** —— 需要 GLESv2 符号入 import 表（骨架的 broforce(2017.4) 支持已覆盖：sf_gl_sym/egl 表）。
3. **无 Swappy/Choreographer**（2018.4 早于 Swappy）—— ddgaiden 的 vsync 等待问题不存在。
4. **libstdc++.so NEEDED** —— 骨架 bionic.c 的 fake_libs 覆盖（bionic libstdc++ 是转发壳，符号面极小：new/delete operators）。
5. Java 面是经典 `com/unity3d/player/UnityPlayer(+Activity)`（骨架 jni.c 原生支持）。

## 3. 数据布局（实测）

```
assets/bin/Data/          散装 1240 文件（globalgamemanagers + level0 + sharedassets*, 无 data.unity3d）
assets/js, assets/com     共 65KB（厂商 WebView/JS 资源，可忽略）
```

## 4. 目标设备画像（192.168.31.196，ArkOS，ark/ark 实测）

| 项 | 实测值 |
|----|--------|
| 机型 | xf40h, 内核 4.4.189, aarch64 |
| 系统 | ArkOS（Ubuntu 19.10 底座），**glibc 2.30**（= 公共端口上限，构建门禁 ≤2.30 恰好吻合） |
| GPU | **Mali-G31 Bifrost**（/usr/local/lib/aarch64-linux-gnu/libmali-bifrost-g31-rxp0-gbm.so）——GLES 3.x 预期 |
| 面板 | **720×720 方屏**（fb0 virtual_size），/dev/dri/card0 存在（KMSDRM） |
| SDL2 | 2.0.26（/usr/lib/aarch64-linux-gnu/） |
| 内存 | 918MB（1GB 级纪律同 ddgaiden） |
| 布局 | **ArkOS 单目录**：/roms/ports 存在（可写），**无 ports_scripts**——启动 .sh 直接放 /roms/ports/ 下 |
| 权限 | ark 用户可直接写 /roms/ports；sudo（密码 ark）可用 |

> brief 中 "Mali-450 / armhf 路径" 为陈旧模板描述，实测设备为 aarch64 + Mali-G31（与 ddgaiden 的情况相同模式），按实测执行。

## 5. 最接近的 NextOS 案例与路线（阶段C 结论）

| 维度 | 复用来源 | 说明 |
|------|----------|------|
| 主骨架 | **newports/ddgaiden/port**（60parsecs 血统，FMOD-free 最新变体） | nx_elf+bionic+jni+android+egl_sdl+audio+input 全家；proven 跨 2017.4→Unity 6 |
| 同代实证 | **ports/fp2-nextos（2018.4.36f1）** | 同代 LTS 已发布案例；其 jar:→open() 翻译证实 2018 代 jar 消费方行为 |
| 老代实证 | newports/broforce（2017.4.40f1） | 骨架家族兼容更老 Unity（GLESv2 直链/essl1） |
| 显示降级件 | greak 系 quality/present_fill/display_contract | 720×720 方屏 vs 游戏 16:9/4:3 —— 显示契约层处理 |

**路线**：复制 ddgaiden port 骨架 → 摘除 xlua/tapsdk/GetSdkParam/XOR-0x09 等 ddgaiden 专属 →
换身份（net.kairosoft.android.mujin_en）→ 广告/计费 Java 面中性化 → 编译 → 部署首跑。

## 6. 风险清单（首跑前已知）

1. **metadata v24 较老**：il2cpp 导出面与 v29 不同（gc 函数名可能缺）——[sf/gc] 探针可能拿不到遥测，不影响运行。
2. **libstdc++ 符号**：libmain/libil2cpp/libunity 都 NEEDED libstdc++（bionic 壳）——若重定位报 new/delete 未解析，需在 bionic 表补 operator new/delete 转发到宿主 glibc。
3. **Kairosoft Java 插件**：C# 会调 kairo/android/plugin/Utility（广告/通知）——auto-stub 返回声明类型活对象（60parsecs 既有策略），按日志补具体回调。
4. **720×720 方屏**：游戏 16:9 内容在方屏上的显示契约（display_contract.c）需首跑后复核。
5. **1GB 内存**：bin 70MB 全量小，风险低于 ddgaiden。
