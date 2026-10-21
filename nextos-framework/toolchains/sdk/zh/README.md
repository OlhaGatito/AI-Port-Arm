[English](../README.en.md) · [Português](../README.md) · [中文](README.md)

# 公开开发 SDK

本 SDK 使用旧版库构建 Linux AArch64/ARMv7 代码，并执行 CPU 测试。该配方仅使用公共源码；不依赖任何 NextOS 私有镜像。容器目标平台为 `linux/amd64`。设备的 GPU 仍在此环境之外。

## 1. 构建环境

在 Linux 上准备 Git、Python 3.11 或更高版本、Docker，以及私有仓库的访问权。克隆完整历史；`--depth 1` 可能遗漏被固定的 commit。在根目录执行：

```sh
mkdir -p work
docker build --platform linux/amd64 -t nextos-public-sdk:1 toolchains/sdk
docker image inspect nextos-public-sdk:1 --format '{{.Id}}'
```

构建需要网络以拉取公共依赖；以下运行步骤使用 `--network none`。请记录镜像 ID。无需将镜像发布到 registry。首次构建需要编译工具，耗时较长；此后 Docker 会复用其镜像层。

## 2. 明确已固定的内容

[sources.json](../sources.json) 与 [Dockerfile](../Dockerfile) 固定了 Debian Buster 的 digest、Debian/security 归档索引的哈希、CMake 3.22.6 的 wheel、Git 2.45.2 源码以及 Python 3.11.9。APT 依旧校验包的签名与哈希。关闭有效期校验仅限于历史归档文件，并不关闭签名校验。

新版 Git 可解析该配方 helper 所需的 V5 对象格式。新版 Python 能解释配方权威信息中使用的 AST 字面量；Python 3.7 不适用于此步骤。本 SDK 中的 Git 是为本地操作而编译的，不含 HTTP 传输：请在主机端完成源码的克隆/下载，之后再挂载 checkout。

所有包的版本位于镜像内的 `/opt/nextos-sdk-packages.tsv`。AArch64 版 SDL2/EGL/GLES2 的头文件/库属于本 SDK；不会拷贝到某个移植中。运行时使用的是固件自带的库。

## 3. 编译与审计 AArch64

```sh
docker run --rm --network none --user "$(id -u):$(id -g)"   -v "$PWD:/src:ro" -v "$PWD/work:/src/work"   -e NEXTOS_AARCH64_CC=/usr/bin/aarch64-linux-gnu-gcc   -e NEXTOS_SYSROOT=/ nextos-public-sdk:1 bash -c '
set -eu
cmake -S examples/shims-reference -B work/sdk-arm64   -DCMAKE_TOOLCHAIN_FILE=/src/toolchains/linux-aarch64.cmake
cmake --build work/sdk-arm64 --parallel 2
python3 tools/audit_elf.py work/sdk-arm64/shim-reference-nextos
'
```

此处 `NEXTOS_SYSROOT=/` 描述的是 **multiarch 容器的根**，由交叉编译器选择其 ARM 库。它并不意味着使用宿主机的根目录，或是把卡片复制一份当作 SDK。所测试示例要求 GLIBC 2.17。

对于 ARMv7，请使用 `NEXTOS_ARMV7_CC=/usr/bin/arm-linux-gnueabihf-gcc`、`toolchains/linux-armv7.cmake`，另一个构建目录以及 `tools/audit_elf.py --machine ARM`。所测试的 ARMv7 示例要求 GLIBC 2.4。这并不证明可在 Android ARMv7 上运行，也不证明 softfp 桥接可用。

## 4. 迈向第一个移植

请遵循 [集成示例](../../../examples/first-port/README.md)。它使用 Clang 编译一个不带 libc/NDK 的 freestanding Android guest，并用 GCC 编译 Linux loader。这个精简配置并不能替代 NDK——对于需要其头文件、libc 或 C++ 运行时的 Android 项目而言如此。

参考：[Debian 归档](https://www.debian.org/distrib/archive.en.html)、[官方 Debian 镜像](https://hub.docker.com/_/debian)、[CMake](https://cmake.org/)、[Python 源码](https://www.python.org/downloads/release/python-3119/)。另见 [验证与边界](../../../publication/ONBOARDING-UPDATE.md)。
