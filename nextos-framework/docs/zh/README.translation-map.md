# docs/zh 目录翻译映射表

状态：20 篇已全部完成（2026-09-26）。翻译时以英文版为主源、葡文版核对术语；zh 下统一使用英文文件名。

源文件 -> 中文输出（docs/zh/ 下统一使用英文文件名，便于上游 diff 对照）。

| 源文件 | 中文文件 | 状态 |
| --- | --- | --- |
| docs/en/GETTING-STARTED.md | GETTING-STARTED.md | 已完成 |
| docs/en/AI-PORTING.md | AI-PORTING.md | 已完成 |
| docs/en/SHIMS.md | SHIMS.md | 已完成 |
| docs/en/NXEXTRACT.md | NXEXTRACT.md | 已完成 |
| docs/en/TESTING.md | TESTING.md | 已完成 |
| docs/en/ANDROID-RUNTIMES.md | ANDROID-RUNTIMES.md | 已完成 |
| docs/en/ANDROID-INVENTORY.md | ANDROID-INVENTORY.md | 已完成 |
| docs/en/PINS-AND-SOURCES.md | PINS-AND-SOURCES.md | 已完成 |
| docs/en/COCOS2D-X.md | COCOS2D-X.md | 已完成 |
| docs/en/GODOT.md | GODOT.md | 已完成 |
| docs/en/GAMEMAKER-ANDROID.md | GAMEMAKER-ANDROID.md | 已完成 |
| docs/en/RENPY-ANDROID.md | RENPY-ANDROID.md | 已完成 |
| docs/en/HAXE-LIME-ANDROID.md | HAXE-LIME-ANDROID.md | 已完成 |
| docs/en/NATIVE-ANDROID.md | NATIVE-ANDROID.md | 已完成 |
| docs/en/ARCHITECTURE.md | ARCHITECTURE.md | 已完成 |
| docs/en/BUILD-ARM.md | BUILD-ARM.md | 已完成 |
| docs/en/DIAGNOSTIC-LAB.md | DIAGNOSTIC-LAB.md | 已完成 |
| docs/en/TROUBLESHOOTING.md | TROUBLESHOOTING.md | 已完成 |
| docs/en/README.md | README.md | 已完成 |
| (仅葡语) PRIMEIROS-PASSOS.md | GETTING-STARTED.md | 已完成 |
| (仅葡语) PORTAR-COM-IA.md | AI-PORTING.md | 已完成 |
| (仅葡语) TESTES-E-ENTREGA.md | TESTING.md | 已完成 |
| (仅葡语) ARQUITETURA.md | ARCHITECTURE.md | 已完成 |
| (仅葡语) COMPILAR-ARM.md | BUILD-ARM.md | 已完成 |
| (仅葡语) DIAGNOSTICO-GUIADO.md | DIAGNOSTIC-LAB.md | 已完成 |
| (仅葡语) PROBLEMAS-COMUNS.md | TROUBLESHOOTING.md | 已完成 |
| (仅葡语) INVENTARIO-ANDROID.md | ANDROID-INVENTORY.md | 已完成 |
| (仅葡语) PINS-E-FONTES.md | PINS-AND-SOURCES.md | 已完成 |

## 第二批：仓库其余目录（2026-09-26）

| 源文件 | 中文文件 | 状态 |
| --- | --- | --- |
| AGENTS.md | ../AGENTS.zh-CN.md | 已完成 |
| CONTRIBUTING.md | ../CONTRIBUTING.zh-CN.md | 已完成 |
| LICENSING.md | ../LICENSING.zh-CN.md | 已完成 |
| catalog/README.md | catalog/zh/README.md | 已完成 |
| catalog/community/avgn12deluxe.md | catalog/zh/community/avgn12deluxe.md | 已完成 |
| catalog/community/strangerthings3.md | catalog/zh/community/strangerthings3.md | 已完成 |
| examples/engines/README.md | examples/zh/engines/README.md | 已完成 |
| examples/first-port/README.md | examples/zh/first-port/README.md | 已完成 |
| examples/shader-lab/README.md | examples/zh/shader-lab/README.md | 已完成 |
| examples/shims-reference/README.md | examples/zh/shims-reference/README.md | 已完成 |
| toolchains/sdk/README.md | toolchains/sdk/zh/README.md | 已完成 |

注：根目录三篇采用 `.zh-CN.md` 后缀（与 README.zh-CN.md 一致），其余各目录采用 `zh/` 子目录并在其中沿用英文文件名。

## 第三批：publication 与语言登记（2026-09-26）

| 源文件（葡语/英文） | 中文文件 | 状态 |
| --- | --- | --- |
| publication/ONBOARDING-SIMULATION.md | publication/ONBOARDING-SIMULATION.zh-CN.md | 已完成 |
| publication/ONBOARDING-UPDATE.md | publication/ONBOARDING-UPDATE.zh-CN.md | 已完成 |
| publication/REVIEW.md | publication/REVIEW.zh-CN.md | 已完成 |
| publication/V5-REVIEW-20260912.md | publication/V5-REVIEW-20260912.zh-CN.md | 已完成 |
| publication/VALIDATION.md | publication/VALIDATION.zh-CN.md | 已完成 |

注：publication 沿用第二批的根目录约定，采用 `.zh-CN.md` 后缀与 en/pt 同级并列。这 5 篇已在 `publication/languages.json` 中登记 `zh` 配对；`catalog/android-runtimes.json` 中被 guides 直接引用的配对保持两键，以免破坏 `tests/test_android_runtimes.py` 的配对断言。

