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

**Windows:** só precisa do Node.js (https://nodejs.org). **Não precisa instalar o gcc**:
o `npm run electron` usa o `prebuilt/motor-windows-x64.exe`, que já vem no repositório
(compilado do mesmo `c/motor.c`; ele só é usado se o hash do código-fonte conferir, então
nunca roda uma versão desatualizada). Se o `gcc` (MinGW-w64) estiver no PATH, o programa
compila o `c/motor.c` normalmente. O `.exe` não é assinado: o Windows Defender / SmartScreen
pode reclamar na primeira execução.

Para quem altera o `c/motor.c`: gerar de novo o `.exe` (a partir do Linux, com o Zig) e o hash:

```bash
python3 -m pip install --target /tmp/zig ziglang
PYTHONPATH=/tmp/zig python3 -m ziglang cc -target x86_64-windows-gnu -O2 -std=gnu11 \
    -o prebuilt/motor-windows-x64.exe c/motor.c
node scripts/hash-fonte.js
```

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
   - o **algoritmo ideal**, inclusive quando são **duas, três ou quatro cifras** (selo "Criptografia múltipla identificada");
   - a tabela **cifras e chaves na ordem em que foram aplicadas**, e os valores da chave em decimal, hexadecimal e binário;
   - a **verificação**: o programa cifra o texto claro com essas cifras e chaves e confere se dá exatamente o arquivo;
   - a **resposta para o professor** (texto claro, cifras na ordem e chaves), pronta para copiar;
   - as outras possibilidades testadas.

### Como a descriptografia em camadas funciona

O programa **não assume uma cifra só**. Para o texto cifrado (ou para o que sobra depois de desfazer o XOR):

1. **Evidências estatísticas:** índice de coincidência (IC) do texto, IC médio das colunas para estimar o
   período da chave de Vigenère e média de trigramas de letras em português e em inglês.
2. **Hipóteses para a camada mais externa:** Atbash, os 25 deslocamentos de César e a chave de Vigenère
   que melhor ajusta cada coluna (de 2 até `--max-periodo` letras, com refinamento letra a letra).
3. **Avaliação do resultado intermediário:** cada hipótese desfaz uma camada e o texto resultante é avaliado.
   Uma busca em feixe (3 hipóteses por nível) repete o processo enquanto houver evidência de outra camada,
   até `--max-camadas` camadas.
4. **Parada só com texto natural:** um texto só é aceito se a média de trigramas passar de um **limiar
   calibrado por simulação** (depende do tamanho do texto). Letras que "parecem válidas" não bastam.
5. **Prova real:** a cadeia encontrada é aplicada de volta ao texto claro e tem de reproduzir exatamente o arquivo.

Se nada passa do limiar, o resultado é **inconclusivo**: o programa **não mostra texto algum como resultado**
(só a melhor tentativa, marcada como baixa confiança) e explica o motivo (texto curto, cifra não suportada,
chave maior que o limite, tempo esgotado).

**O que o texto cifrado prova, e o que não prova.** César e Vigenère somam deslocamentos (e o Atbash só inverte o
alfabeto), então qualquer sequência dessas cifras equivale a **uma só transformação**: um Vigenère de chave
composta, talvez seguido de Atbash. César(3) e depois Vigenère("abc"), Vigenère("abc") e depois César(3) e
Vigenère("def") dão **exatamente o mesmo texto cifrado**, e há 26 divisões equivalentes do deslocamento. Por isso o
programa mostra a **camada efetiva** (provada) e, quando há deslocamento em comum na chave, uma **decomposição
compatível em duas camadas** (César interna e Vigenère externa, por convenção), claramente marcada como não
identificável. O XOR, que não comuta com as outras, é identificado como a última camada.

O mesmo vale para o **Atbash**: `Atbash → Vigenère("abc")` e `Vigenère("azy") → Atbash` dão exatamente o mesmo texto
cifrado (a chave troca de sinal quando o Atbash muda de lado: cada letra vira 26 menos o valor dela). O programa
mostra **as duas formas, cada uma conferida**, e escolhe a principal pela chave que mais parece uma senha (4 letras ou
mais) ou, na dúvida, a de letras mais perto de "a". A resposta para o professor traz as duas.

### Três ou quatro cifras em sequência

César, Vigenère e Atbash, em qualquer quantidade e ordem, se fundem numa transformação só: **Vigenère de chave
efetiva, talvez com Atbash de um dos lados**. Exemplos: `Atbash → César → Vigenère`, `César → Vigenère → Atbash →
XOR`, `Vigenère("abc") → Vigenère("fghij") → César(4)`. O programa encontra essa transformação efetiva (e o XOR, que
fica por último) e mostra a forma mais simples que reproduz o arquivo, conferida pela prova real.

- **Duas Vigenère em sequência** somam as chaves: a chave efetiva tem o **mmc** dos tamanhos (3 e 5 letras dão 15).
  Por isso a busca vai até **24 letras** de chave efetiva. Ela procura também a forma estruturada `a[i mod 3] +
  b[i mod 5]`, que tem só 7 letras livres e é bem mais firme que uma chave livre de 15 letras. Quando a chave efetiva
  é a soma exata de duas chaves de até 8 letras, a tela mostra a **decomposição em duas Vigenère**. A ordem e a divisão
  do deslocamento entre elas não são identificáveis, e o texto da nota diz isso.
- **Duas fases:** primeiro a busca usa chave de até 8 letras (o regulamento, rápido). Só se nada for aceito pela
  média de trigramas ela tenta até 24.
- **Chave longa exige texto mais natural:** quanto menos letras por coluna (n ÷ tamanho da chave), mais a busca
  consegue "ajustar" uma chave a texto sem sentido. O limiar sobe com o tamanho da chave (calibrado como o limiar
  normal, com textos que nunca podem ser aceitos).
- **Chave de ajuste nunca vence a certa:** uma chave longa que é quase a repetição de uma menor, ou quase a soma de
  duas, é trocada pela forma simples **só se** essa também for aceita e perder no máximo 8 nats de trigramas por letra
  livre economizada. Uma letra de ajuste "compra" uns 4 a 5 nats; uma coluna errada custa dezenas.
- **Refinamento final:** a chave escolhida é refinada letra a letra e de duas em duas letras vizinhas (26 × 26), porque
  colunas vizinhas erradas podem formar trigramas plausíveis entre si.

### Textos com muitos nomes próprios ("decidido pela estrutura da cifra")

Nomes como Mamardashvili, Kvaratskhelia ou Szczesny derrubam a média de trigramas: o próprio texto claro de uma
escalação da seleção georgiana fica abaixo do limiar de texto natural. Nesse caso o resultado pode ser aceito com
**confiança média**, pela estrutura da cifra, se **todas** as condições valem:

- pelo menos 150 letras, e pelo menos 20 letras para cada letra de chave escolhida pela busca;
- uma camada desfeita (ou o XOR, que já exige que todo byte vire letra);
- IC do texto decifrado de língua natural (≥ 0,055): as colunas da chave ficaram alinhadas entre si;
- frequência das letras parecida com a do português (≥ −3,40) e bem melhor que a do texto cifrado (+0,25);
- a **metade mais natural** do texto (blocos de 20 letras) acima de um limiar próprio, que também sobe com o tamanho
  da chave.

A tela mostra um aviso amarelo com esses números e pede para ler o texto para confirmar. Esse critério foi calibrado
com os mesmos textos que nunca podem ser aceitos: letras aleatórias, substituição, texto embaralhado, Vigenère de 31 e
37 letras (inclusive sobre texto embaralhado), substituição seguida de Vigenère e substituição seguida de XOR. Em
6.800 desses textos, de 60 a 400 letras, **nenhum** foi aceito (nem pela média, nem pela estrutura).

Opções do motor (`bin/motor auto decifrar`): `--max-camadas 1-4` (padrão 4), `--max-periodo 1-24` (padrão 24: cada
Vigenère tem até 8 letras, mas duas em sequência somam chaves de mmc letras) e `--tempo-ms` (padrão 4500). No modo
manual da tela os dois primeiros têm campos próprios.

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
c/modelo_pt.h       trigramas de letras do português (gerados de texto da Wikipédia e de Dom Casmurro)
c/modelo_en.h       trigramas de letras do inglês (gerados de livros do Project Gutenberg)
tests/              testes automatizados e a calibração do limiar
scripts/gerar-modelo-trigramas.py  gera as tabelas de trigramas
scripts/build-c.js  compila o motor com o gcc (ou usa o .exe pré-compilado no Windows)
prebuilt/           motor.exe pré-compilado para Windows e o hash do código-fonte de onde ele veio
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

## Testes

```bash
npm test          # compila o motor e roda tests/*.test.js (node:test, sem dependências)
npm run calibrar  # recalibra o limiar de "texto natural" (demora cerca de 1 minuto)
```

Os textos cifrados dos testes vêm de implementações de referência em JS (`tests/cifras.js`) que **não usam o
motor**, e as cifras e chaves que o motor reporta são reaplicadas por elas ao texto claro. A suíte cobre
cifras isoladas, César e Vigenère nas duas ordens, combinações com Atbash, três e quatro cifras (com XOR por último),
duas Vigenère em sequência, textos com muitos nomes próprios, chave desconhecida,
texto curto, textos sem solução (aleatório, Vigenère de 12 e 20 letras, substituição, embaralhado), limites de
camadas, período e tempo, e a compatibilidade com XOR e com os botões individuais.

## Limites conhecidos

- **Ordem não identificável:** ver acima. A divisão da chave entre César e Vigenère é uma convenção.
- A escolha da chave é estatística. **Abaixo de 55 letras o resultado é sempre inconclusivo** e, até uns 80,
  a margem é pequena. Para textos de 175 a 250 letras o texto voltou idêntico em mais de 99,8% dos testes.
- O limiar de "natural" foi calibrado com português e inglês. Outro idioma, ou texto sem sentido, pode
  ficar inconclusivo (ou, se for parecido com português/inglês, ser aceito com o idioma errado).
- Chave efetiva acima de 24 letras não é resolvida: por exemplo, Vigenère de 5, 3 e 2 letras em sequência (mmc 30)
  ou de 6 e 5 (mmc 30). Essas cadeias também passam do orçamento do regulamento (8 letras de Vigenère = 37,6 bits).
- Com duas Vigenère somando 9 letras ou mais **e** muitos nomes próprios, o resultado pode ficar inconclusivo.
- **Com 3 ou 4 cifras de letras, o programa não sabe quantas foram usadas:** `César → Vigenère → Atbash` e
  `Vigenère(chave somada) → Atbash` dão o mesmo texto cifrado. A resposta mostra a forma efetiva (conferida), não a
  sequência original.
- Só são analisadas Atbash, César e Vigenère (e o XOR, por último); outras cifras clássicas ficam inconclusivas.
- Se o último byte **cifrado** do XOR for `0x0A` (quebra de linha), o regulamento manda ignorá-lo
  e a última letra se perde; a tela avisa qual seria a letra.
- A quebra de linha do fim do arquivo é aceita em qualquer forma: ausente, `\n`, `\r\n` crua, ou a própria quebra do texto
  cifrada junto com o XOR (arquivo com o dobro de bytes). Nos quatro casos a prova real ("Conferido") é feita.
  Um `\r\n` em que os **dois** bytes foram cifrados não é suportado.
- O Vigenère e as combinações só são descobertos quando o texto (antes do XOR) tem apenas letras a-z.

## Observação sobre o `--no-sandbox`

O `npm run electron` abre o Electron com `--no-sandbox`. Em distribuições
baseadas no Ubuntu 24.04 (como o Mint 22), o Electron instalado pelo npm pode não
abrir sem isso por causa do sandbox do Chromium. A aplicação só carrega arquivos
locais dela mesma.
