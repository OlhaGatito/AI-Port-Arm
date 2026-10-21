# PORTS — catálogo e status

Duas coleções convivem neste repositório. Este doc mapeia quem é quem. Dados de jogo nenhum está aqui.

## A) Ports de trabalho "pessoais" (base `nextos_ports_android-master/ports/`)

| Port | Jogo / engine | ABI | Status | Notas de desbloqueio |
|---|---|---|---|---|
| `bully` | Bully Anniversary (Rockstar) | arm64 | ✅ 100% jogável (inédito) | hooks 4B, `SDL2-mali`, fix do Jimmy, TEX_LIGHT/HALF, asset O(log n) |
| `revc` | GTA Vice City (reVC) | arm64 (2 módulos) | ✅ 100% jogável | `pthread_bridge` recursiva (deadlock cutscene), MAX_LIGHTS 4, `MAX_LEVEL` não existe em GLES2 |
| `sonicmania` | Sonic Mania Plus (RSDKv5 Netflix) | arm64 | ✅ 100% jogável COM SOM | bypass Oboe (mix direto na thread SDL), cloud-save via `CallCallback`, telemetria no-op |
| `dysmantle` | DYSMANTLE (10tons/GameActivity) | arm64 | 🟨 quase (muro de render "mundo branco") | TLS canary 256B, Oboe via dlopen, Paddleboat+SDL, KTX ETC2 |
| `nfs` | NFS Most Wanted 2012 (Blast/Firemonkeys) | armhf | 🟨 ~95% do boot (muro shadergen) | malloc +64B, sigaction wrapper, softfp shim, parse OBB |
| `cuphead` | Cuphead Mobile (Unity IL2CPP) | arm64 | 🚧 título renderiza | $PC=9 ($GC use-after-free resolvido); faltam Rewired/FMOD |
| `re4` | RE4 fan (Unity 2018 Mono) | arm32 | 🚧 boot GLES2 OK (GC Mono trava) | so_util ARM32, pthread_shim, softfp, cgroup 620MB |
| `dusklight` | Zelda TP fan (Aurora decomp, SDL3) | arm64 | 🚧 F0/F1 | TODOS os PT_LOAD + RELR + jni GetEnv; F2 = env TLS |
| `gtavc` | GTA VC Android (RenderWare) | armhf | 🚧 menu renderiza | perfil Mali-400 spoof, swap/low-mem, IPL filtrados |
| `ShadowFight2` | Unity 2017 IL2CPP | arm64 | ❌ scaffold (70 UNKNOWN) | workflow via MSYS2 no Windows documentado |
| `nfstest` | scaffold armhf | armhf | ✅ teste | base de toolchain `armv8a-emuelec-gnueabihf` |

> ⚖️ Alguns desses ports incluem no repo **binários de jogo** extraídos de APKs (ex.: `revc/libreVC.so`).
> ESTES binários **NÃO são publicados** neste dump (ver [EXCLUSOES.md](EXCLUSOES.md)); o código-fonte
> dos loaders/shims foi mantido.

## B) Ports de referência NextOS (`nextos-framework/ports/` — 40 repos/46 títulos)

Baseline Mali-450/GLES2 revisada, código aprovado e comprovado com gates (76→103→141→V5 congel):

Unity: `suzycube` (2017.4.40f1), `fp2` (2018.4.36f1 — NOTICE prevalece sobre detector),
`bombchicken-gunbrick`, `huntdown` (2022/6000), `sor4`, `skate3`, `merchantskies` (6000.4.2f1),
`namelesscat` (6000.3.11f1), `partyhard` (6000.3.10f1), `summertimesaga`,
`beastiebay` (newports)…

Outras engines: `stardewvalley`, `terraria`, `kotor`, `oceanhorn`, `horizonchase`, `castleofillusion`,
`sallyface`, `deadtrigger`, `ff4` (GPL-2.0 próprio), `beachbuggy` (GPLv3 nas subpastas),
`scourgebringer`, `tasm2` (vendoriza box64/sdl2-compat), `pikmin`/`partyboard` (MIT só NextOS)…

## C) Em desenvolvimento (`nextos-framework/newports/`)

| Port | Engine | Estado |
|---|---|---|
| `bad2bad` | Unity 2022.3.62f3 IL2CPP | título renderizando no aparelho; dados do dono |
| `broforce` / `broforce_zh` | Unity | docs/análise + variante ZH (fontes.conf) |
| `60parsecs` | Unity 6000.0.58f2 | módulos típicos estruturados; DRM do publisher identificado |
| `greakmemoriesofazur`, `doubledragongaiden` | Unity | PLAN/ANALYSIS + `il2cpp_trace.c` |
| `beastiebay` | Kairosoft | UnityPy + deployment scripts |
| `realracing3` | EA Blast | fronteira softfp/ARMHF documentada |
| `ragingloop` | NÃO-Unity (Java/GLES 1.x) | abordagem própria stb_image/truetype |

## D) Catálogo comunitário (`nextos-framework/catalog/community/`)
`strangerthings3`, `avgn12deluxe` — fichas PT/EN, código não importado.

## Status de licenças importantes (ver LICENSING.md do framework)
- Novas contribuições de framework: **GPL-3.0-only**.
- FP2: NOTICE prevalece; Forager GPL-2.0 (não misturar no núcleo); Beach Buggy/FF4 GPLv3 subpastas.
- PartyBoard/Pikmin MIT limitado às contribuições NextOS (não cobre árvores reconstruídas).
