# docs — INDEX (documentação do AI-Port-Arm)

Documentação aprofundada do ecossistema de porting Gatito/NextOS. Tudo o que a IA aprendeu
está sincronizado a partir do vault `Gatito Projeto IA`.

| Doc | Conteúdo |
|---|---|
| [ARQUITETURA.md](ARQUITETURA.md) | As duas trilhas do framework (Suíte V5 `nx*` e base `.so`), módulos e contratos |
| [GUIA-DE-PORT.md](GUIA-DE-PORT.md) | Fluxo passo a passo completo (da escolha do APK à prova física) |
| [RECEITAS-MALI450.md](RECEITAS-MALI450.md) | Receitas específicas de GPU/hardware (Mali-450, GLES2, kernel 3.14) |
| [TROUBLESHOOTING.md](TROUBLESHOOTING.md) | Sintoma → causa → solução (tabela consolidada de todos os muros) |
| [APRENDIZADOS.md](APRENDIZADOS.md) | Padrões, boas práticas e erros recorrentes + lições |
| [PORTS.md](PORTS.md) | Catálogo dos ports: status, método e referências |
| [EXCLUSOES.md](EXCLUSOES.md) | O que não está neste repo (copyright/binários) e como obter |

## Guia rápido de escolha

- **Quero entender o framework** → [ARQUITETURA.md](ARQUITETURA.md) e `nextos-framework/framework/README.md`
- **Quero começar um port** → [GUIA-DE-PORT.md](GUIA-DE-PORT.md)
- **Deu crash/erro** → [TROUBLESHOOTING.md](TROUBLESHOOTING.md)
- **Mali-450 mebdeu** → [RECEITAS-MALI450.md](RECEITAS-MALI450.md)
- **Quero saber o que existe pronto** → [PORTS.md](PORTS.md) e `nextos-framework/catalog/ports.json`
