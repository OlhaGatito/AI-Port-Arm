# TROUBLESHOOTING — sintoma → causa → solução

Consolidado de: `docs/pt-BR/PROBLEMAS-COMUNS.md` (framework), `facilitando_o_trabalho/troubleshooting/`,
STATUS/README dos ports, e experimentos (hollow-recon, re4-recon).

## Crash e execução

| Sintoma | Causa provável | Solução |
|---|---|---|
| `Undefined Symbol` no run | falta no `imports.gen.c` | adicionar stub/implementação às pendências UNKNOWN |
| SIGILL | instrução ARMv8.4 em Cortex-A53 v8.0 OU telemetria anti-tamper | usar toolchain certa; no-op na função de telemetria (Sonic `Stats::TryTrackStat`) |
| SIGSEGV `< 0x10000` | sentinel-read do jogo em memória zero | recuperação automática: decodificar LDR/STR no PC, zerar reg destino, pular 4B (template/main.c), MAX_RECOVERY_STREAK 64 |
| `malloc(): invalid size` | std::string overflow tolerada pelo bionic | padding +64B no malloc (ver RECEITAS §2) |
| Deadlock em init_array | pthread/PT_LOAD incompleto | pthread_shim + copiar TODOS os PT_LOAD |
| `GLIBC_x.y not found` | overlink | **rebuild só do candidato novo** com sysroot compatível; NUNCA mexer no manifesto para silenciar |
| `crt1.o`/`-lc ausente` | sysroot incompleto / linkando objeto x86 | reconstruir sysroot; separar host x build |
| `file in wrong format` / `Exec format error` | confusão de arquivo/sistema | separar Linux/Android, ARM32/ARM64; `audit_elf.py` antes |
| `EGL BAD_ALLOC 0x3003` | EGL cru no Mali | usar **SDL2-mali** |
| `EGL BAD_MATCH 0x3009` | config não bate | `eglGetCurrent*` + `eglQuerySurface(CONFIG_ID)` para casar a config real |
| vm->GetEnv crash idx 6 | table JNI incompleta | GetEnv/Attach implementados (JNI indexado) |
| `Android_JNI_GetEnv` fault | env cacheada inválida | verificar TLS via `pthread_key_fake` |
| import resolvido + crash depois | contrato implementado errado (cast genérico) | teste dirigido do contrato específico |
| null vindo de JNI | classe/método/campo inexistentes | resolver assinatura REAL no APK; implementar corretamente |

## Vídeo e áudio

| Sintoma | Causa provável | Solução |
|---|---|---|
| Áudio OK mas tela preta | depth-buffer/shader/Z-clip; SwapBuffers não chamado | RECEITAS §1 + trace por frame (hollow-recon part 3) |
| Frame prova `BLACK`/`DEAD-CONTEXT` 3× | drawable 1×1 (`post-first-present` V4-GRAPHICS-04) | invalidar vídeo; medir pixels ANTES do present (3 amostras conclusivas) |
| "áudio com tela preta" informado como sucesso | inventário falso | política: **invalidar vídeo** e reprovar o release |
| `SDL2_mixer unavailable` | mix não suportado | bypass Oboe → `opensles_shim_set_mixfn` (Sonic) |
| áudio engasga | poll bloqueando | caps de 5 ms; pump antes/depois |

## Dispositivo e sistema

| Sintoma | Causa provável | Solução |
|---|---|---|
| Wedge/cena pesada trava | redundância de memória de textura da GPU (Utgard, não RAM!) | `*_TEX_LIGHT`/`*_TEX_HALF`; verificar `free -m` + swap |
| ETA infinito em aparelho sem Python | gdb não pega bp em heap RWX | log persistente + `sync` + scan manual de stack |
| ETXTBSY ao scp/deploey | binário órfão ~40 s | `rm` no device / `pkill -9 -x` + `sleep 3` |
| Loop 100% CPU após hook | trampolim colide | 4B hook com pool ±128 MB |
| NXExtract "só aceita pasta pronta" | receita falsa | receita real + teste do zero (recipe-check) |
| launcher retorna antes de log | fronteira pré-runtime | separar log de bootstrap do log do jogo |
| mappings confirm/cancel invertidos | eventos duplicados/dupla autoridade | deduplicar eventos; só uma fonte de autoridade |
| toque fora do botão | drawable/viewport/origin-Y errado | content rect e inversa de toque (nxgl_display) |
| eventos A/B invertidos (Trimui) | layout Nintendo | mapa próprio (android_shim) |
| cloud-save spinning (Netflix) | callbacks JNI async não entregues | entregar callbacks via `CallCallback` + patch conflict-state |

## Suíte NX framework

| Sintoma | Causa provável | Solução |
|---|---|---|
| Unity trava no 3º frame | sync/present | medir; patch Swappy apenas no perfil comprovado |
| `verify.py` falha de docs | par PT/EN quebrada | `publication/verify-docs.py` + `languages.json` |
| `nxscan.py` bloqueia literal | segredos/literals no código gerado | usar anotação de tipo (padrão público do nxrelease) |
