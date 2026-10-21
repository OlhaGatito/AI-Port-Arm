import io

p = 'port/src/jni.c'
d = io.open(p, encoding='utf-8', newline='').read()

reps = [
    (u"A troca que este despejo faz e'\uff1a\u300co jogo lancou excecao em algum ponto\n"
     u' * de Init\u300d -> \u300co campo no offset +0x78 esta NULL\u300d.  O primeiro nao aponta\n'
     u' * para nada; o segundo aponta direto para a construcao que falhou.',
     u'A troca que este despejo faz e\': o jogo lancou excecao em algum ponto de\n'
     u' * Init vira o campo no offset +0x78 estar NULL.  O primeiro nao aponta\n'
     u' * para nada; o segundo aponta direto para a construcao que falhou.'),
    (u'/* S\u00f3 o primeiro evento: \u5b57\u6bb5\u4f1a\u968f\u65f6\u95f4\u53d8\u5316, \u800c\u6211\u4eec\u8981\u7684\n'
     u'     * e\' o estado no instante em que o metodo foi chamado. */',
     u'/* S\u00f3 o primeiro evento: os campos mudam com o tempo, e o que\n'
     u'     * interessa e\' o estado no instante em que o metodo foi chamado. */'),
]

for old, new in reps:
    n = d.count(old)
    assert n == 1, (n, old[:60])
    d = d.replace(old, new)

io.open(p, 'w', encoding='utf-8', newline='').write(d)
print('ok')
