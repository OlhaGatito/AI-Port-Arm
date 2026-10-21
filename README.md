# AI-Port-Arm 🐱

<div align="center">

[![framework](https://img.shields.io/badge/NextOS--Framework%20V5-congelado%20%40%20657fb65a-2563eb?style=for-the-badge)](https://github.com/OlhaGatito/AI-Port-Arm)
[![base ports android](https://img.shields.io/badge/base%20ports%20Android-Apache--2.0-16a34a?style=for-the-badge)](https://github.com/OlhaGatito/AI-Port-Arm)
[![ports de referência](https://img.shields.io/badge/ports%20de%20referência-40%20repos%20%2F%2046%20títulos-7c3aed?style=for-the-badge)](docs/PORTS.md)

**Repositório da IA do ecossistema Gatito** — dump organizado do framework de porting
Android → handhelds ARM Linux (R36S, RG40XX H, RK3326 etc.) + base de ports de PC.

</div>

---

## O que é

Distribuição de **duas bases de trabalho** preservando a estrutura original:

| Pasta | Conteúdo | Licença |
|---|---|---|
| [`nextos-framework/`](nextos-framework/) | Suíte **NextOS Framework V5** congelada (tags V1→V5): módulos `nx*` (nxabi, nxbootstrap, nxcompat, nxgl, nxinput, nxloader, nxandroid, nxaudio, nxobs, nxextract, nxgenerator, nxrelease, nxdoctor, nxledger, nxsplash, portmaster), catálogo de 40 repos/46 títulos, guias PT/EN/zh, exemplos e testes de gates | GPL-3.0 |
| [`nextos_ports_android-master/`](nextos_ports_android-master/) | **Base prática de ports `.so` Android**: core (`so_util`, `egl_shim`, `opensles_shim`, fake JNI/android_shim), template, scaffolder (`tools/new-port*.sh`), 11 ports de trabalho (Bully ✅, GTA Vice City ✅, Sonic Mania Plus ✅, DYSMANTLE 🟨…) e receitas/troubleshooting | Apache-2.0 |

> ⚠️ **Nenhum dado de jogo está aqui.** APK/OBB/BIOS/assets/bibliotecas proprietárias devem vir
> do seu próprio backup legal. Veja [docs/EXCLUSOES.md](docs/EXCLUSOES.md) para o que ficou fora e como obter.

## Requisitos

- Linux (recomendado WSL2 Ubuntu 24.04) com Git, Python 3.11+, CMake 3.20+, C99, `readelf`
- Docker (para o SDK `nextos-public-sdk:1`) — opcional, mas recomendado
- Alvo: handheld ARM com Mali-450/GLES2 (muOS, ROCKNIX, ArkOS, Knulli…) — teto **glibc 2.30**

## Como usar (início rápido)

```sh
# 1. Verificar integridade da coleção do framework
python3 publication/verify.py                      # dentro de nextos-framework/

# 2. Compilar exercício host de shims
cmake -S examples/shims-reference -B work/host -DCMAKE_BUILD_TYPE=Release
cmake --build work/host --parallel 2 && ctest --test-dir work/host --output-on-failure

# 3. Gerar um port novo a partir de um APK (base .so)
python3 tools/find_reference.py --engine Unity --abi arm64-v8a --renderer GLES2
bash nextos_ports_android-master/tools/new-port.sh ~/meujogo.apk meujogo   # arm64
bash nextos_ports_android-master/tools/new-port-arm.sh ~/meujogo.apk meujogo  # ARMv7

# 4. Compilar e auditar
bash nextos_ports_android-master/tools/build-port.sh ports/meujogo gles2
python3 tools/audit_elf.py ports/meujogo/build/meujogo
```

## Fluxo de port (resumo)

1. **Inventário** read-only do APK → 2. **Referência por contrato** (`catalog/profiles.json`) →
3. **Scaffold** (`new-port*.sh` gera `imports.gen.c` com UNKNOWNs classificados) →
4. **Wire imports/shims** (pthread, EGL/OpenSLES, fake JNI) → 5. **Ordem: relocações → construtores →
JNI_OnLoad → create → frames → áudio → input → saves** → 6. **Auditar ELF** (glibc ≤ 2.30) →
7. **Testar e provar no aparelho** → 8. **Empacotar** (`nxrelease bundle`).

📖 Documentação completa: [docs/](docs/INDEX.md)

## Estrutura

```
AI-Port-Arm/
├── nextos-framework/          # suíte V5 (framework/, catalog/, docs/, examples/, ports/…)
├── nextos_ports_android-master/  # base de ports .so (core/, template/, ports/, tools/…)
├── docs/                      # documentação da IA: guias, receitas, aprendizados
│   ├── INDEX.md  EXCLUSOES.md  GUIA-DE-PORT.md  ARQUITETURA.md
│   ├── RECEITAS-MALI450.md    # armas que resolvem cada muro específico do Mali-450
│   ├── TROUBLESHOOTING.md     # sintoma → causa → solução (consolidado)
│   ├── APRENDIZADOS.md        # padrões e lições (sincronizado com o vault)
│   └── PORTS.md               # catálogo de ports e status
└── .gitignore                 # regras 3 e 4 (segredos e binários)
```

## Repos do ecossistema Gatito

| Repos | Papel |
|---|---|
| [Gatito-Ports](https://github.com/OlhaGatito/Gatito-Ports) | Catálogo colaborativo de ports + ferramentas Gatito |
| [AI-Workspace](https://github.com/OlhaGatito/AI-Workspace) | Hub do ambiente de IA (setup, skills, MCP, fluxos) |
| [Guia-compilacao-By-NextOS-lembrancas](https://github.com/OlhaGatito/Guia-compilacao-By-NextOS-lembrancas) | Base de conhecimento prática de compilação/porting |
| [Lembra-e-Aprendizado-com-IA](https://github.com/OlhaGatito/Lembra-e-Aprendizado-com-IA) | Memória técnica por projeto (privado) |

## Licenças

- `nextos-framework/` — **GPL-3.0** (ver `nextos-framework/LICENSE` e `LICENSING.md`: casos especiais por port)
- `nextos_ports_android-master/` — **Apache-2.0** (ver `nextos_ports_android-master/LICENSE` e `NOTICE`)
- Documentação em `docs/` — o mesmo regime do framework (GPL-3.0), crédito: **Gatito / NextOS**

> Framework original por **NextOS**; dump organizado pela IA do projeto Gatito em 2026-10-21.
