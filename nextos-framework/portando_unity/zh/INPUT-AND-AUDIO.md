# Unity：输入、音频与生命周期

[English](../en/INPUT-AND-AUDIO.md) · [Português](../pt-BR/INPUT-E-AUDIO.md) · [中文](#)

SDL 发现设备并投递事件；引擎必须按构建的契约消费它们。队列非空并不能证明某个按键已抵达动作。同样的区分也适用于已产出的 PCM 与可闻的声音。

## 1. 找到输入消费方

识别 InControl、Rewired、Input System、自定义 IL2CPP 方法或原生触摸。记录枚举、签名、设备 ID、时间窗与消费线程。在游戏语义动作之前，先用固件/PortMaster 把手柄归一化。

| 参考 | 值得研究的契约 |
| --- | --- |
| [Suzy Cube](../cases/zh/suzycube.md) | InControl 与已被 SDL 变换过的轴 |
| [Oceanhorn](../cases/zh/oceanhorn.md) | 经由 KeyEvent/MotionEvent 的 Rewired |
| [Prizefighters 2](../cases/zh/pf2.md) | 托管 Input System、StateEvent 与语境化鼠标 |
| [Huntdown](../cases/zh/huntdown.md) | 按配置区分的 GamePad 方法 |
| [Party Hard GO](../cases/zh/partyhard.md) | 单一方向权归属，上下文/焦点释放 |
| [Nameless Cat](../cases/zh/namelesscat.md) | SDL_Init 之前的规范化准入 |

不要把游戏键码当作通用表照抄。同一个物理 A 键不得同时确认与取消。菜单/玩法切换必须释放按住的状态，并保留玩家身份。

## 2. 保留短按

DOWN 与 UP 可能发生在两帧原生帧之间。只保留最终状态会丢掉这次点击。要在完整的时间窗内观察 SDL 来源、Android 投递与游戏消费方，至少覆盖事件之后的那一帧。

使用[短按分析器](../diagnostico/toque/zh/README.md)在显式轨迹中检测丢失、重复、缺失释放与覆盖不足。该分析器既不注入事件，也不能把一份不完整的日志变成游戏缺陷的证据。

按消费方 API 实现队列/锁存，保留顺序、时长与所有权。测试快速点击、长按、拖拽、释放、取消、失焦与上下文切换。为了“确保”点击而补充额外的 DOWN 事件会产生重复。

## 3. 语境化光标

当界面需要指针时，使用清晰的箭头、右摇杆连续移动配合径向死区、渐进响应与逐帧计时。菜单中用 R3 点击；玩法中保留原生动作/相机。没有已证实的需求时，不要为移动光标接管方向键或主键。

用真实的内容矩形变换坐标，Y 位置与增量要一致转换。光标画在正确位置，但若落点使用另一套坐标空间，仍可能把点击投递到错误处。

## 4. 重建真实音频

记录后端、对象/虚表、init/start/pause/stop、回调、采样率、声道、格式与帧数。FMOD 仍是混音器；适配器通过 OpenSL、AudioTrack、AAudio 或实际使用的接口搬运 PCM。

不要伪造缺失库的成功打开。不要凭 Unity 版本去选一个“典型”采样率。采样率不匹配会加快或拖慢声音；把字节与帧混淆可导致欠载、重复或混音器过度推进。

[Sally Face](../cases/zh/sallyface.md) 需要执行 FMODAudioDevice.run 线程；[Terraria](../cases/zh/terraria.md) 把 fmodGetInfo 与 DirectByteBuffer 绑定；[Merchant of the Skies](../cases/zh/merchantskies.md) 说明为何虚假可用的 AAudio 会让错误路径保持激活。

## 5. 验证完整生命周期

测试焦点、暂停/恢复、场景切换、重连、偏好设置、存档/读档与退出。SELECT+START 必须走已批准的流程退出，必要时在终止前完成持久化。不要用 `_exit` 跳过存档或初始化。

仅凭队列日志、JNI 调用或 PCM 振幅，不足以构成完整体验。把已消费的事件、可闻的声音、画面与退出绑定到同一个可执行文件/数据组合。开发新适配器时保留参考与 V5。
