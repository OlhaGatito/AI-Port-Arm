[English](LICENSING.en.md) · [Português](LICENSING.md) · [中文](LICENSING.zh-CN.md)

# 许可、署名与再分发

[English](LICENSING.zh-CN.md)

合集与集成的作者：**NextOS** — https://github.com/NextOs-Ports。

本合集中的示例与验证新代码使用 GPL-3.0-only。所导入的组件保留各自的许可。根目录下的 `LICENSE` 文件包含 GPLv3；它并不表示整棵目录树、所有移植或游戏都采用同一许可。

他人可以依据这些许可使用并再分发相关材料，包括不带商业性游戏数据的 ZIP。他们必须保留所需的署名/许可声明，其中既包括 NextOS 的也包括第三方的，并且在适用的许可有要求时提供相应的源代码。经过修改的 ZIP 必须标明为已修改。游戏并不会因此归属 NextOS。

## 非商业限制的审议状态

维护者希望限制商业利用并保留署名。**本次首次的私有准备不改动既有许可。** GPL 允许销售，也不接受额外叠加的全面商业禁止条款。创建本仓库同样不会取消已经授予的 MIT 或其他许可。

新的指南文本可以考虑采用 CC BY-NC-SA 4.0，代码片段则单独授权。该选项在发布前的编辑/法律审核中仍待决定。它不会叠加在 GPL 衍生代码之上。真正独立拥有且权利可控的软件，在经过兼容性审查后可以另择许可。按 OSI 的定义，非商业限制并不属于开源许可。

- [GNU：销售 GPL 副本](https://www.gnu.org/licenses/gpl-faq.en.html#DoesTheGPLAllowMoney)
- [GNU：附加限制条款](https://www.gnu.org/licenses/gpl-faq.en.html#NoMilitary)
- [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/)
- [Creative Commons：不要将 CC 用作软件许可](https://creativecommons.org/faq/#can-i-apply-a-creative-commons-license-to-software)

## 需要专门阅读的案例

- Freedom Planet 2：GitHub 的检测将其显示为 MIT，但其 NOTICE 标明 loader/hooks/GLES 为 GPL-3.0-only、辅助组件为 MIT。自动检测不能替代这一定性范围。
- Forager：GPL-2.0-only。将其作为独立参考纳入，并不等于授权把它的代码混入 GPLv3 的核心部分。
- Beach Buggy 与 FF4：GPLv3 许可位于各自游戏的子目录中。
- PartyBoard/Pikmin：MIT 仅限于 NextOS 的贡献部分；本合集的选取并不包含该游戏完整重建的代码树。

既有的上游声明在各自对应的内容范围内优先。在将合集公开之前，需要复核隐私遗漏与法律声明的完整程度。NextOS 的署名既不取代第三方的署名，也不构成对原游戏的权属证明。
