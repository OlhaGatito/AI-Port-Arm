# ARQUITETURA — as duas trilhas do porting Android → ARM

## A) Suíte NextOS Framework V5 (`nextos-framework/`)

Suite congelada e auditada: tag `framework-v5` @ `657fb65a23b5c3b20040e76307b27e6470b1d17c`,
7676 hashes em `publication/v5-export.json`, `verify.py` PASS. Três camadas:

1. **Pre-main** — launcher Bash → `nxbootstrap` (PortMaster, PT_INTERP, flock, NXExtract isolado,
   NXSplash 5s, `generation_runtime` schema 3 com rollback saudável).
2. **Runtime comum** (estático no loader): `nxloader` (so-loader ELF32/64, relocations, `finalize()`
   icache+RELRO, `begin_patch()` quiescente, softfp AAPCS↔ARMHF), `nxcompat` (probe→plano→apply→receipts,
   `NEXTOS_SETTINGS/1`), `nxgl` (piso Mali-450/GLES2, frame proof irrevogável), `nxinput`
   (mapping soberano + GPTK runtime), `nxandroid` (lifecycle Android como contrato C99; JNI/surface
   pertencem ao adapter), `nxobs` (crash handler, mem/perf, `nx-device-launch`).
3. **Adapter** (`port/src/`) — a única parte nova por jogo.

Ferramentas host: `tools/inventory_apk.py` (inventário read-only de APK), `tools/find_reference.py`
(referência por contrato), `tools/pin_collection.py` (pins executáveis), `tools/recover_reference.py`,
`tools/audit_elf.py` (glibc ≤ 2.30, recusa RPATH), `nxgenerator` (manifest `nxproject` v3 → árvore
nova no-replace), `nxrelease` (validate→stage→verify→bundle com SBOM e `nxscan.py` anti-segredos),
`nxdoctor` (diagnóstico `.nxruntime/`), `nxledger` (identidade derivada, nunca digitada).

Contratos: `framework/contracts/declarative-v1.json`; evidências: `framework/catalog/`
(42 checks/entrada) e `framework/evidence/CFW-EVIDENCE.json` (promoção fail-closed, 7 critérios).

Versões das linhas (todas fechadas): **V1** arquitetura base (76 gates, 2026-08-22) ·
**V3** runtime de produção com GPTK/SETTINGS/generations (103 gates) · **V4** prova automática de
controles por uinput no aparelho + frame proof + empacotamento pós-C7 (141 gates) · **V5** exportação
sanitizada para publicação. Mudança de comportamento só em **V6** (não existe nesta árvore).

## B) Base de ports `.so` Android (`nextos_ports_android-master/`)

Trilha imperativa usada nos ports "de pessoa física" (Sonic Mania, Bully, GTA VC, NFS…):

- **core/**: `so_util` (carrega/reloca `.so` Android em memória), `egl_shim` + `opensles_shim`
  (contracts GLES/OpenSL ES sobre SDL2), `error`, `hashmap`, `imports`, `util`.
- **template/**: `CMakeLists.txt` (opção `USE_GLES2`; OFF → GLESv1_CM), `src/main.c` é o fluxo canônico
  de loader (crash handler com PC+backtrace FP, recuperação de sentinel-crash ARM64, `so_load`→
  patch de strings→relocate→resolve→finalize→hooks de GOT→`execute_init_array`→`android_main`),
  `android_shim` (fake NDK completo: native_app_glue, input SDL→keycodes Android, looper com cap
  de 5 ms para áudio), `jni_shim` (vtable 512 slots, jstrings tagged por tabela, IDs= tagged pointers).
- **tools/**: `new-port.sh <apk> <nome>` (arm64: extrai, sanity-check readelf, classifica todos os
  UND em pthread/egl/opensles/gles/jni/android/log/cxx/abi/passthrough/UNKNOWN e gera
  `imports.gen.c` + relatório de % auto-resolvido; ex. reais: Cuphead 66 UNKNOWN, ShadowFight2 70,
  Dusklight 248), `new-port-arm.sh` (ARMv7: so_util REL, imports mínimos, fallback
  `dlsym(RTLD_DEFAULT)`, multi-módulo `main.c.tmpl` com `libc++_shared.so` primeiro), `build-port.sh`
  (compila com toolchain NextOS + sysroot SDL2).
- **docs de referência**: `docs/reference/lswtcs-src/` — LEGO Star Wars TCS de mtojek como origem do
  padrão; `facilitando_o_trabalho/` (receitas 01–04 + troubleshooting).
- **experiments/**: PC-first (bully/bully-pc), hollow-recon (Unity 2020 IL2CPP, spoof GLES3 +
  tradutor GLES3→GLES2), re4-recon (Unity 2018 Mono arm32, cgroup 620 MB).

## Como as trilhas se combinam

Dream pipeline para um jogo novo:
1. `inventory_apk.py` (qual engine/família?) → `find_reference.py` (quem já fez similar?).
2. Se Unity/IL2CPP/Mono/Godot… → caminho NextOS V5 (nxandroid + guias de runtime).
3. Se engine proprietária (.so sem framework) → caminho `nextos_ports_android`: `new-port.sh` → wire
   UNKNOWN → shims → o mesmo ciclo de debug (receitas Mali-450).
4. Sempre: auditar ELF, provar no aparelho, empacotar (PortMaster) e documentar handoff.
