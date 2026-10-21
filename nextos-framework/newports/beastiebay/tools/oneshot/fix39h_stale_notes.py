# -*- coding: utf-8 -*-
"""Remove o bloco de notas antigo do stub de excecao (run39h).

O bloco 2355..2409 defendia a teoria de que o slot indefinido dependia do
"contexto do conjunto de stubs" e de "declaracao num bloco contiguo".
Isso foi medido e DESMENTIDO: a causa e' a ausencia de
__attribute__((used)).  Deixar a nota antiga ali, ao lado da nota nova,
faria o proximo leitor contenders duas conclusoes -- uma certa e uma falsa.

Remove pelo indice de linha (1-based, inclusivo).
"""
import io
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PATH = os.path.join(os.path.dirname(HERE), "port", "src", "jni.c")

LO = 2355
HI = 2409

with io.open(PATH, encoding="utf-8", newline="") as f:
    lines = f.readlines()

if not (1 <= LO <= HI <= len(lines)):
    sys.exit("aborted: file has %d lines, asked %d..%d" % (len(lines), LO, HI))

head = lines[LO - 1]
tail = lines[HI]
if not head.startswith("/* O stub de excecao usa EXATAMENTE"):
    sys.exit("aborted: line %d does not start the old block: %r" % (LO, head[:60]))
if not tail.startswith("/* Stub do auxiliar de excecao"):
    sys.exit("aborted: line %d is not the new block: %r" % (HI + 1, tail[:60]))

kept = lines[: LO - 1] + lines[HI:]
with io.open(PATH, "w", encoding="utf-8", newline="") as f:
    f.write("".join(kept))

print("removed %d lines; file now %d" % (HI - LO + 1, len(kept)))
