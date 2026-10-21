import io

d = io.open('port/src/jni.c', encoding='utf-8', newline='').read()

old = 'FP2_TRAMP(load);'
new = 'FP2_TRAMP(load);\nFP2_TRAMP(probe7);'
assert d.count(old) == 1, d.count(old)
d = d.replace(old, new)

old2 = 'FP2_W_STUB(load, 4, FP2_W_Load)'
new2 = ('FP2_W_STUB(load, 4, FP2_W_Load)\n'
        'FP2_W_STUB(probe7, 4, FP2_W_Load)')
assert d.count(old2) == 1, d.count(old2)
d = d.replace(old2, new2)

io.open('port/src/jni_probe.c', 'w', encoding='utf-8', newline='').write(d)
print('written port/src/jni_probe.c')
