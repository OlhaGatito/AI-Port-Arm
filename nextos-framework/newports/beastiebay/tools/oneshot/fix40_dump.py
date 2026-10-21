# -*- coding: utf-8 -*-
"""run40: make the throw-time dump show values and the class's method table.

run39i named the object (kairo.unity.io.RecordStore) and the empty field
(+0x18 storage_).  What it did not answer is WHO assigns storage_ -- and that
is a question about code, not data.  The method table of the real class turns
"some method" into a list of file offsets that disasm_at.py can read offline,
cheaply and without guessing which method matters.

Two edits:
  1. fp2_dump_fields_cb prints the pointer instead of "ok", and lists static
     fields rather than dropping them.  "ok" answered "is it null" and threw
     away the address, which is the one thing that lets a later run compare
     two objects.
  2. fp2_dump_bootform_fields also walks the method table.
"""
import io
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PATH = os.path.join(os.path.dirname(os.path.dirname(HERE)),
                    "port", "src", "jni.c")

s = io.open(PATH, encoding="utf-8", newline="").read()

OLD_CB = """    (void)f;
    (void)type_kind;
    if (is_static)
        return;
    /* So os slots que a leitura offline mostrou serem testados por cbnz:
     * +0x70, +0x78, +0x80 e a cadeia +0xc0/+0x10. _dumpar o resto so
     * encheria o log de linhas sem informacao. */
    if (offset < 0 || offset > 0x100)
        return;
    nx_log("campo:  this+0x%02x  %-24s %s", offset, name,
           value ? "ok" : "NULL  <<<");
}"""

NEW_CB = """    (void)f;
    (void)type_kind;
    /* run40: um campo estatico nao mora no objeto, entao nao ha valor a
     * ler -- mas o NOME ainda e' informacao.  `recordStores_`,
     * `rootStorage_` e `usePreference_` sao os tres que dizem se o backend
     * de gravacao tem estado global, e sem eles a lista de instancia parece
     * um objeto que nunca foi inicializado em vez de um objeto cuja
     * inicializacao falhou. */
    if (is_static) {
        nx_log("campo:  static     %-24s (slot 0x%x)", name,
               offset < 0 ? 0u : (unsigned)offset);
        return;
    }
    if (offset < 0 || offset > 0x100)
        return;
    /* run40: imprimir o ponteiro, nao "ok".  "ok" responde "e' nulo?" e
     * joga fora o endereco -- que e' justamente o que permite comparar este
     * objeto com o proximo e saber se e' o mesmo. */
    nx_log("campo:  this+0x%02x  %-24s %p%s", offset, name, value,
           value ? "" : "  <<< NULL");
}"""

assert OLD_CB in s, "field callback anchor not found"
s = s.replace(OLD_CB, NEW_CB, 1)

OLD_TAIL = """    nx_log("campo: ---- %s @ %p ----", il2_class_label(k), self);
    il2_fields_foreach(k, fp2_dump_fields_cb, self);
}"""

NEW_TAIL = """    nx_log("campo: ---- %s @ %p ----", il2_class_label(k), self);
    il2_fields_foreach(k, fp2_dump_fields_cb, self);

    /* run40: os campos dizem O QUE esta vazio; os metodos dizem QUEM deveria
     * ter preenchido.  `storage_` e' NULL e nenhuma leitura offline vai
     * nomear o metodo que o atribui, porque o nome do metodo mora no
     * metadado e o endereco mora na tabela de ponteiros -- nm nao tem um sem
     * o outro.  A API de reflexao tem os dois, e o custo e' uma varredura
     * que ja foi validada no run33.  Cada `+0x...` abaixo e' um offset de
     * arquivo que disasm_at.py le direto. */
    struct fp2_method_scan scan = { il2_class_label(k), 0 };
    il2_methods_foreach(k, fp2_method_scan_cb, &scan);
}"""

assert OLD_TAIL in s, "dump tail anchor not found"
s = s.replace(OLD_TAIL, NEW_TAIL, 1)

io.open(PATH, "w", encoding="utf-8", newline="").write(s)
print("ok: jni.c patched for run40")
