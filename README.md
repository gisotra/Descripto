# Torneio de Criptografia — interface gráfica

Interface em Electron para os 4 algoritmos em C: César, Vigenère, Atbash e XOR.
Funciona só pelo teclado (e também pelo mouse).

## Como rodar

Pré-requisitos (uma vez só):

```bash
# Linux Mint / Ubuntu
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

**Windows:** precisa do Node.js e de um `gcc` no PATH (MinGW-w64, por exemplo pelo MSYS2).
O programa foi escrito para funcionar lá (pasta Downloads, discos, modo binário), mas
**ainda não foi testado no Windows**.

## Atalhos

| Onde | Tecla | O que faz |
|---|---|---|
| Qualquer tela | `Ctrl+1` / `Ctrl+2` / `Ctrl+3` | Descriptografar / Criptografar / Códigos |
| Qualquer tela | `Tab` / `Shift+Tab` | próximo / anterior campo |
| Descriptografar | `↑ ↓` `PgUp PgDn` `Home End` | escolher na lista de arquivos |
| Descriptografar | `→` ou `Enter` | abrir a pasta |
| Descriptografar | `←` ou `Backspace` | voltar uma pasta |
| Descriptografar | **`Espaço`** | **carregar o .txt e descriptografar na hora** |
| Descriptografar | `Ctrl+D` / `Ctrl+L` | ir para Downloads / para a lista de lugares (discos no Windows) |
| Descriptografar | `Ctrl+C` | copiar a resposta para o professor |
| Criptografar | `Enter` ou `Ctrl+Enter` | criptografar |
| Criptografar | `T` / `S` / `R` | trocar de criptografia / segunda criptografia / começar de novo |
| Qualquer tela | `Esc` | volta o foco para o campo principal |

## Descriptografar

1. A lista abre na pasta **Downloads** (no Mint pode se chamar "Transferências"), com o `.txt`
   mais recente já selecionado. Aperte `Espaço`.
2. Em poucos milissegundos a tela mostra:
   - o **algoritmo ideal**, inclusive quando são **duas cifras** (selo "Criptografia dupla identificada");
   - a tabela **cifras e chaves na ordem em que foram aplicadas**, e os valores da chave em decimal, hexadecimal e binário;
   - a **verificação**: o programa cifra o texto claro com essas cifras e chaves e confere se dá exatamente o arquivo;
   - a **resposta para o professor** (texto claro, cifras na ordem e chaves), pronta para copiar;
   - as outras possibilidades testadas.

O modo automático testa XOR (1 a 5 bytes, todas as combinações) como última cifra, e Atbash,
César e Vigenère (1 a 8 letras), incluindo as combinações de duas cifras diferentes. Cada
possibilidade recebe uma pontuação de "parece português" (trigramas de letras + palavras
comuns) e vence a de maior pontuação. Como algumas cadeias são equivalentes (por exemplo
César + Atbash = Atbash + César com outra chave), a resposta pode vir na ordem equivalente:
a verificação garante que ela reproduz o arquivo.

O **modo manual** (embaixo da lista) permite colar o texto e escolher o algoritmo e a chave;
com a chave vazia ele testa todas as possibilidades daquela cifra.

## Criptografar

Uma cifra por vez, sempre por escolha sua. Depois da primeira cifra o programa pergunta:

- **Trocar de criptografia** (`T`): desfaz a cifra e volta para escolher outra;
- **Aplicar uma SEGUNDA criptografia** (`S`): mantém a primeira e cifra o resultado de novo
  (só nas fases 3 e 4; a segunda cifra precisa ser diferente e o XOR só pode ser a última).

A tela confere as regras do regulamento: orçamento de bits da fase (XOR 8 bits por byte, XOR de
2 bits só na fase 1, Vigenère e César 4,7 bits, Atbash 0), Vigenère com no máximo 8 letras,
César de 1 a 25 e texto claro de 200 a 250 letras a-z. No fim gera a **ficha de entrega**
(texto claro, cifras na ordem, chaves) e salva o texto cifrado em Downloads, em UTF-8 e sem
quebra de linha final.

## Estrutura

```
algoritmos/         os 4 códigos originais, SEM modificação (mostrados na tela "Códigos")
c/motor.c           programa em C que a interface executa
c/modelo_pt.h       tabela de trigramas de letras do português (gerada de texto da Wikipédia e de Dom Casmurro)
scripts/build-c.js  compila o motor com o gcc
scripts/preparar.js instala as dependências na primeira execução
main.js             processo principal do Electron (janela, pastas, arquivos, chama o motor)
preload.js          ponte segura entre a tela e o main.js
src/                tela (HTML, CSS e JS)
```

## Por que existe o `motor.c`

Os originais têm o texto e a chave fixos dentro do `main()`
(ex.: `char mensagem[] = "abcxyz"; int senha = 3;`), então não há como a interface passar
dados para eles sem editá-los. O `motor.c` usa as mesmas fórmulas, mas recebe o texto pela
entrada padrão e a chave pela linha de comando:

```bash
echo -n "khoor" | bin/motor cesar decifrar 3
echo -n "aceace" | bin/motor vigenere decifrar abc
bin/motor auto decifrar < desafio.txt          # testa tudo e descreve as cifras e chaves
```

## O erro do `xor.c` original

1. Depois da primeira volta do `for`, o arquivo já está no fim (EOF) e nunca
   volta ao início (`rewind`), por isso só o "Deslocamento 1" mostra o texto.
2. O `do { ... } while (c != EOF)` imprime o caractere do EOF antes de testar.
3. Pelo regulamento, o arquivo é UTF-8: cada byte acima de 127 ocupa 2 bytes,
   e o `fgetc` lê esses 2 bytes como se fossem 2 caracteres.

O `motor.c` lê o arquivo inteiro uma vez, converte de UTF-8 para os códigos 0-255
e corrige os três pontos. O `xor.c` original continua igual.

## Limites conhecidos

- A escolha da chave é estatística: com textos curtos pode errar. Nos testes (cerca de 2000
  desafios com textos de 175 a 230 letras) o texto voltou idêntico em mais de 99,8%.
- Se o último byte **cifrado** do XOR for `0x0A` (quebra de linha), o regulamento manda ignorá-lo
  e a última letra se perde; a tela avisa qual seria a letra.
- O Vigenère e as combinações só são descobertos quando o texto (antes do XOR) tem apenas letras a-z.
- O modelo de português foi treinado em textos gerais; palavras muito raras ou em outros
  idiomas pesam menos, mas não impedem a descoberta da chave (testado com *waffle*, *yakisoba*, *kiwi*).

## Observação sobre o `--no-sandbox`

O `npm run electron` abre o Electron com `--no-sandbox`. Em distribuições
baseadas no Ubuntu 24.04 (como o Mint 22), o Electron instalado pelo npm pode não
abrir sem isso por causa do sandbox do Chromium. A aplicação só carrega arquivos
locais dela mesma.
