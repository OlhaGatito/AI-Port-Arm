import io

p = 'port/src/jni.c'
d = io.open(p, encoding='utf-8', newline='').read()

reps = [
    (u'run39h，注册这个差别花了七个 repro',
     u'run39h: registrar essa diferenca custou sete repros'),
    (u'        FP2_THROW_TAIL);',
     u'        FP2_W_TAIL(xthrow));'),
]

for old, new in reps:
    n = d.count(old)
    assert n == 1, (n, old[:50])
    d = d.replace(old, new)

io.open(p, 'w', encoding='utf-8', newline='').write(d)
print('ok')
