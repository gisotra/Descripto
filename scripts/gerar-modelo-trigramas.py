#!/usr/bin/env python3
"""Gera uma tabela de trigramas de letras (formato de c/modelo_pt.h) a partir de arquivos de texto.

Uso: python3 scripts/gerar-modelo-trigramas.py SAIDA.h NOME_DA_TABELA "descrição" arquivo1.txt [arquivo2.txt ...]

Os textos só servem para CONTAR trigramas de letras (sem acentos, sem espaços, a-z). Cada valor da
tabela é -ln P(letra | 2 letras anteriores) * 16, com suavização por recuo (bigrama e unigrama).
Textos do Project Gutenberg: só o miolo entre "*** START" e "*** END" é usado.
"""
import math
import re
import sys
import unicodedata

A2, A3 = 5.0, 2.0   # força do recuo para bigrama e trigrama


def letras(texto):
    texto = unicodedata.normalize('NFD', texto.lower())
    texto = ''.join(c for c in texto if unicodedata.category(c) != 'Mn')
    return re.sub('[^a-z]', '', texto)


def principal(saida, nome, descricao, arquivos):
    corpus = []
    for arquivo in arquivos:
        texto = open(arquivo, encoding='utf-8', errors='replace').read()
        if '*** START' in texto and '*** END' in texto:
            texto = texto[texto.index('*** START'):texto.index('*** END')]
        corpus.append(letras(texto))
    texto = ''.join(corpus)
    v = [ord(c) - 97 for c in texto]

    c1 = [0] * 26
    c2 = [[0] * 26 for _ in range(26)]
    c3 = [[[0] * 26 for _ in range(26)] for _ in range(26)]
    for i, a in enumerate(v):
        c1[a] += 1
        if i >= 1:
            c2[v[i - 1]][a] += 1
        if i >= 2:
            c3[v[i - 2]][v[i - 1]][a] += 1
    total = sum(c1)
    p1 = [(c1[c] + 1) / (total + 26) for c in range(26)]

    def p2(b, c):
        return (c2[b][c] + A2 * p1[c]) / (sum(c2[b]) + A2)

    def p3(a, b, c):
        return (c3[a][b][c] + A3 * p2(b, c)) / (sum(c3[a][b]) + A3)

    tabela = bytearray(26 ** 3)
    for a in range(26):
        for b in range(26):
            for c in range(26):
                tabela[a * 676 + b * 26 + c] = min(255, round(-math.log(p3(a, b, c)) * 16))

    linhas = [', '.join(str(x) for x in tabela[i:i + 24]) for i in range(0, len(tabela), 24)]
    with open(saida, 'w') as f:
        f.write(f'/* {descricao}\n   Valor = -ln P(letra | 2 letras anteriores) * 16. Texto sem espacos. '
                f'Treino: {len(texto)} letras. */\n')
        f.write(f'static const unsigned char {nome}[26 * 26 * 26] = {{\n' + ',\n'.join(linhas) + '\n};\n')
    print(f'{saida}: {len(texto)} letras de treino')


if __name__ == '__main__':
    if len(sys.argv) < 5:
        sys.exit(__doc__)
    principal(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:])
