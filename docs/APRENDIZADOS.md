# APRENDIZADOS — padrões, boas práticas e erros recorrentes

Sincronizado com o vault `Gatito Projeto IA` (sessão 2026-10-21). Lições que se aplicam a
QUALQUER port, independente de trilha (Suíte V5 ou base .so).

## Princípios de governança (do AGENTS.md do framework)
1. **Nunca commitar APK/OBB/assets/saves/BIOS/keys/dumps** — dados BYO do usuário.
2. **Referência imutável**: `framework/`, `ports/*/upstream/`, `SOURCE-MAP.json` não se modificam;
   port novo vive em `work/ports/<port-id>/` ou `ports/<nome>/`.
3. **Identidade se deriva, não se digita** — hashes/pins do manifesto; `nxledger` deriva de bits.
4. **Sucesso nunca fabricado**: import desconhecido → diagnóstico e stub explícito; vídeo preto
   invalida o release; backend `dummy`/`disk` não audifica áudio; "SSH puro não aprova release".
5. **Preferir AArch64** quando o input é `arm64-v8a`; ARMv7 exige fronteira softfp/ARMHF explícita.
6. **Uma fronteira por vez**; cada mudança isolada com teste dirigido do contrato.
7. Documentação bilíngue PT+EN em paridade verificável.
8. Aparelho só com endereço autorizado **nesta tarefa** (nunca descoberto por texto antigo).

## Método de sessão de port (o padrão que se provou)
- **Recon → classify → wire → shim → render → loop**: inventário, classificar UND, gerar `imports.gen.c`,
  implementar por categoria, iterar com env-flags e logs (`tee run.log`).
- **PC-first quando possível**: rodar o `.so` x86_64 no PC (gdb instantâneo) antes de lutar com Mali
  (experiments/bully).
- **Handoff documentado**: `RESUMO-PROXIMA-SESSAO.md` com estado, último comando, última linha de log,
  env vencedora, próximos ataques.
- **Sessões gravadas como evidência** (`script` do terminal + greps "todos"): auditável e retomável.
- **Evidência sanitizada**: dados da pessoas nunca vão para receipts/claims públicos (7 critérios de
  promoção de CFW-EVIDENCE).

## Erros recorrentes observados (com solução rápida)
| Erro recorrente | Solução | Referência |
|---|---|---|
| Reutilizar receita sem revalidar no aparelho | toda receita = **cadeia estrita de validação em ambiente controlado** + física | `docs/pt-BR/PROBLEMAS-COMUNS.md` |
| Classificar por nome/similaridade e não por contrato | usar `find_reference.py`/`profiles.json` | Guia Fase 2 |
| Dar sucesso parcial "som funciona mas tela preta" | política: invalidar vídeo, reprovar | Guia Fase 7 |
| Commit de artefatos de shell (junk files) | `.gitignore` + revisão de `git status` antes do commit | docs/EXCLUSOES.md §4 |
| Encoding misturado (mojibake) em docs | gravar sempre UTF-8 sem BOM; checar diff legível | regra geral |
| Estados de trabalho paralelos ficarem dessincronizados — sincronizar o que mudou desde a última sessão | regra: **backup local → diff de estado → attest** antes de qualquer refactor | AGENTS.md |

## Quando NÃO fazer
- Não "atualizar manifesto" para silenciar gate GLIBC.
- Não reescrever gates de ports aprovados.
- Não anunciar suporte sem evidência na combinação publicada (device/CFW/runtime).
- Não usar caminho de fonte upstream executando builds/SSH de texto antigo (evidência ≠ autorização).
