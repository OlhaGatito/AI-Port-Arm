# GUIA-DE-PORT — passo a passo completo

Do APK ao aparelho. Fonte: guias `docs/pt-BR/` do framework, `PORTAR-COM-IA.md`,
`examples/first-port/`, `facilitando_o_trabalho/receitas/` e os handoffs dos ports reais.

## Fase 0 — Preparar o ambiente
1. WSL2 Ubuntu 24.04 (ou Linux nativo): Git, Python 3.11+, CMake 3.20+, `readelf`, C99.
2. Docker e SDK: `docker build --platform linux/amd64 -t nextos-public-sdk:1 nextos-framework/toolchains/sdk`.
3. Valide a coleção: `python3 publication/verify.py` (PASS esperado).

## Fase 1 — Inventário (sempre antes de escolher loader)
```sh
python3 tools/inventory_apk.py base.apk config.arm64_v8a.apk --output work/inventory.json
```
Roteia por família: unity / mono-android / godot / cocos2d-x / gamemaker / renpy / haxe-lime / native-android.
**Sanity-check rápido** (scaffolder): o `.so` tem `android_main`? → NativeActivity, caminho fácil.
Tem JNI/Java-heavy? → port mais difícil (fake JNI/shims). Tem `JNI_OnLoad`? → lifecycle pelo Java.

## Fase 2 — Escolher referência POR CONTRATO (não por nome)
```sh
python3 tools/find_reference.py --engine Unity --abi arm64-v8a --renderer GLES2
```
Consulte também `catalog/profiles.json` e os 40 ports de referência em `nextos-framework/ports/`
(dir: `ports/<id>-nextos/`, manifesto `SOURCE-MAP.json` com sha256 por arquivo).

## Fase 3 — Scaffold do port
```sh
# arm64 (so-loader + fake JNI/android_shim, imports.gen.c classificado)
bash nextos_ports_android-master/tools/new-port.sh ~/meujogo.apk meujogo
# ARMv7 (so_util REL + fallback dlsym + shims bionic; libc++_shared sempre PRIMEIRO)
bash nextos_ports_android-master/tools/new-port-arm.sh ~/meujogo.apk meujogo
```
Saída: `ports/meujogo/{src/, build.sh, launch.sh, README.md}` + relatório
`AUTO-RESOLVIDOS: X / TOTAL (N%)` e lista `>>> A IMPLEMENTAR (UNKNOWN) <<<`.

## Fase 4 — Wire dos imports (a arte)
Ordem efetiva das pendências:
1. `__*_chk` (FORTIFY) → mapear para versões normais da glibc;
2. pthread → `pthread_fake`/`pthread_bridge` (mutex bionic 40B vs glibc 48B, SEMPRE bridge);
3. EGL/OpenSLES → `_shim` sobre SDL2 (nunca EGL cru no Mali-450 → BAD_ALLOC 0x3003);
4. JNI: `jni_shim_set_package()` + jstrings C-strings reais (jstring falso ≠ `char*`!);
5. `__system_property_get` → devolver hardware "conhecido", senão o jogo degrada/trava;
6. Resto → stub com permissão explícita + TODO. **Nunca catch-all que retorne sucesso.**

## Fase 5 — Compilar
```sh
bash nextos_ports_android-master/tools/build-port.sh ports/meujogo gles2
# ou dentro do port: cmake -DCMAKE_TOOLCHAIN_FILE=$SR/usr/etc/cmake-aarch64-libreelec-linux-gnu.conf \
#   -DUSE_GLES2=ON -B build && cmake --build build -j$(nproc)
python3 tools/audit_elf.py ports/meujogo/build/meujogo
```
Copy para o device: `.so` do jogo + assets + `launch.sh` (PortMaster).

## Fase 6 — Desbloqueio pela ordem nativa
bibliotecas/relocações → imports → finalize (icache/RELRO) → construtores (DT_INIT & INIT_ARRAY,
uma vez) → `JNI_OnLoad` → `create/resume` → **frames** → áudio → input → saves → pause → destroy.
**Uma fronteira por vez.** Notas:
- `Undefined Symbol` no run → imports.gen.c.
- Crash em `init_array` → falta copiar um PT_LOAD (copiar TODOS!) ou pthread bridge.
- Deadlock futex em ctor 76 (Unity) → `pthread_shim`.
- SIGILL → instrução ARMv8.4 em Cortex-A53 v8.0 (toolchain errada) ou telemetria (Sonic: no-op).
- Tela preta COM som → depth/shader/Z-clip → [RECEITAS-MALI450.md](RECEITAS-MALI450.md).
- SIGSEGV em `malloc(): invalid size` → padding +64B no malloc (bionic tolera overflow que glibc não).

## Fase 7 — Provar no aparelho (nunca aprovar release só por SSH)
- Reproduzir o env do unit do frontend: `nx-device-launch` (framework) / `systemctl stop emustation` + env mali.
- Frames medidos (frame proof); áudio provado com backend real (recusar `dummy` como sucesso);
  controles por uinput/hardware real.
- Logs: sempre `2>&1 | tee run.log` com timestamps + `nx-support-bundle`.

## Fase 8 — Empacotar e publicar
```sh
python3 nextos-framework/framework/nxrelease/nxrelease.py bundle \
  --authority human --manifest nxrelease.json --stage stage/ --destination release/ --archive-name jogo.zip
python3 -B nextos-framework/publication/verify.py     # docs PT+EN em paridade
```
Regras de release: nenhum APK/OBB/BIOS/key no pacote; dados BYO; SBOM gerado.

## Handoff (padrão dos ports reais)
Todo port deve ter um `RESUMO-PROXIMA-SESSAO.md`/`HANDOFF.md` com: estado exato, último comando,
última linha de log, env-config vencedora, e os 2–3 próximos ataques já dimensionados.
