#!/bin/sh
# run39g: capture the FIELD DUMP of BootForm at the instant Init is called.
#
# What run39f proved: the offset-less field list named 13 fields, all NULL,
# and -- decisively -- the three slots the offline read of the generated
# BootForm::Init tests with `cbnz` (+0x70, +0x78, +0x80) were NOT among
# them.  They are inherited.  run39g climbs the base-class chain so those
# three finally get a name and a value.
#
# The payload is small (one dump, at the FIRST hit only), so this script
# drops the /proc/<pid>/mem business entirely and just keeps polling until
# the `campo:` header shows up.  Timing lesson from r39d still applies:
# the game stops its render loop at frame 6 and exits within seconds, so
# poll for the marker instead of sleeping a fixed amount.
#
# CRLF WARNING (learned in r39d): a \r in this file becomes part of a token
# on the device.  Keep it LF-only.
G=/roms/ports/beastiebay
L=$G/log.txt

echo "=== deploy + launch ==="
if [ -f /tmp/bb_newbin ]; then
  # Guarda: /tmp/bb_newbin e' o destino PADRAO do upload do bb_ssh.py, e um
  # teste de upload de script (r39g.sh) para a MESMA janela sobrescreve o
  # binario do jogo com um arquivo de shell.  O jogo dai' nao sobe e o
  # sintoma e' "no process after 13s", que parece falha do hook e nao e'.
  # Checar o magic ELF antes de instalar evita perder um ciclo de build.
  if head -c 4 /tmp/bb_newbin | od -An -c | tr -d ' \n' | grep -q '^177ELF$'; then
    cp /tmp/bb_newbin $G/beastiebay-nextos.new
    chmod +x $G/beastiebay-nextos.new
    mv -f $G/beastiebay-nextos.new $G/beastiebay-nextos
  else
    echo "ABORT: /tmp/bb_newbin nao comeca com \\177ELF -- parece script, nao binario"
    rm -f /tmp/bb_newbin
    exit 1
  fi
fi
md5sum $G/beastiebay-nextos
: > $L
FP2_DIALOG_TRACE=1 sh /tmp/bb_launch.sh

i=0
PID=""
HAVE=0
while [ $i -lt 60 ]; do
  i=$((i + 1))
  sleep 1
  PID=$(ps -eo pid,args 2>/dev/null | grep beastiebay-nextos | grep -v grep \
        | cut -d' ' -f1 | head -1)
  if grep -q 'campo: ----' $L 2>/dev/null; then
    HAVE=1
    break
  fi
  # NUNCA sair por "processo ausente" no primeiro segundo.  `LAUNCHED pid=`
  # e' o pid do setsid, e o jogo real e' filho dele -- nos primeiros
  # instantes `ps` pode ainda nao lista-lo.  A prova de vida que importa e'
  # o dump ja ter aparecido no log; enquanto nao aparecer, continuar
  # esperando e' mais honesto do que declarar "GAME ALREADY GONE".
  if [ -z "$PID" ] && [ $i -gt 12 ]; then
    echo "--- no process after ${i}s and still no dump"
    break
  fi
done

echo
echo "=== waited ${i}s  pid=${PID:-<none>}  dump_seen=${HAVE} ==="

echo
echo "=== narrated events ==="
grep -h 'state:' $L 2>/dev/null | head -20
echo "--- state lines: $(grep -c 'state:' $L 2>/dev/null) ---"

echo
echo "=== FULL FIELD DUMP ==="
if [ "$HAVE" = 1 ]; then
  sed -n '/campo: ----/,/^state: #2/p' $L 2>/dev/null | head -80
else
  echo "(no dump header -- see the events above)"
fi

echo
echo "=== reflection capability ==="
grep -h 'il2:' $L 2>/dev/null | head -20

echo
echo "=== throw helper (run39h) ==="
grep -h 'throw:' $L 2>/dev/null

echo
echo "=== the three slots the offline read cares about ==="
grep -h 'this+0x70\|this+0x78\|this+0x80\|this+0xc0\|this+0x10 ' $L 2>/dev/null

echo
echo "=== dialog / error text ==="
grep -h -i 'dialog\|erro\|error\|100' $L 2>/dev/null | head -20

for p in $(ps -eo pid,args 2>/dev/null | grep beastiebay-nextos | grep -v grep \
          | cut -d' ' -f1); do
  kill -9 $p 2>/dev/null
done
echo R39G_DONE
