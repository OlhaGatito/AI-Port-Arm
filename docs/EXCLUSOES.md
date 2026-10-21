# EXCLUSOES — o que não foi publicado e como obter

O AI-Port-Arm publica **código-fonte do framework e documentação**. Por regra de licença
e tamanho, o conteúdo abaixo **não está no repositório** e deve ser obtido localmente.

## 1. Conteúdo protegido por copyright (nunca publicado)

| Item | Origem | Como obter |
|------|--------|------------|
| `ShadowFight2.apk` (raiz) | Jogo comercial Shadow Fight 2 (Android) | Compra pelo Google Play a partir do seu próprio backup |
| `sf2-analysis/**` (arquivos `lib*.so`: libil2cpp, libunity, libFirebase*, libgpg, libmain) | Bibliotecas extraídas do APK do jogo | Extração do APK legítimo (unzip do APK) |
| `ports/revc/libreVC.so`, `ports/revc/libc++_shared.so` | Binários do port Re-Volt Classic / NDK | Extraídos do APK legítimo do jogo |
| ROMs, BIOS, keys | Jogos de console | Nunca distribuir |

Nenhum port do ecossistema distribui APK/OBB/assets/BIOS/fontes proprietárias.
O usuário fornece os arquivos do seu próprio backup legal.

## 2. Código-fonte vendorizado excluído por tamanho (OSS — obter a fonte upstream)

| Item | Tamanho | Licença upstream | Como obter |
|------|---------|------------------|------------|
| `ports/ShadowFight2/sdl2-src/**` | ~83 MB | SDL2 (zlib) | Clone de https://github.com/libsdl-org/SDL na revisão usada pelo port |
| `nextos-framework/nextos-framework-master.zip` | ~30 MB | GPL-3 | Clonar `nextos-framework/` deste repositório (mesma versão) |

O SDL2 vendorado é necessário apenas para builds específicos do port ShadowFight2;
os outros ports usam as bibliotecas gerais do framework.

## 3. Diretórios duplicados/redundantes (não são obra nova)

| Item | Motivo |
|------|--------|
| `nextos_ports_android-master/sf2-reference/**` | Clone completo do próprio `nextos_ports_android-master` numa revisão anterior (contém `.git` aninhado) |
| `nextos_ports_android-master/ports/ShadowFight2/nextos-check/**` | Cópia de verificação (contém `.git` aninhado e binários `.so`) |
| `nextos_ports_android-master/ports/ShadowFight2/sdl2-src/.git` | Git aninhado (ver item 2) |

## 4. Arquivos-junk de shell (removidos)

Gerados por comandos shell com saída redirecionada ao nome do arquivo (ex.: `./cmd > Updating`):
``=``, `Attaching`, `Appending`, `bsa_flags`, `bsa_mask`, `bsa_restorer`,
`Disabled`, `Disabling`, `Generating`, `Importing`, `Locally`, `nosso`, `Updating`
(estavam na raiz de `nextos_ports_android-master` e `ports/ShadowFight2`).

## 5. Como o dump preserva dados

- Backup espelho pré-revisão: `C:\Users\Fabricio\Downloads\Backup\gatito-repos-backup-2026-10-21\`
- Estrutura lógica preservada: `framework/`, `scripts/` (tools/), `templates/`, `docs/`, `examples/`
- `.gitignore` na raiz impede que as exclusões voltem por acidente.
