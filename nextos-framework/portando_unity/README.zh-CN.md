# 移植 Unity — NextOS 选定版本

[English](README.en.md) · [Português](README.md) · [中文](#)

若想不依赖商业数据即可体验，可使用 [原创 Unity 工程](../examples/zh/engines/README.md) 与 [着色器实验室](../examples/zh/shader-lab/README.md)。主机端实验室已通过，Unity 编辑器构建仍待完成。

本双语版把 `portando_unity` 研究资料适配为 **15 个具有公开仓库的案例**，另附两款通用工具与合成夹具。其中两个额外端口持有明确授权的社区分发卡。其余本地游戏的源码与配方未予收录。

## 阅读顺序

1. [构建三态判定](zh/TRIAGE.md)：身份、运行时、契约与参考对象选择。
2. [图形与纹理](zh/GRAPHICS-AND-TEXTURES.md)：GLES2、ETC1/双平面、alpha、缩放与内存。
3. [输入与音频](zh/INPUT-AND-AUDIO.md)：真实消费方、短按与生命周期。
4. [分界诊断](zh/DIAGNOSTICS.md)：症状、测量、原因与反证。
5. [Freedom Planet 2：从 Vulkan 到 GLES2](zh/FP2-VULKAN-GLES2.md)：基于公开源码的深入专题。

## 具有公开源码的案例

| NextOS 端口 | 选定笔记中的 Unity 版本 | 阅读 |
| --- | --- | --- |
| Suzy Cube | 2017.4.40f1 | [案例](cases/zh/suzycube.md) |
| Freedom Planet 2 | 2018.4.36f1 | [案例](cases/zh/fp2.md) |
| Dead Trigger | 2019.4（补丁待确认 / patch to confirm） | [案例](cases/zh/deadtrigger.md) |
| Terraria | 2021.3.56f2 | [案例](cases/zh/terraria.md) |
| Horizon Chase | 2022.3.33f1 | [案例](cases/zh/horizonchase.md) |
| Bomb Chicken | 2022.3.39f1 | [案例](cases/zh/bombchicken.md) |
| Prizefighters 2 | 2022.3.62f2 | [案例](cases/zh/pf2.md) |
| Oceanhorn: Chronos Dungeon | 2022.3.61f1 | [案例](cases/zh/oceanhorn.md) |
| Hitman GO | 2022.3.67f2 | [案例](cases/zh/hitmango.md) |
| Huntdown | 2022.3.47f1 / 6000.2.6f2 | [案例](cases/zh/huntdown.md) |
| Skateboard Party 3 | 2022.3.45f1 | [案例](cases/zh/skate3.md) |
| Sally Face | 2022.3.62f3 | [案例](cases/zh/sallyface.md) |
| Merchant of the Skies | 6000.4.2f1 | [案例](cases/zh/merchantskies.md) |
| Nameless Cat | 6000.3.11f1 | [案例](cases/zh/namelesscat.md) |
| Party Hard GO | 6000.3.10f1 | [案例](cases/zh/partyhard.md) |

每张卡片指向目录中选定的公开提交。笔记中的历史结果并不自动证明该 HEAD 可用。Huntdown 的 2022 与 6000.x 两套配置各自独立；第二套的标题/菜单证据不构成已通过的玩法证明。

## 经社区分发获得授权的案例

- [Stranger Things 3: The Game](../catalog/zh/community/strangerthings3.md)：Unity 6000.2.6f2，技术卡不含代码快照。
- [AVGN I & II Deluxe](../catalog/zh/community/avgn12deluxe.md)：Unity 2019.4.16f1 与 V6 BYO-data 参考；不改变 V5。

没有其他标题被类推或仅因存在于本地文件而导入。全部为 NextOS 端口；第三方组件保留各自的署名与许可。

## 工具与界限

[ETC1 规划器](diagnostico/texturas/zh/README.md)与[短按分析器](diagnostico/toque/zh/README.md)是离线工具。它们不连接设备、不运行游戏、也不产生实机验收。代码与夹具保留哈希；指南有完整的 PT/EN 版本。

[SOURCE-MAP.json](SOURCE-MAP.json) 记录选定笔记的来源/哈希、与公开代码的对应关系及复制的通用文件。跨领域指南经适配以删除对未入选游戏的引用。图库、数据、日志与私有证据不随本版发布。

开始移植前，请先阅读 [AI 移植指南](../docs/zh/AI-PORTING.md) 与 [ARM 构建指南](../docs/zh/BUILD-ARM.md)。保留 V5、固件 SDL、原生流程以及 NXExtract/NXSplash 接口。共享行为的改动属于 V6 开发线，不属于本版文档。
