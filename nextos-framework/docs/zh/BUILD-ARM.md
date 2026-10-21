[English](../en/BUILD-ARM.md) · [Português](../pt-BR/COMPILAR-ARM.md) · [中文](../../README.zh-CN.md)

# 为 ARM 与 AArch64 构建

目标是弄清每个部件由哪个编译器构建、产出具有正确 ABI 的 ELF，并解读其要求。[公开 SDK](../../toolchains/sdk/zh/README.md) 现在提供了一套经过验证的 AArch64/ARMv7 低 glibc 构建配方。请从它与[集成示例](../../examples/zh/first-port/README.md)入手。下面这些通用路径适用于自行提供其他 toolchain/sysroot 的情形；任何主机构建都不能证明物理支持。

## 1. 理解三种构建

| 产物 | 构建环境 | 运行环境 |
| --- | --- | --- |
| C 契约测试 | 计算机自身的原生编译器 | Linux 主机 |
| `<port-id>-nextos` 加载器/适配器 | Linux 交叉编译器与 sysroot | 设备的 ARM Linux |
| 原生 Android 库示例 | NDK 与 Bionic sysroot | Android 或兼容的 Android 加载器 |

商业库已在 APK 内编译完成；创建移植并不会重新编译它。若可用，优先选择 `arm64-v8a`。`armeabi-v7a` 需要一个 ARM32 进程，并可能需要指向 ARMHF 主机的调用约定桥。没有任何编译选项能把 ELF32 变成 ELF64。Android ARMv7 使用 `softfp` 传递浮点参数，而 Linux ARMHF 接口可能使用 VFP 寄存器。[Android ABI 文档](https://developer.android.com/ndk/guides/abis)。

## 2. 先确认主机示例

```sh
cmake -S examples/shims-reference -B work/host -DCMAKE_BUILD_TYPE=Release
cmake --build work/host --parallel 2
ctest --test-dir work/host --output-on-failure
```

这能在引入交叉编译之前，把示例本身与基础工具的错误隔离开来。生成的二进制文件具有你计算机的结构，无论该目录名叫 `work/host` 还是可执行文件以 `nextos` 结尾。

## 3. 准备一致的 Linux 工具链

sysroot 需要同架构与目标发行版的头文件、链接启动文件、libc 与开发库。设备卡片的不完整副本通常缺少开发文件。请记录 SDK/sysroot 的来源、版本与摘要；私有镜像名称并不构成公开的安装配方。

下面 `/opt/...` 路径只是示例，需替换为真实位置。该脚本不会下载或安装工具。公开发行版最多只能要求 `GLIBC_2.30`；使用较新的 GCC 和较新的库，并不会仅仅因为改了清单文本就产生更旧的兼容性。

## 4. 构建 AArch64

```sh
export NEXTOS_AARCH64_CC=/opt/nextos-toolchain/bin/aarch64-linux-gnu-gcc
export NEXTOS_SYSROOT=/opt/nextos-sysroots/aarch64
test -x "$NEXTOS_AARCH64_CC"
test -d "$NEXTOS_SYSROOT"
"$NEXTOS_AARCH64_CC" --version
"$NEXTOS_AARCH64_CC" -dumpmachine
"$NEXTOS_AARCH64_CC" --sysroot="$NEXTOS_SYSROOT" -print-sysroot
cmake -S examples/shims-reference -B work/aarch64 \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/toolchains/linux-aarch64.cmake" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build work/aarch64 --parallel 2
```

[linux-aarch64.cmake](../../toolchains/linux-aarch64.cmake) 选定 Linux/AArch64、显式指定的编译器与 sysroot；它在目标上查找库/头文件，在主机上构建程序。这正是这些 [CMake 工具链](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html) 搜索选项的作用。该示例使用 C；C++ 项目在自己的工具链文件中还需要对应的 C++ 编译器与库。

更换 ABI、编译器或 sysroot 时，请使用全新的构建目录。CMake 缓存可能仍保留旧的编译器。不要把 ARM 头文件与 x86 库混用，也不要内嵌私有 SDL 来绕开检测失败。

## 5. 执行前审计

```sh
readelf -h work/aarch64/shim-reference-nextos
readelf -l work/aarch64/shim-reference-nextos
readelf -d work/aarch64/shim-reference-nextos
readelf --version-info work/aarch64/shim-reference-nextos
sha256sum work/aarch64/shim-reference-nextos
```

| 输出 | 检查项 |
| --- | --- |
| `-h` | ELF64 与 `Machine: AArch64`；文件名不能证明架构 |
| `-l` | 若存在 `INTERP`，目标机上是否有可用的 Linux 解释器 |
| `-d` | 每一个 `NEEDED` 依赖以及任何 RPATH/RUNPATH |
| `--version-info` | 所需的 GLIBC 版本；C++ 还需复核 GLIBCXX/CXXABI |
| SHA-256 | 将要测试的这批字节的身份 |

对随附交付的**每一个** Linux ELF 审计，包括辅助程序与库。`GLIBC_2.30` 是发布上限，并非兼容性的充分证明：架构、内核、SDL、EGL 与音频仍然相关。不要用 `ldd` 通过执行来分析未知的 Android ELF；应先检查它的静态信息。

不要把该 ELF 当作原生程序在 x86 上运行。本项目在交叉编译期间不注册 CTest 测试：“no tests found” 并不代表 ARM 通过了。QEMU 执行（在明确准备的情况下）必须报告为 CPU 仿真。物理 GPU/音频/输入测试属于[验证通道](TESTING.md)。

## 6. 必要时改用 ARMv7

```sh
export NEXTOS_ARMV7_CC=/opt/nextos-toolchain/bin/arm-linux-gnueabihf-gcc
export NEXTOS_SYSROOT=/opt/nextos-sysroots/armhf
test -x "$NEXTOS_ARMV7_CC"
test -d "$NEXTOS_SYSROOT"
"$NEXTOS_ARMV7_CC" -dumpmachine
cmake -S examples/shims-reference -B work/armv7 \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/toolchains/linux-armv7.cmake" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build work/armv7 --parallel 2
readelf -h -A work/armv7/shim-reference-nextos
```

检查 ELF32/ARM 与调用属性。ARMHF 加载器与 Android ARMv7 客机可能共享同一颗 CPU，却并不共享每一个函数的 ABI。在适配带 `float`/`double` 的回调之前，请先阅读 `framework/nxloader/include/nxloader_softfp.h`；结构体布局与 `pthread` 同样需要按 ABI 复核。

## 7. 构建原生 Android 库

本演练与 Linux 示例相互独立，只教授 NDK 构建。`nextos_guest_add` 接受其和可以放进 `int` 的值。它并不演示 JNI、加载器重定位或掌机上的客机执行。

```sh
mkdir -p work/android-guest
cat > work/android-guest/CMakeLists.txt <<'CMAKE'
cmake_minimum_required(VERSION 3.16)
project(nextos_android_guest LANGUAGES C)
add_library(nextos_guest SHARED guest.c)
CMAKE
cat > work/android-guest/guest.c <<'C'
int nextos_guest_add(int a, int b) { return a + b; }
C
export NEXTOS_NDK=/opt/android-ndk
test -f "$NEXTOS_NDK/build/cmake/android.toolchain.cmake"
cmake -S work/android-guest -B work/android-arm64 \
  -DCMAKE_TOOLCHAIN_FILE="$NEXTOS_NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-21 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build work/android-arm64 --parallel 2
readelf -h -d work/android-arm64/libnextos_guest.so
```

请使用有版本标识的 NDK 安装。本 ARM64 演练使用 API 21；其他项目的 API 级别应跟随其实际的 API。`ANDROID_ABI`、`ANDROID_PLATFORM` 与 `android.toolchain.cmake` 遵循 [NDK CMake 指南](https://developer.android.com/ndk/guides/cmake)。不要把这个工具链用于 Linux 加载器。

## 8. 应用到真实移植

在执行任何脚本之前，先阅读参考配方。识别缺失的输入、外部工具、编译选项、库与生成文件。为新移植编写配方时，要带可验证的 pin 与属于自己的输出目录。记录完整命令、编译器版本、sysroot、commit 与最终 SHA。构建成功仍然不能证明抽取、运行时行为或安装可用。

关于 `crt1.o`、`cannot find -lc`、“wrong format”、“Exec format error” 与 GLIBC 失败，参见[故障排查](TROUBLESHOOTING.md)。不要为了掩盖不兼容而移除某项审计。
