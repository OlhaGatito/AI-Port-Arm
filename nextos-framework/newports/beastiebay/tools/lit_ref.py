#!/usr/bin/env python
""" Acha引用 de um literal de string do IL2CPP dentro do corpo do codigo.

Por que重新写 lit_find.py: aquela ferramenta procurava `movz #<indice>` com
uma suposicao que o build v24 nao cumpre -- o indice do literal pode nao
aparecer como imediato.  Aqui a busca e' mais burra e mais confiavel: varre
TODO o segmento de codigo procurando o par de bytes de um `movz <Wd>, #imm16`
para cada imm16 alvo, em todos os 32 registradores.

Por que isso importa agora: a mensagem de erro e' montada em dois lugares
distintos do metadata ("An error has occurred." = #4390, "\\n100" = #6026).
Achar ONDE o codigo carrega o primeiro literal leva direto ao sitio que
formata a mensagem de erro -- que e' o sitio que decidió dar errado.

Codificacao de MOVZ 32-bit (ARMv8):
    sf=0 opc=10 100101 hw=00 imm16 Rd(5)      ->  0x52800000 | imm16<<5 | Rd
    MOVZ 64-bit:                                ->  0xD2800000 | imm16<<5 | Rd
Imm16 e' o literal INDEX, nao o ponteiro -- o runtime resolve o indice para
o ponteiro do objeto string em tempo de execucao.
"""

import struct
import sys

MOVZ_W = 0x52800000
MOVZ_X = 0xD28000000
REGS = ["w%d" % i for i in range(31)] + ["x%d" % i for i in range(31)]


def find_all(data, imm):
    hits = []
    for width, base, name in ((MOVZ_W, 4, "movz w"), (MOVZ_X, 8, "movz x")):
        lo, hi = (0, 1 << 11) if width == MOVZ_W else (0, 1 << 16)
        for rd in range(32):
            enc = base | (imm << 5) | rd
            pat = struct.pack("<I", enc)
            start = 0
            while True:
                p = data.find(pat, start)
                if p < 0:
                    break
                hits.append((p, "%s%d, #0x%x" % (name, rd, imm)))
                start = p + 1
    return sorted(hits)


if __name__ == "__main__":
    path = sys.argv[1]
    imms = [int(x, 0) for x in sys.argv[2:]]
    data = open(path, "rb").read()
    print("file: %s (%d bytes)" % (path, len(data)))
    for imm in imms:
        hits = find_all(data, imm)
        print("\n=== literal #%d (0x%X): %d hit(s) ===" % (imm, imm, len(hits)))
        for off, txt in hits:
            print("   file_off 0x%08x   %s" % (off, txt))
        if not hits:
            print("   (nenhuma referencia por imediato -- o indice deve estar")
            print("    numa tabela de uso estatico, referenciada por adrp/ldr)")
