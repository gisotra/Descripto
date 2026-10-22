# Torneio de Criptografia — interface gráfica

Interface em Electron para os 4 algoritmos em C: César, Vigenère, Atbash e XOR.

## Como rodar (Linux Mint)

Pré-requisitos (uma vez só):

```bash
sudo apt install build-essential nodejs npm   # gcc + Node.js 18 ou mais novo
git clone https://github.com/gisotra/Descripto.git
cd Descripto
```

Em toda iteração:

```bash
git pull
npm run electron
```

O `npm run electron` instala o Electron sozinho na primeira vez, compila
`c/motor.c` com o gcc (gera `bin/motor`) e abre a janela.

## Estrutura

```
algoritmos/      os 4 códigos originais, SEM modificação (mostrados na tela "Códigos")
c/motor.c        programa em C que a interface executa
scripts/build-c.js  compila o motor com o gcc
main.js          processo principal do Electron (janela, arquivos, chama o motor)
preload.js       ponte segura entre a tela e o main.js
src/             tela (HTML, CSS e JS)
```

## Por que existe o `motor.c`

Os originais têm o texto e a chave fixos dentro do `main()`
(ex.: `char mensagem[] = "abcxyz"; int senha = 3;`), então não há como a
interface passar dados para eles sem editá-los. O `motor.c` usa as mesmas
fórmulas, mas recebe o texto pela entrada padrão e a chave pela linha de comando:

```bash
echo -n "khoor" | bin/motor cesar decifrar 3
echo -n "aceace" | bin/motor vigenere decifrar abc
bin/motor xor decifrar "" < desafio.txt          # busca automática da chave
```

## O erro do `xor.c` original

1. Depois da primeira volta do `for`, o arquivo já está no fim (EOF) e nunca
   volta ao início (`rewind`), por isso só o "Deslocamento 1" mostra o texto.
2. O `do { ... } while (c != EOF)` imprime o caractere do EOF antes de testar.
3. Pelo regulamento, o arquivo é UTF-8: cada byte acima de 127 ocupa 2 bytes,
   e o `fgetc` lê esses 2 bytes como se fossem 2 caracteres.

O `motor.c` lê o arquivo inteiro uma vez, converte de UTF-8 para os códigos 0-255
e corrige os três pontos. O `xor.c` original continua igual.

## Uso da tela

**Caminho mais simples:** clique em **Adicionar arquivo .txt** (ou **Abrir .txt no campo**).
A análise começa sozinha e a tela mostra o **algoritmo ideal**, a chave (em decimal,
hexadecimal e binário), o texto decifrado e as outras possibilidades testadas.
Leva em torno de 0,1 s (testado: 30 a 220 ms para textos de 250 letras; 0,4 s para 2000 letras).

O modo **Automático** testa, em todo arquivo:
- XOR (chaves de 1 a 5 bytes, todas as combinações) como última cifra;
- Atbash, César e Vigenère (chaves de 1 a 8 letras, os 26 deslocamentos de cada letra);
- as combinações de duas cifras diferentes (XOR sempre por último).

Cada possibilidade recebe uma pontuação de "parece português" (frequência das letras +
palavras comuns) e vence a de maior pontuação; se uma cifra mais simples chega perto,
ela é preferida. Como Atbash + César equivale a César + Atbash com outra chave, o
resultado pode vir na ordem equivalente (a cifra e a chave informadas reproduzem o
mesmo texto cifrado).

Os botões César, Vigenère, Atbash e XOR continuam disponíveis:
- **Chave vazia**: testa todas as possibilidades daquela cifra e devolve a chave.
- **Chave informada**: César `1` a `25`; Vigenère só letras; XOR bytes `42`, `0x2a` ou `12, 200, 7`.
- **Usar como entrada**: joga o resultado no campo de texto para aplicar outra cifra.
- **Salvar .txt**: grava o resultado em UTF-8, no formato do regulamento.

## Limites conhecidos

- A escolha da chave é estatística: com textos curtos e chaves de Vigenère de 8 letras
  pode errar. Nos testes (1150 desafios, textos de 175 a 200 letras) 99,8% voltaram idênticos.
- Se o último byte **cifrado** do XOR for 0x0A (quebra de linha), o regulamento manda
  ignorá-lo e a última letra se perde; a tela avisa qual seria a letra.
- O Vigenère só é descoberto quando o texto tem apenas letras a-z.

## Observação sobre o `--no-sandbox`

O `npm run electron` abre o Electron com `--no-sandbox`. Em distribuições
baseadas no Ubuntu 24.04 (como o Mint 22), o Electron instalado pelo npm pode não
abrir sem isso por causa do sandbox do Chromium. A aplicação só carrega arquivos
locais dela mesma.
