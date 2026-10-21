#!/usr/bin/env python
"""Sobe um arquivo para o aparelho pelo CANAL DE EXEC, em base64.

Por que existe: o SFTP do ArkOS (`bb_ssh.py put`) falha com ENOENT ao
criar QUALQUER arquivo, mesmo em `/tmp` que `stat` responde e que o shell
`touch` cria sem problema.  Nao e' permissao (o diretorio e' 1777) nem
caminho (o mesmo `/tmp/bb_newbin` funciona quando e' o destino padrao) --
e' o subsistema sftp do aparelho, que so abre arquivo que ele proprio
criou.  Nao vale investigar: o exec channel funciona de forma confiavel.

Metodo: base64 no caminho local, `base64 -d` no aparelho.  Escolhido em
vez de heredoc porque o shell do aparelho e'ash e o conteudo pode ter
aspas, crases e `$` que o quoting local comecaria a interpretar.

Uso:
    python put_b64.py <local> <remote>
    python put_b64.py tools/r39g.sh /tmp/r39g.sh
"""

import base64
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bb_ssh  # noqa: E402

# Abaixo de uma linha de comando.  exec_command manda a linha inteira de
# uma vez; um payload de centenas de KB estouraria o limite do canal.
CHUNK = 900


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    local, remote = sys.argv[1], sys.argv[2]

    with open(local, "rb") as f:
        raw = f.read()
    b64 = base64.b64encode(raw).decode()

    c = bb_ssh.connect()
    try:
        out, err, code = bb_ssh.run(c, "rm -f %s" % remote)
        if code != 0:
            print("[put_b64]rm falhou: %s" % err.strip(), file=sys.stderr)
            return code

        n = 0
        for i in range(0, len(b64), CHUNK):
            piece = b64[i:i + CHUNK]
            op = ">" if i == 0 else ">>"
            out, err, code = bb_ssh.run(
                c, "printf '%%s' '%s' %s %s.b64" % (piece, op, remote))
            if code != 0:
                print("[put_b64]chunk %d falhou: %s" % (n, err.strip()),
                      file=sys.stderr)
                return code
            n += 1

        out, err, code = bb_ssh.run(
            c, "base64 -d %s.b64 > %s && rm -f %s.b64 && md5sum %s && wc -c %s"
               % (remote, remote, remote, remote, remote))
        if code != 0:
            print("[put_b64]decode falhou: %s" % err.strip(), file=sys.stderr)
            return code
        sys.stdout.write(out)
        sys.stderr.write("[put_b64]%s -> %s em %d chunks\n"
                         % (local, remote, n))
    finally:
        c.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
