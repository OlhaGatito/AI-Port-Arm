# RECEITAS-MALI450 — armas contra cada muro de hardware

> Consolidado de `facilitando_o_trabalho/receitas/` (01–04), ports reais (Bully, reVC, Sonic Mania,
> DYSMANTLE, NFS, RE4) e `hollow-recon/MALI450-SHADER-MOONSHOT.md`.
> P específicos de build/ABI: ver [TROUBLESHOOTING.md](TROUBLESHOOTING.md).

## 1. Shaders (a "Alquimia")
| Sintoma | Receita |
|---|---|
| compile shader falha `P0004` (highp no FS) | **highp→mediump SÓ no fragment shader** |
| shader grande demais `L0005` (limite ~512 palavras de VS) | reduzir constantes: ex. MAX_LIGHTS 8→4 |
| texturas escuras/invisíveis | `GL_LUMINANCE` no Mali-450 lê como (L,L,L,L) → **converter na CPU para RGBA8888** |
| texturas com filtro errado | sized `glTex*`→unsized + filtro mipmap→`GL_LINEAR` |
| roupas/efeitos somem dentro de efeitos (Bully) | **fix do Jimmy**: `glClear` força COLOR apenas FORA de FBO (`g_in_fbo`) |
| im2d com Z errado | z-fix do im2d + z-buffer real ao invés de Z-clipping |
| jogos GLES3 no Mali-450 (motores GLES3-only) | **spoof GLES3 + tradutor GLES3→GLES2 em runtime** (veja `experiments/hollow-recon`) — 18 shaders traduzidos/provados; stubs de verdade em `my_dlsym` (loop MSAA: `glGetInternalformativ` deve ser stub que zera) |

## 2. ABI / pthread (a "Ponte de Vidro")
| Muro | Receita |
|---|---|
| Deadlock futex em init_array (Unity) | **pthread_shim**: mutex/cond/rwlock/once precisam de bridge bionic (40B)→glibc (48B), com mapa de ponteiros; `pthread_t` é ok |
| Deadlock de cutscene (reVC) | `pthread_bridge` lazy + recursivo para tudo; mutex bionic sempre |
| `sigaction` crash (bionic struct 16B vs glibc 132B) | **wrapper `my_sigaction`** traduzindo campo a campo; ignora `si_code<=0` |
| `__stack_chk_fail` aleatório (Squirrel) | canary neste stack em `tpidr_el0+0x28` colide com TLS glibc → **pad de 256 B no exe** |
| `malloc(): invalid size` (std::string overflow tolerada bionic) | **malloc/calloc/realloc/memalign com +64 B de padding** |
| float ABI (Unity 2018 arm32) | softfp guest (AAPCS) em host hard-float → `softfp_shim.c` `pcs("aapcs")` (44 wrappers NFS/RE4) |
| OOM falso em proc 32-bit | overcommit reserva ~1 GB virtual → `vm.overcommit_memory=1` + swap 2 GB (`vm.swappiness=20`) |
| Toolchain (NEON) | `armv7-a -mfpu=neon -mfloat-abi=hard` (scaffold `nfstest`) |

## 3. Áudio
| Muro | Receita |
|---|---|
| `SDL2_mixer unavailable` / Oboe não suportado | **bypass**: rotear mix direto na thread SDL (`opensles_shim_set_mixfn`) forçando volumes (Sonic Mania Plus FEZ a receita) |
| Oboe real (DYSMANTLE) | interceptar dlopen do Oboe → opensles shim; PCM float32→S16 |
| áudio engasga no poll | cap do `ALooper_pollAll` em **5 ms** + pump de callbacks antes/depois |
| backend `dummy` por trás do som | INVALIDAR como sucesso: exigir backend real do SDL do aparelho |

## 4. Relocações/loader
| Muro | Receita |
|---|---|
| init_array crash +0 | libmain com **9 PT_LOAD mas só 2 copiados** → copiar TODOS os PT_LOAD |
| vtables destruídas | `.relr.dyn` (RELR do clang-21) → implementar handler RELR (bitmap odd/even) |
| `vm->GetEnv` em idx 6 crash | JNI indexado: GetEnv/Attach implementados, resto vtable de stubs preenchidos por spec |
| Hook colide com função adjacente (loop 100% CPU) | trampolim 16B → **hook de 4 bytes com pool de trampolins ±128 MB** (Bully) |
| patch de strings e re-execução de código | `so_make_text_writable` → patch → `so_finalize/flush_caches` |

## 5. Memória/VRAM (o "Guardião")
| Muro | Receita |
|---|---|
| wedge/perf em cenas grandes | **limite de memória de TEXTURA da GPU Utgard** (não RAM!) → `*_TEX_LIGHT`/`*_TEX_HALF`; prova com log `free -m` + swap |
| asset archive lento (60k itens) | índice binário busca O(log n) (qsort+bsearch) |
| PC presa em loop illegível | `kill -SEGV <pid>` para capturar PC |
| device sem Python no gdb | log persistente + `sync`; scan de stack manual |

## 6. Regras de ouro do portador (Mali-450 / kernel 3.14)
1. **Direct-to-hardware**: SDL2-mali por padrão, EGL cru como último recurso.
2. **Ponte bionic→glibc** sempre, nunca confiar na ABI igual.
3. **Cada engine comercial (FMOD, Oboe, Mono GC, Squirrel, Rewired) merece estágio próprio.**
4. Instrumentar por `env` (`NFS_*`, `CUP_*`, `HK_*`, `RE4_*`, `BULLY_*`) em vez de patches invisíveis.
5. Tela preta com som = depth/shader/Z-clipping. Buscar nessa ordem.
6. Sempre `tee run.log` com timestamps. Handoff com próxima sessão.
7. Device com SSH em hang por pilha crashed → log persistente + análise off-device.
