[English](../../../examples/engines/README.en.md) · [Português](../../../examples/engines/README.md) · [中文](README.md)

# 各个引擎的原创练习

用于探究每个技术家族边界的小型示例。这些是练习项目，不是商业移植，也不是 [集成式 ARM64 示例](../first-port/README.md) 的替代品。打开项目前请把编辑器项目复制到 `work/`，使缓存和构建产物不落在源码目录里。

## Mono：内存布局、回调与 P/Invoke

[Program.cs](../../engines/mono/Program.cs) 调用原创的 [native.c](../../engines/mono/native.c)，采用顺序布局、Cdecl、保留的回调以及错误传播。在宿主机上准备 Mono/mcs 和 C 编译器：

```sh
mkdir -p work/mono-training
cc -shared -fPIC -O2 examples/engines/mono/native.c   -o work/mono-training/libnextos_training.so
mcs -out:work/mono-training/Training.exe examples/engines/mono/Program.cs
mono work/mono-training/Training.exe
```

观察到的结果：`PASS: managed/native layout, callback lifetime and error propagation`。本练习是一个 Linux 托管主机。它**不实现 Mono Android/Xamarin、程序集存储、Bionic 或 Activity**。这些步骤属于 [Mono Android 指南](../../../docs/zh/MONO-ANDROID.md)；区分这两条路线也是课程的内容。

## Godot：带可测试逻辑的原创项目

使用[官方 Godot 3.5.3](https://godotengine.org/download/archive/3.5.3-stable/)。把 `NEXTOS_GODOT` 设为你的可执行文件路径。headless 版本用于测试逻辑；图形编辑器/运行程序以 GLES2 打开项目。

```sh
mkdir -p work/engine-training
cp -R examples/engines/godot work/engine-training/godot
"$NEXTOS_GODOT" --path work/engine-training/godot --script test_model.gd
```

观察到的结果：移动逻辑、delta 上限与收集逻辑全部 PASS。要实际游玩，请用 Godot 3.5.3 打开副本，或执行图形版程序并带 `--path` 且不带 `--script`。方向键移动，Escape 保存并退出。不要使用 headless 执行来声称有画面。[project.godot](../../engines/godot/project.godot) 固定为 GLES2；Linux/ARM 导出需要对应的模板，以及 [Godot 指南](../../../docs/zh/GODOT.md) 中所述的测试。

## Unity：创建自己的 Android 输入

安装并授权 Unity 2019.4.40f1，包含 Android/IL2CPP 模块及其受支持的工具链。把 `NEXTOS_UNITY_EDITOR` 设为编辑器路径。该项目只包含原创脚本：

```sh
mkdir -p work/engine-training
cp -R examples/engines/unity work/engine-training/unity
"$NEXTOS_UNITY_EDITOR" -batchmode -quit   -projectPath "$PWD/work/engine-training/unity"   -executeMethod TrainingBuild.Android -logFile -
```

[TrainingBuild.cs](../../engines/unity/Assets/Editor/TrainingBuild.cs) 创建场景、选择 IL2CPP/AArch64/GLES2，并在工作副本中生成 `Build/training.apk`。[Training.cs](../../engines/unity/Assets/Training.cs) 绘制游戏画面并通过 PlayerPrefs 存档。**本次评审未执行该编辑器构建。** 安装编辑器/模块/授权不属于 C SDK 的范围。移植这个输入仍需要研究 Unity 并实现其适配器；它不是第一课中的 freestanding 访客。

## Cocos2d-x：供模板集成的场景

[TrainingScene.cpp](../../engines/cocos/TrainingScene.cpp) 与其[头文件](../../engines/cocos/TrainingScene.h) 实现了绘制、键盘输入、收集以及 UserDefault。请使用 Cocos2d-x 3.17.2 上游的 C++ 模板，commit `1528ea01d2749b4ef65e97f79cffa6135fe13c4d`，在新的项目中连同其依赖一起使用。[上游 CMake 配方](https://github.com/cocos2d/cocos2d-x/blob/cocos2d-x-3.17.2/templates/cpp-template-default/CMakeLists.txt) 标识了哪些文件进入构建目标。

把这两个文件复制到 `Classes/`；将 `Classes/TrainingScene.cpp` 加入 `GAME_SOURCE`，把头文件加入 `GAME_HEADER`。在 `AppDelegate.cpp` 中包含 `TrainingScene.h`，并把初始场景的创建替换为 `TrainingScene::create()`。使用 640×480 的设计分辨率，并保留模板中的 GLView/生命周期创建。通过模板的 CMake 项目构建；Android 还需要该引擎版本自带的 Gradle/NDK 项目。

**本次评审未编译 Cocos 集成。** 它需要上游 checkout/依赖以及自己的图形测试。[Cocos2d-x 指南](../../../docs/zh/COCOS2D-X.md) 覆盖了单个原创场景无法演示的 Android 适配、JNI、文本与音频。
