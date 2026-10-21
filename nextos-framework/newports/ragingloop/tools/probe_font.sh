ls -la /storage/roms/ports/PortMaster/resources/*.ttf /storage/roms/ports/PortMaster/resources/*.otf 2>/dev/null
echo "== sfnt tag (4 bytes) =="
for f in /storage/roms/ports/PortMaster/resources/NotoSansSC-Regular.ttf; do echo -n "$f: "; od -An -tx1 -N4 "$f"; done
echo "== tables =="
python3 - <<PY
import struct,sys
p="/storage/roms/ports/PortMaster/resources/NotoSansSC-Regular.ttf"
d=open(p,"rb").read(1<<16)
tag=struct.unpack(">I",d[:4])[0]
print("sfnt version 00000000"0x0p+0g)
n=struct.unpack(">H",d[4:6])[0]
print("num tables",n)
for i in range(n):
    off=12+16*i
    name=d[off:off+4].decode("latin1")
    print("  ",name)
PY