## 第四批：portando_unity（2026-09-26）

| 源文件（葡语/英文） | 中文文件 | 状态 |
| --- | --- | --- |
| portando_unity/README.md | portando_unity/README.zh-CN.md | 已完成 |
| portando_unity/README.en.md | portando_unity/README.zh-CN.md | 已完成 |
| portando_unity/cases/suzycube.md | portando_unity/cases/zh/suzycube.md | 已完成 |
| portando_unity/cases/fp2.md | portando_unity/cases/zh/fp2.md | 已完成 |
| portando_unity/cases/deadtrigger.md | portando_unity/cases/zh/deadtrigger.md | 已完成 |
| portando_unity/cases/terraria.md | portando_unity/cases/zh/terraria.md | 已完成 |
| portando_unity/cases/horizonchase.md | portando_unity/cases/zh/horizonchase.md | 已完成 |
| portando_unity/cases/bombchicken.md | portando_unity/cases/zh/bombchicken.md | 已完成 |
| portando_unity/cases/pf2.md | portando_unity/cases/zh/pf2.md | 已完成 |
| portando_unity/cases/oceanhorn.md | portando_unity/cases/zh/oceanhorn.md | 已完成 |
| portando_unity/cases/hitmango.md | portando_unity/cases/zh/hitmango.md | 已完成 |
| portando_unity/cases/huntdown.md | portando_unity/cases/zh/huntdown.md | 已完成 |
| portando_unity/cases/skate3.md | portando_unity/cases/zh/skate3.md | 已完成 |
| portando_unity/cases/sallyface.md | portando_unity/cases/zh/sallyface.md | 已完成 |
| portando_unity/cases/merchantskies.md | portando_unity/cases/zh/merchantskies.md | 已完成 |
| portando_unity/cases/namelesscat.md | portando_unity/cases/zh/namelesscat.md | 已完成 |
| portando_unity/cases/partyhard.md | portando_unity/cases/zh/partyhard.md | 已完成 |
| portando_unity/pt-BR/TRIAGEM.md | portando_unity/zh/TRIAGE.md | 已完成 |
| portando_unity/pt-BR/GRAFICOS-E-TEXTURAS.md | portando_unity/zh/GRAPHICS-AND-TEXTURES.md | 已完成 |
| portando_unity/pt-BR/INPUT-E-AUDIO.md | portando_unity/zh/INPUT-AND-AUDIO.md | 已完成 |
| portando_unity/pt-BR/DIAGNOSTICO.md | portando_unity/zh/DIAGNOSTICS.md | 已完成 |
| portando_unity/pt-BR/FP2-VULKAN-GLES2.md | portando_unity/zh/FP2-VULKAN-GLES2.md | 已完成 |
| portando_unity/diagnostico/texturas/README.md | portando_unity/diagnostico/texturas/zh/README.md | 已完成 |
| portando_unity/diagnostico/toque/README.md | portando_unity/diagnostico/toque/zh/README.md | 已完成 |

注：`portando_unity/zh/` 沿用 `portando_unity/en/` 的英文文件名（葡语名到英文名的映射：TRIAGEM→TRIAGE、GRAFICOS-E-TEXTURAS→GRAPHICS-AND-TEXTURES、INPUT-E-AUDIO→INPUT-AND-AUDIO、DIAGNOSTICO→DIAGNOSTICS）；`cases/zh/` 沿用各案例英文名；根 README 沿用 `.zh-CN.md` 后缀。上述 23 条中除被 `catalog/android-runtimes.json` 的 guides 直接引用的根 README 配对外，已在 `publication/languages.json` 登记 `zh` 键。链接校验：23 篇中文文档、286 条本地链接、失效 0。

## 第五批：ports（2026-09-26）

| 源文件（英文） | 中文文件 | 状态 |
| --- | --- | --- |
| ports/<port>/README.en.md | ports/<port>/README.zh-CN.md | 已完成（40 个 port） |

`ports` 下 40 个 port 目录的主 README 完全同构（各 27 行，均为 NextOS 参考卡），差异仅在四处：标题游戏名（第 1 行）、是否附 Android 运行时指南链接（第 5 行）、仓库名与 commit（第 7 行）、源文件数与隐私/内容省略数（第 13 行）。中文版由 `.workbuddy/gen_ports_zh.py` 解析上述字段后套统一模板生成，未手工转录，因此各 port 的数字与 commit 与英文版严格一致。

输出约定：沿用第二批的根目录写法，中文页与各语言源同级并列为 `README.zh-CN.md`；`upstream/` 下的上游 README 属 AGENTS.md 定义的已固定参考源，字节不得改动，本次不译。

游戏译名策略：有通行中文译名的取「原名（中文译名）」并列（如 `Streets of Rage 4（怒之铁拳 4）`）；无通行译名的（Huntdown、ScourgeBringer、Tearscape、Swordigo、Forager Nuclear、Prizefighters 2）保留英文原名，不臆造译名。

40 条配对已在 `publication/languages.json` 登记 `zh` 键（其中 8 条因被 `catalog/android-runtimes.json` 的 guides 直接引用而按既定规则排除）。链接校验：40 篇中文文档、337 条本地链接、失效 0。

配套调整：`catalog/zh/README.md` 的「公开仓库来源」表共 44 行（40 个 port 目录对应 44 个游戏标题，部分目录含两部作品），其「来源」列原指向葡语 `README.md`，现已全部改指中文 `README.zh-CN.md`；该表 58 条本地链接失效 0。
