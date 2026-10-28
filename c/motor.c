/*
 * motor.c - programa em C chamado pela interface Electron.
 *
 * Os algoritmos originais (pasta algoritmos/) NAO foram alterados. Eles tem
 * o texto e a chave fixos dentro do main(), entao a interface nao consegue
 * passar dados para eles. Este motor usa as MESMAS formulas dos originais,
 * mas recebe o texto pela entrada padrao (stdin) e a chave pela linha de
 * comando.
 *
 * Uso:   motor <cesar|vigenere|atbash|xor|auto> <cifrar|decifrar> [chave]
 *
 * Sem chave, decifrar testa TODAS as possibilidades e escolhe a que mais
 * parece portugues. "auto" tambem testa as combinacoes de duas cifras.
 *
 * stdout: somente o texto resultante
 * stderr: avisos, erros e linhas que a interface entende:
 *           @cifras <texto>   cifras encontradas (ordem em que foram aplicadas)
 *           @chave <texto>    chave(s) encontrada(s)
 *           @valor <nome> | <decimal> | <hexa> | <binario>   cada valor da chave
 *           @opcao <descricao> :: <previa>   outras possibilidades testadas
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <time.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "modelo_pt.h"   /* TRIGRAMA_PT: trigramas de letras do portugues */
#include "modelo_en.h"   /* TRIGRAMA_EN: trigramas de letras do ingles */

#define MAX_CHAVE_XOR 16
#define MAX_BYTES_BUSCA_XOR 5     /* 40 bits: maior orcamento do torneio */
#define TOLERANCIA_TAMANHO 0.02   /* ver buscar_chave_xor() */
#define LIMIAR_PORTUGUES -3.1     /* so para o XOR: frequencia de letras, portugues fica perto de -2.7 */
#define LIMIAR_TRIGRAMA -3.4      /* portugues fica perto de -2.2; chave errada, perto de -3.7 */
#define MAX_CHAVE_VIGENERE 8      /* maximo de letras permitido no regulamento */
#define TOLERANCIA_VIGENERE 0.03  /* ver buscar_vigenere() */
#define TOLERANCIA_SIMPLES 0.01   /* ver executar_automatico() */
#define PESO_PALAVRA 1.0          /* pontos por letra que faz parte de uma palavra comum */
#define MAX_PONTUADO 4096
#define PREVIA 70                 /* caracteres mostrados em cada possibilidade */
#define MAX_PERIODO 24            /* maior chave de Vigenere (em letras) que a busca aceita */
#define MAX_CAMADAS 4             /* maior numero de cifras em sequencia que a busca aceita */
#define PADRAO_MAX_CAMADAS 3
#define PADRAO_MAX_PERIODO 8      /* o regulamento permite chaves de ate 8 letras */
#define PADRAO_TEMPO_MS 3000
#define LARGURA_FEIXE 3           /* hipoteses mantidas em cada nivel da busca em camadas */
#define GANHO_MINIMO 0.10         /* quanto uma camada precisa melhorar o texto (nats por letra) */
#define MIN_LETRAS 55             /* abaixo disso nenhuma conclusao e confiavel (ver limiar_natural) */
#define TOLERANCIA_XOR_AUTO 0.08  /* ver buscar_xor_automatico() */
#define MAX_COMBINACOES_XOR 4096
#define MAX_ALTERNATIVAS_XOR 6    /* chaves XOR finalistas no automatico */

static unsigned char *ler_entrada(size_t *tamanho) {
    size_t cap = 4096, n = 0, lidos;
    unsigned char *buf = malloc(cap);
    if (buf == NULL) {
        return NULL;
    }
    while ((lidos = fread(buf + n, 1, cap - n, stdin)) > 0) {
        n += lidos;
        if (n == cap) {
            unsigned char *novo = realloc(buf, cap * 2);
            if (novo == NULL) {
                free(buf);
                return NULL;
            }
            buf = novo;
            cap *= 2;
        }
    }
    *tamanho = n;
    return buf;
}

/* Remove uma quebra de linha no final (o regulamento manda ignora-la). */
static size_t sem_quebra_final(const unsigned char *texto, size_t n) {
    if (n > 0 && texto[n - 1] == '\n') {
        n--;
    }
    if (n > 0 && texto[n - 1] == '\r') {
        n--;
    }
    return n;
}

/* Le um numero decimal ("42") ou hexadecimal ("0x2a"). */
static int ler_numero(const char *s, long *valor) {
    int base = 10;
    char *fim;

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        s += 2;
    }
    if (*s == '\0') {
        return 0;
    }
    *valor = strtol(s, &fim, base);
    return *fim == '\0';
}

/* ------------------------------------------------------------------ */
/* Frequencia das letras no portugues (em %) e palavras comuns. Usadas */
/* so para escolher entre as chaves testadas (busca sem chave).        */
/* ------------------------------------------------------------------ */
static const double FREQUENCIA_PT[26] = {
    14.63, 1.04, 3.88, 4.99, 12.57, 1.02, 1.30, 1.28, 6.18, 0.40, 0.02, 2.78, 4.74,
    5.05, 10.73, 2.52, 1.20, 6.53, 7.81, 4.34, 4.63, 1.67, 0.01, 0.21, 0.01, 0.47
};
static double log_frequencia[26];

/* Sem acentos, como o texto claro do torneio. */
static const char *const PALAVRAS[] = {
    "que", "uma", "com", "por", "mas", "foi", "ele", "ela", "das", "dos", "nos", "seu", "sua",
    "tem", "ser", "ter", "aos", "nas", "num", "meu", "dia", "ano", "vez", "sol", "mar", "paz",
    "sem", "vai", "vao", "bem", "mal", "fez", "ver", "viu", "ler", "ate", "sao", "nao", "era",
    "mais", "como", "para", "pelo", "pela", "isso", "esse", "essa", "este", "esta", "quem",
    "todo", "toda", "cada", "onde", "aqui", "hoje", "tudo", "nada", "vida", "casa", "tempo",
    "anos", "dias", "agua", "novo", "nova", "bons", "boas", "rua", "pai", "mae", "arte",
    "muito", "muitos", "muitas", "quando", "depois", "tambem", "entre", "mesmo", "mesma",
    "ainda", "sempre", "nunca", "todos", "todas", "outro", "outra", "outros", "outras",
    "grande", "escola", "aluno", "alunos", "aula", "aulas", "amigo", "amigos", "grupo",
    "grupos", "texto", "trabalho", "cidade", "pessoa", "pessoas", "mundo", "praia", "parque",
    "livro", "livros", "estudo", "estudar", "projeto", "projetos", "ciencia", "ciencias",
    "historia", "mensagem", "codigo", "chave", "chaves", "senha", "semana", "manha", "tarde",
    "noite", "ontem", "amanha", "melhor", "primeiro", "segundo", "final", "fazer", "vamos",
    "fomos", "voltar", "voltou", "chegou", "chegar", "passou", "passar", "ficou", "ficar",
    "falou", "falar", "disse", "olhou", "escreveu", "escrever", "aprender", "aprendeu",
    "ensinar", "jogo", "jogos", "time", "jogador", "jogadores", "professor", "professora",
    "biblioteca", "estudantes", "estudante", "prova", "provas", "comida", "frutas", "suco",
    "chuva", "vento", "calor", "frio", "festa", "familia", "filho", "filha", "irmao", "praca",
    "centro", "bairro", "estrada", "carro", "onibus", "viagem", "viajar", "passeio",
    "dormir", "acordar", "correr", "andar", "cantar", "musica", "filme", "cinema", "teatro",
    "pintura", "desenho", "ouvir", "sentir", "pensar", "saber", "querer", "poder", "dever",
    "gostar", "criptografia", "cifra", "cifrar", "decifrar", "torneio", "desafio", "segredo",
    "durante", "sobre", "enquanto", "porque", "entao", "assim", "apenas", "quase", "algum",
    "alguma", "alguns", "algumas", "nenhum", "nenhuma", "qualquer", "varios", "varias",
    "pouco", "pouca", "tanto", "tanta", "menos", "antes", "agora", "logo", "cedo", "longe",
    "perto", "dentro", "fora", "acima", "abaixo", "frente", "atras", "lado", "estava",
    "estavam", "estamos", "estao", "estou", "foram", "tinha", "tinham", "havia", "fazia",
    "fazem", "feito", "dizem", "passa", "passam", "deixou", "deixar", "levou", "levar",
    "trouxe", "trazer", "encontrar", "encontram", "encontrou", "procurar", "procurou",
    "abriu", "abrir", "fechou", "fechar", "comecou", "comecar", "terminou", "terminar",
    "ganhou", "ganhar", "perdeu", "perder", "venceu", "vencer", "jogaram", "jogar", "jogou",
    "treinar", "treinamento", "precisaram", "precisar", "aproveitou", "aproveitar",
    "explicar", "explicou", "naquele", "naquela", "aquele", "aquela", "dela", "dele", "seus",
    "suas", "minha", "nosso", "nossa", "nossos", "nossas", "essas", "esses", "estas", "estes"
};
#define QTD_PALAVRAS (sizeof PALAVRAS / sizeof PALAVRAS[0])
static size_t tamanho_palavra[QTD_PALAVRAS];
static size_t palavras_da_letra[26][QTD_PALAVRAS];   /* indices em PALAVRAS, por primeira letra */
static size_t qtd_da_letra[26];

static void preparar_frequencias(void) {
    for (int i = 0; i < 26; i++) {
        log_frequencia[i] = log(FREQUENCIA_PT[i] / 100.0);
    }
    for (size_t w = 0; w < QTD_PALAVRAS; w++) {
        int letra = PALAVRAS[w][0] - 'a';

        tamanho_palavra[w] = strlen(PALAVRAS[w]);
        palavras_da_letra[letra][qtd_da_letra[letra]++] = w;
    }
}

/* Idiomas que o avaliador linguistico reconhece. ic = indice de coincidencia do texto claro. */
typedef struct {
    const char *nome;
    const unsigned char *tabela;
    double ic;
} Idioma;

static const Idioma IDIOMAS[] = {
    { "português", TRIGRAMA_PT, 0.0745 },
    { "inglês", TRIGRAMA_EN, 0.0667 },
};
#define QTD_IDIOMAS ((int) (sizeof IDIOMAS / sizeof IDIOMAS[0]))
#define IC_ALEATORIO 0.0385       /* indice de coincidencia de letras sorteadas ao acaso (1/26) */

/* Media, por letra, do log da probabilidade pelos trigramas de UM idioma. */
static double media_trigramas_idioma(const unsigned char *texto, size_t n, int idioma) {
    const unsigned char *tabela = IDIOMAS[idioma].tabela;
    double total = 0;
    size_t contados = 0;
    int a = -1, b = -1;

    for (size_t i = 0; i < n; i++) {
        int letra = texto[i] - 'a';

        if (letra < 0 || letra > 25) {
            a = b = -1;
            continue;
        }
        if (a >= 0 && b >= 0) {
            total -= tabela[a * 676 + b * 26 + letra] / 16.0;
            contados++;
        }
        a = b;
        b = letra;
    }
    return contados > 0 ? total / (double) contados : -100.0;
}

/* Media pelo idioma que melhor explica o texto (quanto maior, mais "natural"). */
static double media_trigramas(const unsigned char *texto, size_t n, int *idioma) {
    double melhor = -HUGE_VAL;

    for (int i = 0; i < QTD_IDIOMAS; i++) {
        double media = media_trigramas_idioma(texto, n, i);

        if (media > melhor) {
            melhor = media;
            if (idioma != NULL) {
                *idioma = i;
            }
        }
    }
    return melhor;
}

/* Indice de coincidencia: chance de duas letras sorteadas do texto serem iguais. */
static double indice_coincidencia(const unsigned char *texto, size_t n) {
    size_t contagem[26] = {0}, total = 0;

    for (size_t i = 0; i < n; i++) {
        if (texto[i] >= 'a' && texto[i] <= 'z') {
            contagem[texto[i] - 'a']++;
            total++;
        }
    }
    if (total < 2) {
        return 0;
    }
    double soma = 0;
    for (int c = 0; c < 26; c++) {
        soma += (double) contagem[c] * (double) (contagem[c] - 1);
    }
    return soma / ((double) total * (double) (total - 1));
}

/*
 * Estima o periodo de uma cifra polialfabetica: para cada periodo candidato, a media ponderada do
 * indice de coincidencia das colunas (letras de mesma posicao na chave). Colunas de uma mesma
 * chave se parecem com texto natural (IC alto); com o periodo errado, com letras aleatorias.
 * Multiplos do periodo certo tambem pontuam alto, entao vale o menor periodo perto do maximo.
 */
static int estimar_periodo_ic(const unsigned char *texto, size_t n, int max_periodo, double *ic_medio) {
    double ics[MAX_PERIODO + 1], melhor = 0;
    int limite = max_periodo;

    if ((size_t) limite > n / 4) {
        limite = (int) (n / 4);     /* pelo menos 4 letras por coluna */
    }
    if (limite < 1) {
        limite = 1;
    }
    for (int tam = 1; tam <= limite; tam++) {
        double soma = 0;
        size_t peso = 0;

        for (int p = 0; p < tam; p++) {
            size_t contagem[26] = {0}, total = 0;

            for (size_t i = (size_t) p; i < n; i += (size_t) tam) {
                contagem[texto[i] - 'a']++;
                total++;
            }
            if (total >= 2) {
                double s2 = 0;

                for (int c = 0; c < 26; c++) {
                    s2 += (double) contagem[c] * (double) (contagem[c] - 1);
                }
                soma += s2 / ((double) total * (double) (total - 1)) * (double) total;
                peso += total;
            }
        }
        ics[tam] = peso > 0 ? soma / (double) peso : 0;
        if (ics[tam] > melhor) {
            melhor = ics[tam];
        }
    }
    for (int tam = 1; tam <= limite; tam++) {
        if (ics[tam] >= melhor - 0.006) {
            if (ic_medio != NULL) {
                *ic_medio = ics[tam];
            }
            return tam;
        }
    }
    return 1;
}

/*
 * Pontuacao total do texto: log da probabilidade de cada letra dadas as duas
 * anteriores (trigramas, ver modelo_pt.h e modelo_en.h; vale o idioma que melhor
 * explica o texto) mais, no portugues, PESO_PALAVRA por letra que pertence a uma
 * palavra comum. Texto sem sentido (chave errada)
 * pontua bem menos que o texto claro. Os trigramas reconhecem portugues mesmo
 * quando o texto tem palavras raras (waffle, yakisoba), que enganam um
 * criterio baseado so na frequencia de cada letra.
 */
static double pontuacao(const unsigned char *texto, size_t n) {
    static unsigned char coberto[MAX_PONTUADO];
    size_t m = n < MAX_PONTUADO ? n : MAX_PONTUADO;
    size_t cobertos = 0;
    double total = 0, total_ingles = 0;
    int a = -1, b = -1;     /* duas letras anteriores (-1 = inicio do texto ou nao-letra) */

    for (size_t i = 0; i < n; i++) {
        int letra = texto[i] - 'a';

        if (letra < 0 || letra > 25) {
            a = b = -1;
            continue;
        }
        if (a >= 0 && b >= 0) {
            total -= TRIGRAMA_PT[a * 676 + b * 26 + letra] / 16.0;
            total_ingles -= TRIGRAMA_EN[a * 676 + b * 26 + letra] / 16.0;
        } else {
            total += log_frequencia[letra];
            total_ingles += log_frequencia[letra];
        }
        a = b;
        b = letra;
    }
    memset(coberto, 0, m);
    for (size_t i = 0; i < m; i++) {
        int letra = texto[i] - 'a';

        if (letra < 0 || letra > 25) {
            continue;
        }
        for (size_t k = 0; k < qtd_da_letra[letra]; k++) {
            size_t w = palavras_da_letra[letra][k];
            size_t tam = tamanho_palavra[w];

            if (i + tam <= m && memcmp(texto + i, PALAVRAS[w], tam) == 0) {
                memset(coberto + i, 1, tam);
            }
        }
    }
    for (size_t i = 0; i < m; i++) {
        cobertos += coberto[i];
    }
    total += PESO_PALAVRA * (double) cobertos;
    return total > total_ingles ? total : total_ingles;
}

/* ------------------------------------------------------------------ */
/* Saida para a interface (stderr).                                    */
/* ------------------------------------------------------------------ */

/* Uma linha da tabela de chave: valor em decimal, hexadecimal e binario. */
static void emitir_valor(const char *nome, int valor, int bits) {
    fprintf(stderr, "@valor %s | %d | 0x%0*X | 0b", nome, valor, (bits + 3) / 4, (unsigned) valor);
    for (int b = bits - 1; b >= 0; b--) {
        fputc('0' + ((valor >> b) & 1), stderr);
    }
    fputc('\n', stderr);
}

/* Outra possibilidade testada, com os primeiros caracteres do texto. */
static void emitir_opcao(const char *descricao, const unsigned char *texto, size_t n) {
    size_t m = n < PREVIA ? n : PREVIA;

    fprintf(stderr, "@opcao %s :: ", descricao);
    for (size_t i = 0; i < m; i++) {
        fputc(texto[i] >= 32 && texto[i] < 127 ? texto[i] : ' ', stderr);
    }
    if (n > m) {
        fputs("…", stderr);
    }
    fputc('\n', stderr);
}

static void avisar_se_nao_parece_portugues(const unsigned char *texto, size_t n) {
    if (media_trigramas(texto, n, NULL) < LIMIAR_TRIGRAMA) {
        fprintf(stderr, "O resultado não parece português: o texto pode ser curto demais ou "
                        "usar outra combinação de cifras.\n");
    }
}

/* ------------------------------------------------------------------ */
/* CESAR - mesma regra do caesar.c: senha positiva cifra, negativa     */
/* decifra. So letras de a a z sao alteradas.                          */
/* ------------------------------------------------------------------ */
static void cesar(unsigned char *texto, size_t n, int senha) {
    for (size_t i = 0; i < n; i++) {
        if (texto[i] < 'a' || texto[i] > 'z') {
            continue;
        }
        if (senha > 0) {
            if (((int) texto[i] + senha) > 122) {
                texto[i] += senha - 26;
            } else {
                texto[i] += senha;
            }
        } else {
            if (((int) texto[i] + senha) < 97) {
                texto[i] += senha + 26;
            } else {
                texto[i] += senha;
            }
        }
    }
}

/* Sem chave: testa os 25 deslocamentos e fica com o mais parecido com portugues. */
static int descobrir_cesar(unsigned char *texto, size_t n) {
    double pontos[26];
    int ordem[25];
    unsigned char *copia;

    n = sem_quebra_final(texto, n);
    if (n == 0) {
        fprintf(stderr, "Erro: o texto está vazio.\n");
        return 1;
    }
    copia = malloc(n);
    if (copia == NULL) {
        fprintf(stderr, "Erro: memória insuficiente.\n");
        return 1;
    }

    for (int d = 1; d <= 25; d++) {
        memcpy(copia, texto, n);
        cesar(copia, n, -d);
        pontos[d] = pontuacao(copia, n);
        ordem[d - 1] = d;
    }
    for (int i = 1; i < 25; i++) {          /* ordena do mais para o menos provavel */
        int atual = ordem[i], j = i - 1;
        while (j >= 0 && pontos[ordem[j]] < pontos[atual]) {
            ordem[j + 1] = ordem[j];
            j--;
        }
        ordem[j + 1] = atual;
    }

    memcpy(copia, texto, n);
    cesar(copia, n, -ordem[0]);
    fwrite(copia, 1, n, stdout);
    fprintf(stderr, "@cifras César\n@chave deslocamento %d\n", ordem[0]);
    emitir_valor("deslocamento", ordem[0], 5);
    avisar_se_nao_parece_portugues(copia, n);

    for (int k = 0; k < 5; k++) {
        char descricao[48];

        memcpy(copia, texto, n);
        cesar(copia, n, -ordem[k]);
        snprintf(descricao, sizeof descricao, "deslocamento %d", ordem[k]);
        emitir_opcao(descricao, copia, n);
    }
    fprintf(stderr, "Foram testados os 25 deslocamentos.\n");
    free(copia);
    return 0;
}

static int executar_cesar(unsigned char *texto, size_t n, int cifrar, const char *chave) {
    long deslocamento;

    if (chave[0] == '\0') {
        if (cifrar) {
            fprintf(stderr, "Erro: informe o deslocamento do César (1 a 25).\n");
            return 1;
        }
        return descobrir_cesar(texto, n);
    }

    if (!ler_numero(chave, &deslocamento) || deslocamento < 1 || deslocamento > 25) {
        fprintf(stderr, "Erro: a chave do César deve ser um número de 1 a 25.\n");
        return 1;
    }
    cesar(texto, n, cifrar ? (int) deslocamento : (int) -deslocamento);
    fwrite(texto, 1, n, stdout);
    return 0;
}

/* ------------------------------------------------------------------ */
/* VIGENERE - mesmas formulas do vigenere.c. A posicao da senha avanca */
/* so nas letras; para texto so com a-z (regra do torneio) o resultado */
/* e identico ao original.                                             */
/* ------------------------------------------------------------------ */
typedef struct {
    int tam;
    int desl[MAX_PERIODO];          /* 0 a 25: a=0, b=1, ... z=25 */
    double pontos;
} ChaveVigenere;

static void vigenere_para_texto(const ChaveVigenere *k, char *saida) {
    for (int i = 0; i < k->tam; i++) {
        saida[i] = (char) ('a' + k->desl[i]);
    }
    saida[k->tam] = '\0';
}

/* Mesma conta do vigenere.c: letra - chave. So para texto a-z. */
static void vigenere_decifrar_buffer(const unsigned char *origem, size_t n, const int *desl, int tam,
                                     unsigned char *destino) {
    for (size_t i = 0; i < n; i++) {
        destino[i] = (unsigned char) ('a' + ((origem[i] - 'a') - desl[i % (size_t) tam] + 26) % 26);
    }
}

/*
 * Testa todas as chaves de 1 a "limite" letras. Para cada tamanho, cada letra da
 * chave e uma "coluna" do texto: os 26 deslocamentos de cada coluna sao
 * testados e os 2 melhores (pela frequencia das letras) entram nas
 * combinacoes, que sao decididas pela pontuacao do texto inteiro (acima de
 * 8 letras so a melhor de cada coluna entra, senao seriam 2^tam combinacoes).
 * Chaves maiores sempre se ajustam um pouco melhor, entao fica o menor
 * tamanho que chega perto da melhor pontuacao.
 * Retorna quantos tamanhos foram testados (0 se faltar memoria).
 */
static int buscar_vigenere(const unsigned char *texto, size_t n, ChaveVigenere *escolhida,
                           ChaveVigenere por_tamanho[], int limite) {
    unsigned char *tentativa = malloc(n > 0 ? n : 1);
    double melhor_geral = -HUGE_VAL;
    int max_tam = limite > MAX_PERIODO ? MAX_PERIODO : limite;

    if ((size_t) max_tam > n) {
        max_tam = (int) n;
    }

    if (tentativa == NULL) {
        return 0;
    }
    for (int tam = 1; tam <= max_tam; tam++) {
        int primeiro[MAX_PERIODO], segundo[MAX_PERIODO];
        ChaveVigenere *r = &por_tamanho[tam];

        for (int p = 0; p < tam; p++) {
            double pontos[26];
            int a = 0, b;

            for (int s = 0; s < 26; s++) {
                pontos[s] = 0;
                for (size_t i = (size_t) p; i < n; i += (size_t) tam) {
                    pontos[s] += log_frequencia[(texto[i] - 'a' - s + 26) % 26];
                }
                if (pontos[s] > pontos[a]) {
                    a = s;
                }
            }
            b = a == 0 ? 1 : 0;
            for (int s = 0; s < 26; s++) {
                if (s != a && pontos[s] > pontos[b]) {
                    b = s;
                }
            }
            primeiro[p] = a;
            segundo[p] = b;
        }

        r->tam = tam;
        r->pontos = -HUGE_VAL;
        for (unsigned mascara = 0; mascara < (tam <= MAX_CHAVE_VIGENERE ? (1u << tam) : 1u); mascara++) {
            int desl[MAX_PERIODO];
            double pts;

            for (int p = 0; p < tam; p++) {
                desl[p] = ((mascara >> p) & 1u) ? segundo[p] : primeiro[p];
            }
            vigenere_decifrar_buffer(texto, n, desl, tam, tentativa);
            pts = pontuacao(tentativa, n);
            if (pts > r->pontos) {
                r->pontos = pts;
                memcpy(r->desl, desl, sizeof(int) * (size_t) tam);
            }
        }

        /* Refinamento: troca uma letra por vez pelo melhor dos 26 deslocamentos,
           para o caso da letra certa nao estar entre as 2 melhores da coluna. */
        for (int rodada = 0, mudou = 1; mudou && rodada < 4; rodada++) {
            mudou = 0;
            for (int p = 0; p < tam; p++) {
                int original = r->desl[p];

                for (int s = 0; s < 26; s++) {
                    double pts;

                    if (s == original) {
                        continue;
                    }
                    r->desl[p] = s;
                    vigenere_decifrar_buffer(texto, n, r->desl, tam, tentativa);
                    pts = pontuacao(tentativa, n);
                    if (pts > r->pontos + 1e-9) {
                        r->pontos = pts;
                        original = s;
                        mudou = 1;
                    }
                }
                r->desl[p] = original;
            }
        }
        if (r->pontos > melhor_geral) {
            melhor_geral = r->pontos;
        }
    }

    for (int tam = 1; tam <= max_tam; tam++) {
        if (por_tamanho[tam].pontos >= melhor_geral - TOLERANCIA_VIGENERE * (double) n) {
            *escolhida = por_tamanho[tam];
            break;
        }
    }
    free(tentativa);
    return max_tam;
}

/* Sem chave: descobre a senha do Vigenere (1 a 8 letras). */
static int descobrir_vigenere(unsigned char *texto, size_t n) {
    ChaveVigenere por_tamanho[MAX_CHAVE_VIGENERE + 1], escolhida;
    int ordem[MAX_CHAVE_VIGENERE], max_tam;
    unsigned char *saida;
    char senha[MAX_CHAVE_VIGENERE + 1];

    n = sem_quebra_final(texto, n);
    if (n == 0) {
        fprintf(stderr, "Erro: o texto está vazio.\n");
        return 1;
    }
    for (size_t i = 0; i < n; i++) {
        if (texto[i] < 'a' || texto[i] > 'z') {
            fprintf(stderr, "Erro: para descobrir a senha, o texto deve ter apenas letras de a a z "
                            "(sem espaços, acentos ou números).\n");
            return 1;
        }
    }
    saida = malloc(n);
    if (saida == NULL) {
        fprintf(stderr, "Erro: memória insuficiente.\n");
        return 1;
    }
    max_tam = buscar_vigenere(texto, n, &escolhida, por_tamanho, MAX_CHAVE_VIGENERE);
    if (max_tam == 0) {
        free(saida);
        fprintf(stderr, "Erro: memória insuficiente.\n");
        return 1;
    }

    vigenere_decifrar_buffer(texto, n, escolhida.desl, escolhida.tam, saida);
    fwrite(saida, 1, n, stdout);

    vigenere_para_texto(&escolhida, senha);
    fprintf(stderr, "@cifras Vigenère\n@chave %s\n", senha);
    for (int i = 0; i < escolhida.tam; i++) {
        char nome[32];

        snprintf(nome, sizeof nome, "letra %d (%c)", i + 1, senha[i]);
        emitir_valor(nome, escolhida.desl[i], 5);
    }
    avisar_se_nao_parece_portugues(saida, n);
    if (n < 20 * (size_t) escolhida.tam) {
        fprintf(stderr, "Aviso: o texto é curto para uma chave de %d letra%s; confira o resultado.\n",
                escolhida.tam, escolhida.tam == 1 ? "" : "s");
    }

    for (int i = 0; i < max_tam; i++) {     /* um candidato por tamanho de chave */
        ordem[i] = i + 1;
    }
    for (int i = 1; i < max_tam; i++) {
        int atual = ordem[i], j = i - 1;
        while (j >= 0 && por_tamanho[ordem[j]].pontos < por_tamanho[atual].pontos) {
            ordem[j + 1] = ordem[j];
            j--;
        }
        ordem[j + 1] = atual;
    }
    for (int i = 0; i < max_tam && i < 4; i++) {
        char descricao[64], outra[MAX_CHAVE_VIGENERE + 1];
        const ChaveVigenere *k = &por_tamanho[ordem[i]];

        vigenere_para_texto(k, outra);
        snprintf(descricao, sizeof descricao, "chave \"%s\" (%d letra%s)", outra, k->tam,
                 k->tam == 1 ? "" : "s");
        vigenere_decifrar_buffer(texto, n, k->desl, k->tam, saida);
        emitir_opcao(descricao, saida, n);
    }
    fprintf(stderr, "Foram testados todos os 26 deslocamentos de cada letra, para chaves de 1 a %d letra%s.\n",
            max_tam, max_tam == 1 ? "" : "s");
    free(saida);
    return 0;
}

static int executar_vigenere(unsigned char *texto, size_t n, int cifrar, const char *chave) {
    char senha[256];
    size_t tamanho_senha = strlen(chave);
    size_t j = 0;

    if (tamanho_senha == 0) {
        if (!cifrar) {
            return descobrir_vigenere(texto, n);
        }
        fprintf(stderr, "Erro: informe a senha do Vigenère (letras de a a z).\n");
        return 1;
    }
    if (tamanho_senha >= sizeof senha) {
        fprintf(stderr, "Erro: senha muito longa.\n");
        return 1;
    }
    for (size_t i = 0; i < tamanho_senha; i++) {
        int c = tolower((unsigned char) chave[i]);
        if (c < 'a' || c > 'z') {
            fprintf(stderr, "Erro: a senha do Vigenère deve ter apenas letras de a a z.\n");
            return 1;
        }
        senha[i] = (char) c;
    }

    for (size_t i = 0; i < n; i++) {
        if (texto[i] < 'a' || texto[i] > 'z') {
            continue;
        }
        int letra = texto[i] - 'a';
        int k = senha[j % tamanho_senha] - 'a';

        if (cifrar) {
            texto[i] = 'a' + (letra + k) % 26;
        } else {
            texto[i] = 'a' + (letra - k + 26) % 26;
        }
        j++;
    }
    fwrite(texto, 1, n, stdout);
    return 0;
}

/* ------------------------------------------------------------------ */
/* ATBASH - mesmas funcoes do atbash.c (cifrar e decifrar sao iguais). */
/* ------------------------------------------------------------------ */
static int posicao_alfabeto(unsigned char letra) {
    if (letra >= 'a' && letra <= 'z') {
        return letra - 'a' + 1;
    } else if (letra >= 'A' && letra <= 'Z') {
        return letra - 'A' + 1;
    }
    return -1;
}

static int executar_atbash(unsigned char *texto, size_t n, const char *chave) {
    if (chave[0] != '\0') {
        fprintf(stderr, "Aviso: o Atbash não usa chave; a chave foi ignorada.\n");
    }
    for (size_t i = 0; i < n; i++) {
        int pos = posicao_alfabeto(texto[i]);

        if (pos == -1) {
            continue;
        }
        if (texto[i] >= 'a' && texto[i] <= 'z') {
            texto[i] = 'a' + (26 - pos);
        } else {
            texto[i] = 'A' + (26 - pos);
        }
    }
    fwrite(texto, 1, n, stdout);
    fprintf(stderr, "@cifras Atbash\n@chave o Atbash não tem chave (inverte o alfabeto: a↔z, b↔y, ...)\n");
    return 0;
}

/* ------------------------------------------------------------------ */
/* XOR - formato do regulamento: cada byte cifrado (0 a 255) e gravado */
/* como o caractere de mesmo codigo em UTF-8. Diferente do xor.c, o    */
/* arquivo e lido inteiro uma unica vez (o original nao voltava ao     */
/* inicio do arquivo, por isso so o deslocamento 1 aparecia).          */
/* ------------------------------------------------------------------ */

/* Converte UTF-8 em codigos 0-255. Retorna a quantidade ou -1. */
static long utf8_para_codigos(const unsigned char *b, size_t n, unsigned char *saida) {
    size_t i = 0, k = 0;

    while (i < n) {
        if (b[i] < 0x80) {
            saida[k++] = b[i++];
        } else if ((b[i] == 0xC2 || b[i] == 0xC3) && i + 1 < n && (b[i + 1] & 0xC0) == 0x80) {
            saida[k++] = (unsigned char) (((b[i] & 0x1F) << 6) | (b[i + 1] & 0x3F));
            i += 2;
        } else {
            return -1;
        }
    }
    return (long) k;
}

static void escrever_utf8(unsigned char c) {
    if (c < 0x80) {
        putchar(c);
    } else {
        putchar(0xC0 | (c >> 6));
        putchar(0x80 | (c & 0x3F));
    }
}

/* Le "42", "0x2a" ou varios bytes separados: "12, 200, 0x07". */
static int ler_chave_xor(const char *texto, unsigned char *chave) {
    char copia[512];
    int n = 0;

    if (strlen(texto) >= sizeof copia) {
        return -1;
    }
    strcpy(copia, texto);
    for (char *tok = strtok(copia, " ,;\t"); tok != NULL; tok = strtok(NULL, " ,;\t")) {
        long v;
        if (n == MAX_CHAVE_XOR || !ler_numero(tok, &v) || v < 0 || v > 255) {
            return -1;
        }
        chave[n++] = (unsigned char) v;
    }
    return n;
}

/* Uma quebra de linha no final, cifrada ou nao, e ignorada (regulamento). */
static int final_ignoravel(const unsigned char *cod, size_t n, size_t i, unsigned char d) {
    return i == n - 1 && (cod[i] == '\n' || d == '\n');
}

static void escrever_decifrado(const unsigned char *cod, size_t n, const unsigned char *chave, size_t tam) {
    for (size_t i = 0; i < n; i++) {
        unsigned char d = cod[i] ^ chave[i % tam];
        if (i == n - 1 && cod[i] == '\n' && d != '\n') {
            if (d >= 'a' && d <= 'z') {
                fprintf(stderr, "Aviso: a quebra de linha no fim do arquivo foi ignorada (regulamento). "
                                "Se ela fosse um byte cifrado, a última letra seria '%c'.\n", d);
            }
            d = '\n';
        }
        escrever_utf8(d);
    }
}

/* Mesma conta de escrever_decifrado, mas para um buffer (sem a quebra de linha final). */
static size_t xor_para_buffer(const unsigned char *cod, size_t n, const unsigned char *chave, size_t tam,
                              unsigned char *saida) {
    size_t k = 0;

    for (size_t i = 0; i < n; i++) {
        unsigned char d = cod[i] ^ chave[i % tam];
        if (!final_ignoravel(cod, n, i, d)) {
            saida[k++] = d;
        }
    }
    return k;
}

/* A chave k serve para a coluna se todas as letras dela viram a-z. */
static int coluna_valida(const unsigned char *cod, size_t n, size_t inicio, size_t passo, unsigned char k) {
    for (size_t i = inicio; i < n; i += passo) {
        unsigned char d = cod[i] ^ k;
        if ((d >= 'a' && d <= 'z') || final_ignoravel(cod, n, i, d)) {
            continue;
        }
        return 0;
    }
    return 1;
}

/* Preenche os valores possiveis de cada byte da chave (os que deixam a
   coluna so com letras a-z). Retorna 0 se algum byte nao tem nenhum. */
static int candidatos_xor(const unsigned char *cod, size_t n, size_t tam,
                          unsigned char candidatos[][256], int *qtd) {
    for (size_t p = 0; p < tam; p++) {
        qtd[p] = 0;
        for (int k = 1; k <= 255; k++) {
            if (coluna_valida(cod, n, p, tam, (unsigned char) k)) {
                candidatos[p][qtd[p]++] = (unsigned char) k;
            }
        }
        if (qtd[p] == 0) {
            return 0;
        }
    }
    return 1;
}

/* Cifra testada depois de desfazer o XOR (fases 3 e 4):
   0 = nenhuma, 1 a 25 = Cesar com esse deslocamento, 26 = Atbash. */
#define ATBASH_DEPOIS 26

static int letra_depois_do_xor(unsigned char letra, int t) {
    int pos = letra - 'a';
    if (t == ATBASH_DEPOIS) {
        return 25 - pos;
    }
    return (pos - t + 26) % 26;
}

static double pontuar_coluna(const unsigned char *cod, size_t n, size_t inicio, size_t passo,
                             unsigned char k, int t) {
    double total = 0;
    for (size_t i = inicio; i < n; i += passo) {
        unsigned char d = cod[i] ^ k;
        if (final_ignoravel(cod, n, i, d)) {
            continue;
        }
        if (d >= 'a' && d <= 'z') {
            total += log_frequencia[letra_depois_do_xor(d, t)];
        }
    }
    return total;
}

typedef struct {
    size_t tam;
    unsigned char chave[MAX_BYTES_BUSCA_XOR];
    int depois;      /* cifra que parece estar por baixo do XOR (ver ATBASH_DEPOIS) */
    double pontos;
} XorBusca;

/*
 * Sem chave: procura chaves de 1 a 5 bytes. Como o texto que entrou no XOR
 * so tem letras a-z, cada byte da chave e testado separadamente na sua
 * "coluna" do texto. Varios valores podem gerar so letras, entao a escolha
 * e feita pela frequencia das letras do portugues, testando tambem Cesar e
 * Atbash aplicados antes do XOR (desafios combinados).
 * Retorna 0 se nenhuma chave transforma o texto so em letras.
 */
static int buscar_chave_xor(const unsigned char *cod, size_t n, XorBusca *resultado) {
    static unsigned char candidatos[MAX_BYTES_BUSCA_XOR][256];
    int qtd[MAX_BYTES_BUSCA_XOR];
    int existe[MAX_BYTES_BUSCA_XOR + 1] = {0};
    double pontos[MAX_BYTES_BUSCA_XOR + 1];
    unsigned char chaves[MAX_BYTES_BUSCA_XOR + 1][MAX_BYTES_BUSCA_XOR];
    int depois[MAX_BYTES_BUSCA_XOR + 1];
    double melhor = -HUGE_VAL;
    size_t escolhido = 0;

    for (size_t tam = 1; tam <= MAX_BYTES_BUSCA_XOR && tam <= n; tam++) {
        if (!candidatos_xor(cod, n, tam, candidatos, qtd)) {
            continue;
        }
        existe[tam] = 1;
        pontos[tam] = -HUGE_VAL;
        for (int t = 0; t <= ATBASH_DEPOIS; t++) {
            unsigned char chave[MAX_BYTES_BUSCA_XOR];
            double total = 0;

            for (size_t p = 0; p < tam; p++) {
                double melhor_coluna = -HUGE_VAL;
                for (int c = 0; c < qtd[p]; c++) {
                    double v = pontuar_coluna(cod, n, p, tam, candidatos[p][c], t);
                    if (v > melhor_coluna) {
                        melhor_coluna = v;
                        chave[p] = candidatos[p][c];
                    }
                }
                total += melhor_coluna;
            }
            if (total > pontos[tam]) {
                pontos[tam] = total;
                depois[tam] = t;
                memcpy(chaves[tam], chave, tam);
            }
        }
        if (pontos[tam] > melhor) {
            melhor = pontos[tam];
        }
    }

    if (melhor == -HUGE_VAL) {
        return 0;
    }

    /* Chaves maiores sempre se ajustam um pouco melhor ao texto; fica com
       a menor chave que chega perto da melhor pontuacao. */
    for (size_t tam = 1; tam <= MAX_BYTES_BUSCA_XOR; tam++) {
        if (existe[tam] && pontos[tam] >= melhor - TOLERANCIA_TAMANHO * (double) n) {
            escolhido = tam;
            break;
        }
    }

    resultado->tam = escolhido;
    memcpy(resultado->chave, chaves[escolhido], escolhido);
    resultado->depois = depois[escolhido];
    resultado->pontos = pontos[escolhido];
    return 1;
}

/* "200, 17, 51" */
static void chave_xor_para_texto(const unsigned char *chave, size_t tam, char *saida, size_t cap) {
    size_t usado = 0;

    saida[0] = '\0';
    for (size_t p = 0; p < tam && usado < cap; p++) {
        usado += (size_t) snprintf(saida + usado, cap - usado, "%s%u", p > 0 ? ", " : "", chave[p]);
    }
}

/* Uma linha de @valor para cada byte da chave XOR (decimal, hexa e binario). */
static void emitir_valores_xor(const unsigned char *chave, size_t tam) {
    for (size_t p = 0; p < tam; p++) {
        char nome[32];

        snprintf(nome, sizeof nome, "XOR byte %zu", p + 1);
        emitir_valor(nome, chave[p], 8);
    }
}

/*
 * Ajuste rapido do texto a uma chave de Vigenere de 1 a 8 letras (so pela
 * frequencia das letras de cada coluna). Serve para comparar chaves XOR
 * quando ha um Vigenere por baixo, caso em que a frequencia simples das
 * letras nao distingue as chaves. invertido = 1 testa o texto invertido
 * por Atbash.
 */
static double ajuste_rapido(const unsigned char *y, size_t n, int invertido) {
    double por_tamanho[MAX_CHAVE_VIGENERE + 1];
    double melhor = -HUGE_VAL;
    int max_tam = n < MAX_CHAVE_VIGENERE ? (int) n : MAX_CHAVE_VIGENERE;

    for (int tam = 1; tam <= max_tam; tam++) {
        double total = 0;

        for (int p = 0; p < tam; p++) {
            int contagem[26] = {0};
            double melhor_coluna = -HUGE_VAL;

            for (size_t i = (size_t) p; i < n; i += (size_t) tam) {
                int letra = y[i] - 'a';
                contagem[invertido ? 25 - letra : letra]++;
            }
            for (int s = 0; s < 26; s++) {
                double pontos = 0;

                for (int letra = 0; letra < 26; letra++) {
                    pontos += contagem[letra] * log_frequencia[(letra - s + 26) % 26];
                }
                if (pontos > melhor_coluna) {
                    melhor_coluna = pontos;
                }
            }
            total += melhor_coluna;
        }
        por_tamanho[tam] = total;
        if (total > melhor) {
            melhor = total;
        }
    }
    for (int tam = 1; tam <= max_tam; tam++) {
        if (por_tamanho[tam] >= melhor - TOLERANCIA_VIGENERE * (double) n) {
            return por_tamanho[tam];
        }
    }
    return melhor;
}

/*
 * Chaves XOR para o modo automatico: testa TODAS as combinacoes de valores
 * possiveis de cada byte (1 a 5 bytes) e devolve as melhores (ate "max"),
 * da mais para a menos provavel; quem decide entre elas e o texto final.
 * Chaves maiores sempre se ajustam um pouco melhor, entao so entram as do
 * menor tamanho que chega perto da melhor pontuacao.
 * Retorna quantas chaves foram devolvidas (0 se nenhuma chave transforma o
 * texto so em letras).
 */
static int buscar_xor_automatico(const unsigned char *cod, size_t n, XorBusca *resultados, int max) {
    static unsigned char candidatos[MAX_BYTES_BUSCA_XOR][256];
    static XorBusca topo[MAX_BYTES_BUSCA_XOR + 1][MAX_ALTERNATIVAS_XOR];
    int qtd[MAX_BYTES_BUSCA_XOR];
    int qtd_topo[MAX_BYTES_BUSCA_XOR + 1] = {0};
    double melhor = -HUGE_VAL;
    unsigned char *tentativa = malloc(n > 0 ? n : 1);

    if (tentativa == NULL) {
        return 0;
    }
    for (size_t tam = 1; tam <= MAX_BYTES_BUSCA_XOR && tam <= n; tam++) {
        long combinacoes = 1;
        int idx[MAX_BYTES_BUSCA_XOR] = {0};

        if (!candidatos_xor(cod, n, tam, candidatos, qtd)) {
            continue;
        }
        for (size_t p = 0; p < tam; p++) {   /* limita o total de combinacoes */
            combinacoes *= qtd[p];
        }
        while (combinacoes > MAX_COMBINACOES_XOR) {
            size_t maior = 0;

            for (size_t p = 1; p < tam; p++) {
                if (qtd[p] > qtd[maior]) {
                    maior = p;
                }
            }
            combinacoes = combinacoes / qtd[maior] * (qtd[maior] - 1);
            qtd[maior]--;
        }

        for (long c = 0; c < combinacoes; c++) {
            XorBusca atual;
            size_t m;
            double a, b;
            int pos;

            atual.tam = tam;
            atual.depois = 0;
            for (size_t p = 0; p < tam; p++) {
                atual.chave[p] = candidatos[p][idx[p]];
            }
            m = xor_para_buffer(cod, n, atual.chave, tam, tentativa);
            a = ajuste_rapido(tentativa, m, 0);
            b = ajuste_rapido(tentativa, m, 1);
            atual.pontos = b > a ? b : a;

            /* guarda so as MAX_ALTERNATIVAS_XOR melhores deste tamanho */
            pos = qtd_topo[tam];
            if (pos == MAX_ALTERNATIVAS_XOR && atual.pontos <= topo[tam][pos - 1].pontos) {
                pos = -1;
            } else if (pos == MAX_ALTERNATIVAS_XOR) {
                pos--;
            } else {
                qtd_topo[tam]++;
            }
            if (pos >= 0) {
                while (pos > 0 && topo[tam][pos - 1].pontos < atual.pontos) {
                    topo[tam][pos] = topo[tam][pos - 1];
                    pos--;
                }
                topo[tam][pos] = atual;
            }

            for (size_t p = tam; p-- > 0;) {
                if (++idx[p] < qtd[p]) {
                    break;
                }
                idx[p] = 0;
            }
        }
        if (qtd_topo[tam] > 0 && topo[tam][0].pontos > melhor) {
            melhor = topo[tam][0].pontos;
        }
    }
    free(tentativa);

    for (size_t tam = 1; tam <= MAX_BYTES_BUSCA_XOR; tam++) {
        if (qtd_topo[tam] > 0 && topo[tam][0].pontos >= melhor - TOLERANCIA_XOR_AUTO * (double) n) {
            int total = qtd_topo[tam] < max ? qtd_topo[tam] : max;

            memcpy(resultados, topo[tam], sizeof(XorBusca) * (size_t) total);
            return total;
        }
    }
    return 0;
}

static int busca_xor(const unsigned char *cod, size_t n) {
    static unsigned char candidatos[MAX_BYTES_BUSCA_XOR][256];
    int qtd[MAX_BYTES_BUSCA_XOR];
    XorBusca r;
    char texto_chave[96];

    if (!buscar_chave_xor(cod, n, &r)) {
        fprintf(stderr, "Nenhuma chave XOR de 1 a %d bytes transforma o texto somente em letras a-z. "
                        "Informe a chave manualmente no campo Chave.\n", MAX_BYTES_BUSCA_XOR);
        return 1;
    }

    chave_xor_para_texto(r.chave, r.tam, texto_chave, sizeof texto_chave);
    fprintf(stderr, "@cifras XOR (%zu byte%s = %zu bits)\n@chave %s\n", r.tam, r.tam == 1 ? "" : "s",
            r.tam * 8, texto_chave);
    emitir_valores_xor(r.chave, r.tam);

    if (r.pontos / (double) n < LIMIAR_PORTUGUES) {
        fprintf(stderr, "O resultado não parece português: pode estar cifrado também com Vigenère "
                        "(use o botão Automático).\n");
    } else if (r.depois == ATBASH_DEPOIS) {
        fprintf(stderr, "O resultado parece estar cifrado também com Atbash: use o resultado como entrada e clique em Atbash.\n");
    } else if (r.depois > 0) {
        fprintf(stderr, "O resultado parece estar cifrado também com César (deslocamento %d): "
                        "use o resultado como entrada e decifre com César.\n", r.depois);
    }

    candidatos_xor(cod, n, r.tam, candidatos, qtd);
    for (size_t p = 0; p < r.tam; p++) {
        if (qtd[p] < 2) {
            continue;
        }
        fprintf(stderr, "Outros valores que também geram só letras no byte %zu:", p + 1);
        for (int c = 0; c < qtd[p]; c++) {
            if (candidatos[p][c] != r.chave[p]) {
                fprintf(stderr, " %u", candidatos[p][c]);
            }
        }
        fprintf(stderr, ".\n");
    }

    escrever_decifrado(cod, n, r.chave, r.tam);
    return 0;
}

/* Converte o arquivo/texto cifrado em codigos 0-255. Retorna a quantidade ou -1. */
static long preparar_codigos(unsigned char *entrada, size_t n, unsigned char *cod) {
    long qtd = utf8_para_codigos(entrada, n, cod);

    if (qtd < 0) {
        fprintf(stderr, "Aviso: o arquivo não está em UTF-8 com códigos 0-255; os bytes foram lidos diretamente.\n");
        memcpy(cod, entrada, n);
        qtd = (long) n;
    }
    return qtd;
}

static int executar_xor(unsigned char *entrada, size_t n, int cifrar, const char *chave_texto) {
    unsigned char chave[MAX_CHAVE_XOR];
    int tam = 0;
    long qtd;
    unsigned char *cod;

    if (chave_texto[0] != '\0') {
        tam = ler_chave_xor(chave_texto, chave);
        if (tam <= 0) {
            fprintf(stderr, "Erro: chave XOR inválida. Use bytes de 0 a 255, ex.: 42  ou  0x2a  ou  12, 200, 7\n");
            return 1;
        }
    } else if (cifrar) {
        fprintf(stderr, "Erro: informe a chave do XOR para cifrar.\n");
        return 1;
    }

    if (cifrar) {
        n = sem_quebra_final(entrada, n);
    }
    cod = malloc(n > 0 ? n : 1);
    if (cod == NULL) {
        fprintf(stderr, "Erro: memória insuficiente.\n");
        return 1;
    }
    if (cifrar) {
        qtd = utf8_para_codigos(entrada, n, cod);
        if (qtd < 0) {
            fprintf(stderr, "Erro: o texto tem caracteres fora da faixa 0-255.\n");
            free(cod);
            return 1;
        }
    } else {
        qtd = preparar_codigos(entrada, n, cod);
    }
    if (qtd == 0) {
        fprintf(stderr, "Erro: o texto/arquivo está vazio.\n");
        free(cod);
        return 1;
    }

    int resultado = 0;
    if (cifrar) {
        for (long i = 0; i < qtd; i++) {
            escrever_utf8(cod[i] ^ chave[i % tam]);
        }
    } else if (tam > 0) {
        escrever_decifrado(cod, (size_t) qtd, chave, (size_t) tam);
    } else {
        resultado = busca_xor(cod, (size_t) qtd);
    }
    free(cod);
    return resultado;
}

/* ------------------------------------------------------------------ */
/* CAMADAS - descriptografia de varias cifras em sequencia.            */
/*                                                                     */
/* O texto cifrado e analisado, formula-se hipoteses para a camada     */
/* mais EXTERNA (Atbash, Cesar ou Vigenere), desfaz-se a camada, avalia-*/
/* se o resultado intermediario (trigramas de portugues e ingles,      */
/* indice de coincidencia) e repete-se enquanto houver evidencia de    */
/* outra camada. Uma busca em feixe mantem as hipoteses mais          */
/* promissoras em cada nivel; so se conclui quando o texto e "natural" */
/* acima de um limiar calibrado, senao o resultado e INCONCLUSIVO.     */
/*                                                                     */
/* Limite matematico: Cesar e Vigenere somam deslocamentos; Atbash     */
/* inverte o alfabeto. Qualquer sequencia dessas cifras equivale a UMA  */
/* transformacao "Vigenere (talvez seguido de Atbash)", com varias     */
/* decomposicoes igualmente corretas. A ordem e a divisao da chave NAO */
/* podem ser descobertas so pelo texto cifrado; a camada efetiva sim.  */
/* ------------------------------------------------------------------ */
typedef struct {
    int max_camadas;       /* quantas cifras em sequencia a busca aceita desfazer */
    int max_periodo;       /* maior chave de Vigenere, em letras */
    long tempo_ms;         /* tempo maximo da busca */
    clock_t inicio;
    int tempo_esgotado;
} Opcoes;

static int sem_tempo(Opcoes *op) {
    if ((long) ((clock() - op->inicio) * 1000 / CLOCKS_PER_SEC) > op->tempo_ms) {
        op->tempo_esgotado = 1;
    }
    return op->tempo_esgotado;
}

/* Uma cifra de letras: Atbash (reflexo) ou Vigenere de "tam" letras (tam 1 = Cesar). */
typedef struct {
    int reflexo;
    int tam;
    int desl[MAX_PERIODO];
} Camada;

static void desfazer_camada(const Camada *c, const unsigned char *origem, size_t n, unsigned char *destino) {
    if (c->reflexo) {
        for (size_t i = 0; i < n; i++) {
            destino[i] = (unsigned char) ('z' - (origem[i] - 'a'));
        }
    } else {
        vigenere_decifrar_buffer(origem, n, c->desl, c->tam, destino);
    }
}

/* Mesma conta do vigenere.c ao cifrar: letra + chave. So para texto a-z (pode ser no proprio buffer). */
static void vigenere_cifrar_buffer(const unsigned char *origem, size_t n, const int *desl, int tam,
                                   unsigned char *destino) {
    for (size_t i = 0; i < n; i++) {
        destino[i] = (unsigned char) ('a' + ((origem[i] - 'a') + desl[i % (size_t) tam]) % 26);
    }
}

/*
 * Limiar de "texto natural": media de trigramas (nats por letra, melhor idioma) que um texto
 * precisa atingir para ser aceito. Depende do tamanho porque textos curtos tem estatistica
 * ruidosa. Calibrado por simulacao (node tests/calibracao.js, 450 a 600 textos por tamanho): cada valor
 * e o maximo que a MESMA busca conseguiu arrancar de textos que NAO devem ser aceitos (letras aleatorias,
 * Vigenere com chave de 12 e 20 letras, substituicao monoalfabetica, texto natural embaralhado) mais 0,3.
 * Textos reais em claro ficam em media em -2,2 (desvio 0,1). Abaixo de MIN_LETRAS os dois grupos se
 * sobrepoem (a busca "se ajusta" ate a letras aleatorias): nenhum texto assim e aceito.
 */
static double limiar_forcado = -HUGE_VAL;    /* --limiar: so para calibrar/testar (substitui a tabela) */

static double limiar_natural(size_t n) {
    static const struct { size_t n; double limiar; } TABELA[] = {
        { 60, -2.66 }, { 80, -3.04 }, { 100, -3.17 }, { 130, -3.28 }, { 166, -3.48 }, { 250, -3.50 }, { 500, -3.55 },
    };
    const int q = (int) (sizeof TABELA / sizeof TABELA[0]);

    if (limiar_forcado > -HUGE_VAL) {
        return limiar_forcado;
    }
    if (n < MIN_LETRAS) {
        return 99.0;
    }
    if (n <= TABELA[0].n) {
        return TABELA[0].limiar;
    }
    for (int i = 1; i < q; i++) {
        if (n <= TABELA[i].n) {
            double f = (double) (n - TABELA[i - 1].n) / (double) (TABELA[i].n - TABELA[i - 1].n);

            return TABELA[i - 1].limiar + f * (TABELA[i].limiar - TABELA[i - 1].limiar);
        }
    }
    return TABELA[q - 1].limiar;
}

/* Um texto candidato com a sequencia de camadas que o produziu (na ordem em que foram DESFEITAS). */
typedef struct {
    Camada camadas[MAX_CAMADAS];
    int qtd;
    unsigned char *texto;
    size_t n;
    double pontos;       /* pontuacao() */
    double media;        /* trigramas, nats por letra, melhor idioma */
    double ic;
    int idioma;
} Hipotese;

static void hip_liberar(Hipotese *h) {
    free(h->texto);
    h->texto = NULL;
}

static void hip_avaliar(Hipotese *h) {
    h->pontos = pontuacao(h->texto, h->n);
    h->media = media_trigramas(h->texto, h->n, &h->idioma);
    h->ic = indice_coincidencia(h->texto, h->n);
}

static int hip_natural(const Hipotese *h) {
    return h->n >= MIN_LETRAS && h->media >= limiar_natural(h->n);
}

static int hip_copiar(const Hipotese *origem, Hipotese *destino) {
    *destino = *origem;
    destino->texto = malloc(origem->n > 0 ? origem->n : 1);
    if (destino->texto == NULL) {
        return 0;
    }
    memcpy(destino->texto, origem->texto, origem->n);
    return 1;
}

/* Hipotese filha: desfaz mais uma camada do texto do pai. */
static int hip_derivar(const Hipotese *pai, const Camada *camada, Hipotese *filho) {
    filho->texto = malloc(pai->n > 0 ? pai->n : 1);
    if (filho->texto == NULL) {
        return 0;
    }
    desfazer_camada(camada, pai->texto, pai->n, filho->texto);
    filho->n = pai->n;
    memcpy(filho->camadas, pai->camadas, sizeof(Camada) * (size_t) pai->qtd);
    filho->camadas[pai->qtd] = *camada;
    filho->qtd = pai->qtd + 1;
    hip_avaliar(filho);
    return 1;
}

static uint64_t hash_texto(const unsigned char *texto, size_t n) {
    uint64_t h = 1469598103934665603ULL;

    for (size_t i = 0; i < n; i++) {
        h = (h ^ texto[i]) * 1099511628211ULL;
    }
    return h;
}

#define MAX_FILHOS 12
#define MAX_ALTERNATIVAS 4

/*
 * Hipoteses de deslocamento (Cesar ou Vigenere) para o texto de "base": os 3 melhores deslocamentos
 * de Cesar e a chave de Vigenere de 2 letras ou mais que melhor ajusta o texto (mais, se houver, o
 * melhor outro tamanho de chave). Retorna quantas hipoteses foram escritas em "saida".
 */
static int gerar_deslocamentos(const Hipotese *base, Opcoes *op, Hipotese *saida, int cap) {
    int q = 0;
    unsigned char *tmp = malloc(base->n > 0 ? base->n : 1);
    double pontos[26];
    int usado[26] = {0};
    Camada camada;

    if (tmp == NULL || base->qtd >= MAX_CAMADAS) {
        free(tmp);
        return 0;
    }
    memset(&camada, 0, sizeof camada);
    camada.tam = 1;
    for (int d = 1; d <= 25; d++) {                /* Cesar: os 25 deslocamentos, ficam os 3 melhores */
        camada.desl[0] = d;
        desfazer_camada(&camada, base->texto, base->n, tmp);
        pontos[d] = pontuacao(tmp, base->n);
    }
    free(tmp);
    for (int k = 0; k < 3; k++) {
        int melhor = 0;

        for (int d = 1; d <= 25; d++) {
            if (!usado[d] && (melhor == 0 || pontos[d] > pontos[melhor])) {
                melhor = d;
            }
        }
        usado[melhor] = 1;
        camada.desl[0] = melhor;
        if (q < cap && hip_derivar(base, &camada, &saida[q])) {
            q++;
        }
    }

    /* Vigenere de 2 letras ou mais. Texto com IC de texto natural so pode ser monoalfabetico
       (ou ja estar em claro): a busca polialfabetica seria desperdicio. */
    if (op->max_periodo >= 2 && !(base->n >= 80 && base->ic >= 0.074) && !sem_tempo(op)) {
        ChaveVigenere por_tamanho[MAX_PERIODO + 1], escolhida;

        if (buscar_vigenere(base->texto, base->n, &escolhida, por_tamanho, op->max_periodo) > 0) {
            int segundo = 0;

            for (int tam = 2; tam <= op->max_periodo && (size_t) tam <= base->n; tam++) {
                if (tam != escolhida.tam && (segundo == 0 || por_tamanho[tam].pontos > por_tamanho[segundo].pontos)) {
                    segundo = tam;
                }
            }
            for (int v = 0; v < 2; v++) {
                const ChaveVigenere *k = v == 0 ? &escolhida : (segundo > 0 ? &por_tamanho[segundo] : NULL);

                if (k == NULL || k->tam < 2) {
                    continue;
                }
                camada.reflexo = 0;
                camada.tam = k->tam;
                memcpy(camada.desl, k->desl, sizeof(int) * (size_t) k->tam);
                if (q < cap && hip_derivar(base, &camada, &saida[q])) {
                    q++;
                }
            }
        }
    }
    return q;
}

/*
 * Hipoteses para a proxima camada a desfazer. Duas cifras de deslocamento seguidas se fundem em uma so
 * (de periodo igual ao mmc), que a busca de Vigenere do proprio pai ja cobre; por isso, depois de uma
 * camada de deslocamento so vale tentar o Atbash. O Atbash nao tem chave e, sozinho, quase nunca deixa
 * o texto parecer melhor (so depois do deslocamento que vem junto), entao ele e expandido JUNTO com os
 * deslocamentos que o seguem: o filho "so Atbash" e os netos "Atbash e depois deslocamento".
 */
static int gerar_filhos(const Hipotese *pai, Opcoes *op, Hipotese *filhos, int cap) {
    int q = 0;
    int ultimo_reflexo = pai->qtd > 0 && pai->camadas[pai->qtd - 1].reflexo;
    int ultimo_deslocamento = pai->qtd > 0 && !pai->camadas[pai->qtd - 1].reflexo;
    Camada atbash;

    memset(&atbash, 0, sizeof atbash);
    atbash.reflexo = 1;
    atbash.tam = 1;
    if (!ultimo_reflexo && pai->qtd < MAX_CAMADAS && hip_derivar(pai, &atbash, &filhos[q])) {
        q++;
        if (!ultimo_deslocamento && q == 1) {
            q += gerar_deslocamentos(&filhos[0], op, filhos + q, cap - q);
        }
    }
    if (!ultimo_deslocamento) {
        q += gerar_deslocamentos(pai, op, filhos + q, cap - q);
    }
    return q;
}

typedef struct {
    int conclusivo;
    int tempo_esgotado;
    int avaliadas;
    Hipotese escolhida;                      /* a conclusiva, ou a melhor tentativa */
    Hipotese alternativas[MAX_ALTERNATIVAS]; /* outras hipoteses promissoras, da melhor para a pior */
    int qtd_alt;
} Resultado;

static void liberar_resultado(Resultado *r) {
    hip_liberar(&r->escolhida);
    for (int i = 0; i < r->qtd_alt; i++) {
        hip_liberar(&r->alternativas[i]);
    }
    r->qtd_alt = 0;
}

/* Guarda h entre as melhores alternativas (por media), sem repetir texto. */
static void registrar_alternativa(Resultado *r, const Hipotese *h) {
    uint64_t id = hash_texto(h->texto, h->n);
    int pos = r->qtd_alt;

    for (int i = 0; i < r->qtd_alt; i++) {
        if (hash_texto(r->alternativas[i].texto, r->alternativas[i].n) == id) {
            return;
        }
    }
    if (pos == MAX_ALTERNATIVAS) {
        if (h->media <= r->alternativas[pos - 1].media) {
            return;
        }
        hip_liberar(&r->alternativas[--pos]);
    } else {
        r->qtd_alt++;
    }
    while (pos > 0 && r->alternativas[pos - 1].media < h->media) {
        r->alternativas[pos] = r->alternativas[pos - 1];
        pos--;
    }
    if (!hip_copiar(h, &r->alternativas[pos])) {
        memmove(&r->alternativas[pos], &r->alternativas[pos + 1], sizeof(Hipotese) * (size_t) (r->qtd_alt - pos - 1));
        r->qtd_alt--;
    }
}

/*
 * Busca em feixe sobre as camadas externas. Nivel 0: o proprio texto (se ja e natural, nao ha
 * cifra). Nivel k: cada hipotese do feixe gera filhos (uma camada a mais desfeita); filho que nao
 * melhora o texto em GANHO_MINIMO e descartado; se algum filho e natural a busca termina; senao
 * ficam as LARGURA_FEIXE melhores e o processo se repete, ate max_camadas ou o tempo acabar.
 */
static int resolver_camadas(const unsigned char *y, size_t n, Opcoes *op, Resultado *res) {
    Hipotese feixe[LARGURA_FEIXE], novos[LARGURA_FEIXE * MAX_FILHOS];
    uint64_t vistos[512];
    int qf = 0, qv = 0;
    Hipotese raiz;

    memset(res, 0, sizeof *res);
    raiz.qtd = 0;
    raiz.n = n;
    raiz.texto = malloc(n > 0 ? n : 1);
    if (raiz.texto == NULL) {
        return 0;
    }
    memcpy(raiz.texto, y, n);
    hip_avaliar(&raiz);
    vistos[qv++] = hash_texto(raiz.texto, raiz.n);
    if (!hip_copiar(&raiz, &res->escolhida)) {
        hip_liberar(&raiz);
        return 0;
    }
    if (hip_natural(&raiz)) {
        res->conclusivo = 1;
        hip_liberar(&raiz);
        return 1;
    }
    if (n < MIN_LETRAS) {          /* sem estatistica suficiente: procurar cifras so geraria texto enganoso */
        hip_liberar(&raiz);
        return 1;
    }
    feixe[qf++] = raiz;

    for (int nivel = 1; nivel <= op->max_camadas && nivel <= MAX_CAMADAS && !sem_tempo(op); nivel++) {
        int qn = 0, natural = -1;

        for (int b = 0; b < qf && !sem_tempo(op); b++) {
            Hipotese filhos[MAX_FILHOS];
            int qfi = feixe[b].qtd < MAX_CAMADAS ? gerar_filhos(&feixe[b], op, filhos, MAX_FILHOS) : 0;

            for (int f = 0; f < qfi; f++) {
                uint64_t id = hash_texto(filhos[f].texto, filhos[f].n);
                int repetido = 0;

                res->avaliadas++;
                for (int v = 0; v < qv; v++) {
                    repetido = repetido || vistos[v] == id;
                }
                if (repetido || qn >= LARGURA_FEIXE * MAX_FILHOS || filhos[f].qtd > op->max_camadas
                    || (filhos[f].media < feixe[b].media + GANHO_MINIMO && !hip_natural(&filhos[f]))) {
                    hip_liberar(&filhos[f]);
                    continue;
                }
                if (qv < 512) {
                    vistos[qv++] = id;
                }
                novos[qn++] = filhos[f];
            }
        }
        if (qn == 0) {
            break;
        }
        for (int i = 1; i < qn; i++) {            /* ordena por pontuacao, da maior para a menor */
            Hipotese atual = novos[i];
            int j = i - 1;

            while (j >= 0 && novos[j].pontos < atual.pontos) {
                novos[j + 1] = novos[j];
                j--;
            }
            novos[j + 1] = atual;
        }
        for (int i = 0; i < qn; i++) {
            registrar_alternativa(res, &novos[i]);
            if (hip_natural(&novos[i]) && (natural < 0 || novos[i].media > novos[natural].media)) {
                natural = i;
            }
        }
        for (int b = 0; b < qf; b++) {
            hip_liberar(&feixe[b]);
        }
        qf = 0;
        if (natural >= 0) {
            hip_liberar(&res->escolhida);
            res->escolhida = novos[natural];
            novos[natural].texto = NULL;
            res->conclusivo = 1;
        } else if (novos[0].media > res->escolhida.media) {
            hip_liberar(&res->escolhida);
            hip_copiar(&novos[0], &res->escolhida);
        }
        for (int i = 0; i < qn; i++) {
            if (natural < 0 && qf < LARGURA_FEIXE) {
                feixe[qf++] = novos[i];
            } else {
                hip_liberar(&novos[i]);
            }
        }
        if (natural >= 0) {
            break;
        }
    }
    for (int b = 0; b < qf; b++) {
        hip_liberar(&feixe[b]);
    }
    res->tempo_esgotado = op->tempo_esgotado;
    return 1;
}

/* ---------- camada efetiva e decomposicao ---------- */

#define MAX_EFETIVO 720

/*
 * Efeito total das camadas desfeitas, na forma canonica: desfazer = [Atbash] e depois subtrair a chave
 * k de "periodo" letras (periodo 1 = Cesar). Em ordem de CIFRAGEM: Vigenere(k) e, se reflexo, depois Atbash.
 */
typedef struct {
    int reflexo;
    int periodo;
    int k[MAX_EFETIVO];
} Efetivo;

static int mdc(int a, int b) {
    return b == 0 ? a : mdc(b, a % b);
}

static int calcular_efetivo(const Camada *camadas, int qtd, Efetivo *ef) {
    int t[MAX_EFETIVO], periodo = 1, sinal = 1;

    t[0] = 0;                    /* texto_claro = sinal * cifrado + t[posicao % periodo] */
    for (int c = 0; c < qtd; c++) {
        if (camadas[c].reflexo) {
            sinal = -sinal;
            for (int j = 0; j < periodo; j++) {
                t[j] = ((25 - t[j]) % 26 + 26) % 26;
            }
        } else {
            int tam = camadas[c].tam, novo = periodo / mdc(periodo, tam) * tam;

            if (novo > MAX_EFETIVO) {
                return 0;
            }
            for (int j = periodo; j < novo; j++) {
                t[j] = t[j % periodo];
            }
            for (int j = 0; j < novo; j++) {
                t[j] = ((t[j] - camadas[c].desl[j % tam]) % 26 + 26) % 26;
            }
            periodo = novo;
        }
    }
    for (int d = 1; d < periodo; d++) {          /* menor periodo que repete a chave */
        int serve = periodo % d == 0;

        for (int j = 0; serve && j < periodo; j++) {
            serve = t[j] == t[j % d];
        }
        if (serve) {
            periodo = d;
            break;
        }
    }
    ef->reflexo = sinal < 0;
    ef->periodo = periodo;
    for (int j = 0; j < periodo; j++) {
        ef->k[j] = sinal > 0 ? (26 - t[j]) % 26 : ((25 - t[j]) % 26 + 26) % 26;
    }
    return 1;
}

static int efetivo_sem_chave(const Efetivo *ef) {
    for (int j = 0; j < ef->periodo; j++) {
        if (ef->k[j] != 0) {
            return 0;
        }
    }
    return 1;
}

/*
 * Com Atbash e uma chave existem DUAS formas que produzem o MESMO texto cifrado:
 *   - "Vigenere(k) e depois Atbash"  (a forma de Efetivo: Atbash por ultimo);
 *   - "Atbash e depois Vigenere(-k)" (Atbash primeiro; a chave troca de sinal: abc vira azy).
 * Qual delas foi usada nao esta no texto cifrado. Estas funcoes tratam as duas.
 */
static void chave_atbash_primeiro(const Efetivo *ef, int *saida) {
    for (int j = 0; j < ef->periodo; j++) {
        saida[j] = (26 - ef->k[j]) % 26;
    }
}

static int soma_chave(const int *k, int tam) {
    int soma = 0;

    for (int j = 0; j < tam; j++) {
        soma += k[j];
    }
    return soma;
}

/*
 * Forma principal quando ha Atbash e chave (1 = Atbash primeiro). Criterio, sem pretender adivinhar o que o autor
 * fez: (1) chave de 4+ letras: a que mais parece uma senha/palavra pelos trigramas, se a diferenca for de pelo
 * menos 1 nat por letra; (2) senao, a de letras mais perto de "a" (abc em vez de azy). A outra forma e sempre
 * mostrada tambem.
 */
static int preferir_atbash_primeiro(const Efetivo *ef) {
    int negada[MAX_EFETIVO];

    if (!ef->reflexo || efetivo_sem_chave(ef)) {
        return 0;
    }
    chave_atbash_primeiro(ef, negada);
    if (ef->periodo >= 4 && ef->periodo <= MAX_PERIODO) {
        unsigned char a[MAX_PERIODO], b[MAX_PERIODO];
        double media_a, media_b;

        for (int j = 0; j < ef->periodo; j++) {
            a[j] = (unsigned char) ('a' + ef->k[j]);
            b[j] = (unsigned char) ('a' + negada[j]);
        }
        media_a = media_trigramas(a, (size_t) ef->periodo, NULL);
        media_b = media_trigramas(b, (size_t) ef->periodo, NULL);
        if (fabs(media_a - media_b) >= 1.0) {
            return media_b > media_a;
        }
    }
    return soma_chave(negada, ef->periodo) < soma_chave(ef->k, ef->periodo);
}

/* Cifra o texto claro com a camada efetiva, na forma pedida (ordem de cifragem). O buffer tem de ter n bytes. */
static void cifrar_efetivo(const Efetivo *ef, int atbash_primeiro, const unsigned char *claro, size_t n,
                           unsigned char *saida) {
    int negada[MAX_EFETIVO];
    const int *chave = ef->k;

    memcpy(saida, claro, n);
    if (atbash_primeiro && ef->reflexo) {
        chave_atbash_primeiro(ef, negada);
        chave = negada;
        for (size_t i = 0; i < n; i++) {
            saida[i] = (unsigned char) ('z' - (saida[i] - 'a'));
        }
    }
    if (!efetivo_sem_chave(ef)) {
        vigenere_cifrar_buffer(saida, n, chave, ef->periodo, saida);
    }
    if (ef->reflexo && !atbash_primeiro) {
        for (size_t i = 0; i < n; i++) {
            saida[i] = (unsigned char) ('z' - (saida[i] - 'a'));
        }
    }
}

/* Chave em letras ("def"), limitada a "cap" letras. */
static void chave_para_letras(const int *k, int tam, char *saida, size_t cap) {
    size_t q = (size_t) tam < cap - 1 ? (size_t) tam : cap - 1;

    for (size_t i = 0; i < q; i++) {
        saida[i] = (char) ('a' + k[i]);
    }
    saida[q] = '\0';
}

/* Nome da cifra de deslocamento para a interface: Cesar (1 letra) ou Vigenere. */
static const char *nome_deslocamento(int periodo) {
    return periodo == 1 ? "César" : "Vigenère";
}

/*
 * Decomposicao compativel em DUAS camadas (Cesar + Vigenere): como as duas somam deslocamentos, o
 * deslocamento comum a todas as letras da chave pode ser atribuido a uma Cesar. Convencao usada: o
 * Cesar leva o menor deslocamento da chave e a Vigenere fica com uma letra "a". Existem 26
 * decomposicoes igualmente corretas; esta nao e uma deteccao, e uma forma de apresentar.
 */
static int decompor(const Efetivo *ef, int *cesar, int *k_vigenere) {
    int menor = 25;

    if (ef->reflexo || ef->periodo < 2) {
        return 0;
    }
    for (int j = 0; j < ef->periodo; j++) {
        menor = ef->k[j] < menor ? ef->k[j] : menor;
    }
    if (menor == 0) {
        return 0;
    }
    *cesar = menor;
    for (int j = 0; j < ef->periodo; j++) {
        k_vigenere[j] = ef->k[j] - menor;
    }
    return 1;
}

/* Linha de passo da analise: metricas de um texto intermediario. */
static void emitir_passo(int numero, const char *acao, const unsigned char *texto, size_t n) {
    int idioma = 0;
    double media = media_trigramas(texto, n, &idioma), ic = indice_coincidencia(texto, n);
    int natural = n >= MIN_LETRAS && media >= limiar_natural(n);

    fprintf(stderr, "@passo %d | %s | %.4f | %.2f | %s | %s\n", numero, acao, ic, media, IDIOMAS[idioma].nome,
            natural ? "texto natural: fim" : "ainda parece cifrado: continua");
}

/* ---------- automatico (fases 3 e 4 e qualquer sequencia de cifras de letras) ---------- */

typedef struct {
    char nome[16];
    char chave[96];
} Etapa;

/* Etapas na ordem de CIFRAGEM e a camada efetiva em palavras. Retorna quantas etapas. */
static int listar_etapas(const Efetivo *ef, int atbash_primeiro, Etapa *etapas) {
    int q = 0, negada[MAX_EFETIVO];
    const int *chave = ef->k;

    if (atbash_primeiro && ef->reflexo) {
        chave_atbash_primeiro(ef, negada);
        chave = negada;
        snprintf(etapas[q].nome, sizeof etapas[q].nome, "Atbash");
        snprintf(etapas[q].chave, sizeof etapas[q].chave, "sem chave");
        q++;
    }
    if (!efetivo_sem_chave(ef)) {
        snprintf(etapas[q].nome, sizeof etapas[q].nome, "%s", nome_deslocamento(ef->periodo));
        if (ef->periodo == 1) {
            snprintf(etapas[q].chave, sizeof etapas[q].chave, "%d", chave[0]);
        } else {
            chave_para_letras(chave, ef->periodo, etapas[q].chave, sizeof etapas[q].chave);
        }
        q++;
    }
    if (ef->reflexo && !atbash_primeiro) {
        snprintf(etapas[q].nome, sizeof etapas[q].nome, "Atbash");
        snprintf(etapas[q].chave, sizeof etapas[q].chave, "sem chave");
        q++;
    }
    return q;
}

/* Prova real de uma forma: cifra o texto claro com ela (e o XOR, se houver) e compara com o arquivo recebido. */
static int conferir_forma(const Efetivo *ef, int atbash_primeiro, const Hipotese *h, int com_xor, const XorBusca *xr,
                          const unsigned char *cod, long qtd_cod, const unsigned char *entrada, size_t n) {
    unsigned char *conferir = malloc(h->n > 0 ? h->n : 1);
    int igual;

    if (conferir == NULL) {
        return 0;
    }
    cifrar_efetivo(ef, atbash_primeiro, h->texto, h->n, conferir);
    if (com_xor) {
        for (size_t i = 0; i < h->n; i++) {
            conferir[i] ^= xr->chave[i % xr->tam];
        }
        igual = (qtd_cod == (long) h->n || (qtd_cod == (long) h->n + 1 && cod[qtd_cod - 1] == '\n'))
                && memcmp(conferir, cod, h->n) == 0;
    } else {
        igual = n == h->n && memcmp(conferir, entrada, h->n) == 0;
    }
    free(conferir);
    return igual;
}

/* Maior pontuacao entre as chaves XOR candidatas: conclusivo antes de inconclusivo, depois a media. */
static int melhor_resultado(const Resultado *a, const Resultado *b) {
    if (a->conclusivo != b->conclusivo) {
        return a->conclusivo;
    }
    return a->escolhida.media >= b->escolhida.media;
}

/* Valor numerico de cada letra da chave, em decimal, hexadecimal e binario. */
static void emitir_valores_letras(const char *rotulo, const int *k, int tam) {
    for (int i = 0; i < tam; i++) {
        char nome[48];

        if (tam == 1) {
            snprintf(nome, sizeof nome, "%s deslocamento", rotulo);
        } else {
            snprintf(nome, sizeof nome, "%s letra %d (%c)", rotulo, i + 1, 'a' + k[i]);
        }
        emitir_valor(nome, k[i], 5);
    }
}

static void emitir_inconclusivo(const Resultado *r, size_t n, const Opcoes *op, int com_xor) {
    const char *motivo;
    char texto[256];

    if (n < MIN_LETRAS) {
        snprintf(texto, sizeof texto, "o texto tem só %zu letras; com menos de %d não há estatística para decidir", n, MIN_LETRAS);
        motivo = texto;
    } else if (r->tempo_esgotado) {
        motivo = "o tempo máximo da análise acabou antes de encontrar um texto natural";
    } else {
        snprintf(texto, sizeof texto,
                 "nenhuma sequência de até %d cifra(s) (Atbash, César, Vigenère com chave até %d letras%s) produziu texto "
                 "natural em português ou inglês; o texto pode ser curto, usar outra cifra ou uma chave maior",
                 op->max_camadas, op->max_periodo, com_xor ? ", depois de desfazer o XOR" : "");
        motivo = texto;
    }
    fprintf(stderr, "@status inconclusivo | %s\n", motivo);
    fprintf(stderr, "@pontuacao %.2f | %.2f | %zu\n", r->escolhida.media, n >= MIN_LETRAS ? limiar_natural(n) : 0.0, n);
    fprintf(stderr, "Não foi possível recuperar o texto com confiança. Nada foi apresentado como resultado "
                    "para não passar texto aleatório por descriptografia.\n");
    if (r->escolhida.qtd > 0) {
        emitir_opcao("melhor tentativa (baixa confiança, não é um resultado)", r->escolhida.texto, r->escolhida.n);
    }
}

static int executar_automatico(unsigned char *entrada, size_t n_bruto, Opcoes *op) {
    XorBusca alternativas[MAX_ALTERNATIVAS_XOR], xr;
    Resultado melhor, atual;
    int tem_melhor = 0, so_letras = 1, com_xor = 0, total_alt = 1, resultado = 1;
    size_t n = sem_quebra_final(entrada, n_bruto), nb = n;
    long qtd_cod = 0;
    unsigned char *cod = NULL, *base = entrada;
    char chave_xor[96] = "";

    memset(&xr, 0, sizeof xr);
    memset(&melhor, 0, sizeof melhor);
    op->inicio = clock();
    if (n == 0) {
        fprintf(stderr, "Erro: o texto/arquivo está vazio.\n");
        return 1;
    }
    for (size_t i = 0; i < n; i++) {
        if (entrada[i] < 'a' || entrada[i] > 'z') {
            so_letras = 0;
            break;
        }
    }

    if (!so_letras) {
        com_xor = 1;
        cod = malloc(n_bruto);
        base = malloc(n_bruto);
        if (cod == NULL || base == NULL) {
            fprintf(stderr, "Erro: memória insuficiente.\n");
            goto fim;
        }
        qtd_cod = preparar_codigos(entrada, n_bruto, cod);
        total_alt = qtd_cod > 0 ? buscar_xor_automatico(cod, (size_t) qtd_cod, alternativas, MAX_ALTERNATIVAS_XOR) : 0;
        if (total_alt == 0) {
            fprintf(stderr, "Erro: o texto tem caracteres que não são letras a-z e nenhuma chave XOR de 1 a %d bytes "
                            "os transforma em letras. O automático espera texto só com letras (Atbash, César, "
                            "Vigenère) ou um arquivo cifrado com XOR.\n", MAX_BYTES_BUSCA_XOR);
            goto fim;
        }
    }

    /* Cada chave XOR finalista e analisada por inteiro; vence a que der o texto mais natural. */
    for (int a = 0; a < total_alt; a++) {
        if (com_xor) {
            nb = xor_para_buffer(cod, (size_t) qtd_cod, alternativas[a].chave, alternativas[a].tam, base);
            if (nb == 0) {
                continue;
            }
        }
        if (!resolver_camadas(base, nb, op, &atual)) {
            fprintf(stderr, "Erro: memória insuficiente.\n");
            liberar_resultado(&atual);
            if (tem_melhor) {
                liberar_resultado(&melhor);
            }
            goto fim;
        }
        if (!tem_melhor || melhor_resultado(&atual, &melhor)) {
            if (tem_melhor) {
                liberar_resultado(&melhor);
            }
            melhor = atual;
            tem_melhor = 1;
            xr = alternativas[a];
        } else {
            liberar_resultado(&atual);
        }
        if (melhor.conclusivo && !com_xor) {
            break;
        }
    }
    if (!tem_melhor) {
        fprintf(stderr, "Erro: o texto/arquivo está vazio.\n");
        goto fim;
    }
    nb = melhor.escolhida.n;
    if (com_xor) {
        unsigned char ultimo = cod[qtd_cod - 1];

        /* "base" guarda o texto da ULTIMA chave avaliada; as evidencias e os passos usam a escolhida */
        nb = xor_para_buffer(cod, (size_t) qtd_cod, xr.chave, xr.tam, base);
        unsigned char letra = ultimo ^ xr.chave[(size_t) (qtd_cod - 1) % xr.tam];

        chave_xor_para_texto(xr.chave, xr.tam, chave_xor, sizeof chave_xor);
        if (ultimo == '\n' && letra >= 'a' && letra <= 'z') {
            fprintf(stderr, "Aviso: a quebra de linha no fim do arquivo foi ignorada (regulamento). "
                            "Se ela fosse um byte cifrado, a última letra seria '%c'.\n", letra);
        }
    }

    /* ---- evidencias sobre o texto que sobrou depois do XOR (ou o proprio texto) ---- */
    {
        double ic = indice_coincidencia(base, nb), ic_colunas = 0;
        int periodo = estimar_periodo_ic(base, nb, op->max_periodo, &ic_colunas);

        fprintf(stderr, "@evidencia IC do texto cifrado = %.4f (texto claro: %.4f em português, %.4f em inglês; letras "
                        "aleatórias: %.4f) → %s\n",
                ic, IDIOMAS[0].ic, IDIOMAS[1].ic, IC_ALEATORIO,
                ic >= 0.062 ? "compatível com texto claro ou cifra monoalfabética (César/Atbash)"
                            : "compatível com cifra polialfabética (Vigenère)");
        fprintf(stderr, "@evidencia período estimado pelo IC das colunas = %d (IC médio das colunas = %.4f)\n",
                periodo, ic_colunas);
    }

    if (!melhor.conclusivo || nb < MIN_LETRAS) {
        emitir_inconclusivo(&melhor, nb, op, com_xor);
        resultado = 0;
        liberar_resultado(&melhor);
        goto fim;
    }

    {
        const Hipotese *h = &melhor.escolhida;
        Efetivo ef;
        Etapa etapas[3];
        Etapa etapas_alt[3];
        int qtd_etapas, qtd_alt_etapas = 0, cesar = 0, k_vig[MAX_EFETIVO], decomposto, primeiro, tem_forma_alt;
        char texto_cifras[160] = "", texto_chaves[256] = "", letras[96];
        int verificado = 0, verificado_alt = 0;
        int chave_forma[MAX_EFETIVO];

        if (!calcular_efetivo(h->camadas, h->qtd, &ef)) {
            fprintf(stderr, "Erro: não foi possível resumir as camadas encontradas.\n");
            liberar_resultado(&melhor);
            goto fim;
        }
        decomposto = ef.periodo <= MAX_PERIODO && decompor(&ef, &cesar, k_vig);
        primeiro = preferir_atbash_primeiro(&ef);
        tem_forma_alt = ef.reflexo && !efetivo_sem_chave(&ef);
        qtd_etapas = listar_etapas(&ef, primeiro, etapas);
        if (primeiro) {
            chave_atbash_primeiro(&ef, chave_forma);
        } else {
            memcpy(chave_forma, ef.k, sizeof(int) * (size_t) ef.periodo);
        }

        /* prova real, de cada forma: cifrar o texto claro com ela (+ XOR) e comparar com o arquivo */
        verificado = conferir_forma(&ef, primeiro, h, com_xor, &xr, cod, qtd_cod, entrada, n);
        if (tem_forma_alt) {
            qtd_alt_etapas = listar_etapas(&ef, !primeiro, etapas_alt);
            verificado_alt = conferir_forma(&ef, !primeiro, h, com_xor, &xr, cod, qtd_cod, entrada, n);
        }

        for (int e = 0; e < qtd_etapas; e++) {
            size_t usado = strlen(texto_cifras), usado_chaves = strlen(texto_chaves);

            snprintf(texto_cifras + usado, sizeof texto_cifras - usado, "%s%.15s", e ? " → " : "", etapas[e].nome);
            snprintf(texto_chaves + usado_chaves, sizeof texto_chaves - usado_chaves, "%s%.15s: %.95s", e ? " · " : "",
                     etapas[e].nome, etapas[e].chave);
        }
        if (com_xor) {
            size_t usado = strlen(texto_cifras), usado_chaves = strlen(texto_chaves);

            snprintf(texto_cifras + usado, sizeof texto_cifras - usado, "%sXOR", qtd_etapas ? " → " : "");
            snprintf(texto_chaves + usado_chaves, sizeof texto_chaves - usado_chaves, "%sXOR: %.95s",
                     qtd_etapas ? " · " : "", chave_xor);
        }
        if (texto_cifras[0] == '\0') {
            snprintf(texto_cifras, sizeof texto_cifras, "Nenhuma (o texto já está em claro)");
            snprintf(texto_chaves, sizeof texto_chaves, "sem chave");
        }

        fwrite(h->texto, 1, h->n, stdout);
        {
            double limiar = limiar_natural(h->n), margem = h->media - limiar;

            fprintf(stderr, "@status conclusivo | %s\n", margem >= 0.6 ? "confiança alta" : margem >= 0.25 ? "confiança média" : "confiança baixa");
            fprintf(stderr, "@pontuacao %.2f | %.2f | %zu\n", h->media, limiar, h->n);
        }
        fprintf(stderr, "@idioma %s\n", IDIOMAS[h->idioma].nome);
        fprintf(stderr, "@cifras %s\n@chave %s\n", texto_cifras, texto_chaves);
        for (int e = 0; e < qtd_etapas; e++) {
            fprintf(stderr, "@etapa %s | %s\n", etapas[e].nome, etapas[e].chave);
        }
        if (com_xor) {
            fprintf(stderr, "@etapa XOR | %s\n", chave_xor);
        }
        fprintf(stderr, "@verificado %s\n", verificado ? "sim" : "nao");
        if (tem_forma_alt) {                  /* a outra forma, que reproduz o mesmo texto cifrado */
            for (int e = 0; e < qtd_alt_etapas; e++) {
                fprintf(stderr, "@etapaeq %s | %s\n", etapas_alt[e].nome, etapas_alt[e].chave);
            }
            if (com_xor) {
                fprintf(stderr, "@etapaeq XOR | %s\n", chave_xor);
            }
            fprintf(stderr, "@verificadoeq %s\n", verificado_alt ? "sim" : "nao");
        }

        /* ---- passos da descriptografia, na ordem inversa da cifragem, com avaliacao de cada resultado ---- */
        emitir_passo(0, com_xor ? "texto depois de desfazer o XOR" : "texto cifrado (entrada)", base, nb);
        {
            unsigned char *passo = malloc(h->n > 0 ? h->n : 1), *proximo = malloc(h->n > 0 ? h->n : 1);
            Camada plano[3];
            int qtd_plano = 0;

            if (passo != NULL && proximo != NULL && ef.periodo <= MAX_PERIODO) {
                memcpy(passo, base, nb);
                memset(plano, 0, sizeof plano);
                if (decomposto) {         /* Vigenere (externa) e depois Cesar (interna) */
                    plano[0].tam = ef.periodo;
                    memcpy(plano[0].desl, k_vig, sizeof(int) * (size_t) ef.periodo);
                    plano[1].tam = 1;
                    plano[1].desl[0] = cesar;
                    qtd_plano = 2;
                } else {                  /* ordem inversa da cifragem, na forma principal */
                    if (ef.reflexo && !primeiro) {          /* cifrou V e depois Atbash: desfaz Atbash primeiro */
                        plano[qtd_plano].reflexo = 1;
                        plano[qtd_plano++].tam = 1;
                    }
                    if (!efetivo_sem_chave(&ef)) {
                        plano[qtd_plano].tam = ef.periodo;
                        memcpy(plano[qtd_plano].desl, chave_forma, sizeof(int) * (size_t) ef.periodo);
                        qtd_plano++;
                    }
                    if (ef.reflexo && primeiro) {           /* cifrou Atbash e depois V: desfaz V primeiro */
                        plano[qtd_plano].reflexo = 1;
                        plano[qtd_plano++].tam = 1;
                    }
                }
                for (int p = 0; p < qtd_plano; p++) {
                    char acao[160], letras_plano[MAX_PERIODO + 1];

                    desfazer_camada(&plano[p], passo, nb, proximo);
                    memcpy(passo, proximo, nb);
                    if (plano[p].reflexo) {
                        snprintf(acao, sizeof acao, "desfazer Atbash");
                    } else if (plano[p].tam == 1) {
                        snprintf(acao, sizeof acao, "desfazer César (deslocamento %d)%s", plano[p].desl[0],
                                 decomposto ? ", camada interna" : "");
                    } else {
                        chave_para_letras(plano[p].desl, plano[p].tam, letras_plano, sizeof letras_plano);
                        snprintf(acao, sizeof acao, "desfazer Vigenère (chave %s)%s", letras_plano,
                                 decomposto ? ", camada externa" : "");
                    }
                    emitir_passo(p + 1, acao, passo, nb);
                }
            }
            free(passo);
            free(proximo);
        }

        if (decomposto) {
            chave_para_letras(k_vig, ef.periodo, letras, sizeof letras);
            fprintf(stderr, "@decomposicao César (deslocamento %d, camada interna, aplicada primeiro) → Vigenère (chave %s, camada "
                            "externa, aplicada depois)\n", cesar, letras);
            fprintf(stderr, "@nota Esta decomposição é só uma forma compatível de apresentar a chave: César e Vigenère somam "
                            "deslocamentos, então o texto cifrado é idêntico se a ordem for invertida ou se o deslocamento "
                            "for dividido de outro jeito (há 26 divisões equivalentes). O que o texto cifrado prova é a camada "
                            "efetiva: Vigenère com a chave %s.\n", etapas[0].chave);
        } else if (tem_forma_alt) {
            char chave_a[96], chave_b[96];
            int negada[MAX_EFETIVO];

            chave_para_letras(ef.k, ef.periodo, chave_a, sizeof chave_a);
            chave_atbash_primeiro(&ef, negada);
            chave_para_letras(negada, ef.periodo, chave_b, sizeof chave_b);
            if (ef.periodo == 1) {
                snprintf(chave_a, sizeof chave_a, "%d", ef.k[0]);
                snprintf(chave_b, sizeof chave_b, "%d", negada[0]);
            }
            fprintf(stderr, "@nota O texto cifrado não diz de que lado o Atbash foi aplicado: \"%s(%s) e depois Atbash\" e "
                            "\"Atbash e depois %s(%s)\" produzem exatamente o mesmo texto cifrado (a chave troca de sinal: "
                            "cada letra vira 26 menos o valor dela). As duas formas foram conferidas e estão listadas; a "
                            "principal é a de chave mais parecida com uma senha ou, na dúvida, com letras mais perto de \"a\".\n",
                    nome_deslocamento(ef.periodo), chave_a, nome_deslocamento(ef.periodo), chave_b);
        }

        if (ef.periodo >= 1 && !efetivo_sem_chave(&ef)) {
            emitir_valores_letras(nome_deslocamento(ef.periodo), chave_forma, ef.periodo);
        }
        if (com_xor) {
            emitir_valores_xor(xr.chave, xr.tam);
        }
        if (h->n < 20 * (size_t) (ef.periodo > 1 ? ef.periodo : 1)) {
            fprintf(stderr, "Aviso: o texto é curto para uma chave de %d letras; confira o resultado.\n", ef.periodo);
        }
        for (int i = 0; i < melhor.qtd_alt; i++) {
            char descricao[160];
            Efetivo ef_alt;
            const Hipotese *alt = &melhor.alternativas[i];

            if (hash_texto(alt->texto, alt->n) == hash_texto(h->texto, h->n) || !calcular_efetivo(alt->camadas, alt->qtd, &ef_alt)) {
                continue;
            }
            {
                Etapa e_alt[3];
                int q_alt = listar_etapas(&ef_alt, 0, e_alt);
                char *p = descricao;

                p += snprintf(descricao, sizeof descricao, "hipótese descartada");
                for (int e = 0; e < q_alt; e++) {
                    p += snprintf(p, sizeof descricao - (size_t) (p - descricao), "%s%s (%s)", e ? " → " : ": ", e_alt[e].nome, e_alt[e].chave);
                }
                snprintf(p, sizeof descricao - (size_t) (p - descricao), " · média %.2f", alt->media);
            }
            emitir_opcao(descricao, alt->texto, alt->n);
        }
        fprintf(stderr, "Foram avaliadas %d hipóteses de camada (Atbash, César e Vigenère até %d letras), até %d camadas.\n",
                melhor.avaliadas, op->max_periodo, op->max_camadas);
    }
    liberar_resultado(&melhor);
    resultado = 0;

fim:
    free(cod);
    if (com_xor) {
        free(base);
    }
    return resultado;
}

/* Opcao "--nome valor" da linha de comando, limitada a [minimo, maximo]. Retorna 0 se o valor e invalido. */
static int ler_opcao(const char *valor, long minimo, long maximo, long *destino) {
    long v;

    if (valor == NULL || !ler_numero(valor, &v) || v < minimo || v > maximo) {
        return 0;
    }
    *destino = v;
    return 1;
}

/* Modo "avaliar": so mede o texto (idioma, media de trigramas, IC e limiar), sem procurar cifras. */
static int avaliar_texto(unsigned char *texto, size_t n) {
    int idioma = 0;
    double media, ic;

    n = sem_quebra_final(texto, n);
    for (size_t i = 0; i < n; i++) {
        if (texto[i] < 'a' || texto[i] > 'z') {
            fprintf(stderr, "Erro: o modo avaliar espera só letras de a a z.\n");
            return 1;
        }
    }
    media = media_trigramas(texto, n, &idioma);
    ic = indice_coincidencia(texto, n);
    printf("@avaliacao media=%.4f ic=%.4f idioma=%s limiar=%.4f natural=%s letras=%zu\n", media, ic, IDIOMAS[idioma].nome,
           limiar_natural(n), n >= MIN_LETRAS && media >= limiar_natural(n) ? "sim" : "nao", n);
    return 0;
}

int main(int argc, char *argv[]) {
    const char *posicionais[3] = { NULL, NULL, NULL };
    int qtd_pos = 0;
    Opcoes op = { PADRAO_MAX_CAMADAS, PADRAO_MAX_PERIODO, PADRAO_TEMPO_MS, 0, 0 };

    for (int i = 1; i < argc; i++) {
        long valor = 0;

        if (strcmp(argv[i], "--max-camadas") == 0 && ler_opcao(i + 1 < argc ? argv[i + 1] : NULL, 1, MAX_CAMADAS, &valor)) {
            op.max_camadas = (int) valor;
            i++;
        } else if (strcmp(argv[i], "--max-periodo") == 0 && ler_opcao(i + 1 < argc ? argv[i + 1] : NULL, 1, MAX_PERIODO, &valor)) {
            op.max_periodo = (int) valor;
            i++;
        } else if (strcmp(argv[i], "--tempo-ms") == 0 && ler_opcao(i + 1 < argc ? argv[i + 1] : NULL, 100, 600000, &valor)) {
            op.tempo_ms = valor;
            i++;
        } else if (strcmp(argv[i], "--limiar") == 0 && i + 1 < argc) {
            limiar_forcado = strtod(argv[i + 1], NULL);
            i++;
        } else if (strncmp(argv[i], "--", 2) == 0 && argv[i][2] != '\0') {
            fprintf(stderr, "Erro: opção inválida ou sem valor válido: %s\n", argv[i]);
            return 2;
        } else if (qtd_pos < 3) {
            posicionais[qtd_pos++] = argv[i];
        } else {
            qtd_pos = 4;
        }
    }
    if (qtd_pos < 2 || qtd_pos > 3) {
        fprintf(stderr, "Uso: %s <cesar|vigenere|atbash|xor|auto> <cifrar|decifrar> [chave] "
                        "[--max-camadas 1-%d] [--max-periodo 1-%d] [--tempo-ms ms]\n", argv[0], MAX_CAMADAS, MAX_PERIODO);
        return 2;
    }

    const char *algoritmo = posicionais[0];
    const char *modo = posicionais[1];
    const char *chave = qtd_pos == 3 ? posicionais[2] : "";
    int cifrar;

    if (strcmp(modo, "cifrar") == 0) {
        cifrar = 1;
    } else if (strcmp(modo, "decifrar") == 0) {
        cifrar = 0;
    } else {
        fprintf(stderr, "Erro: modo inválido '%s'.\n", modo);
        return 2;
    }

#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);    /* sem isso o Windows troca \n por \r\n e estraga o XOR */
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    size_t n;
    unsigned char *texto = ler_entrada(&n);
    if (texto == NULL) {
        fprintf(stderr, "Erro: não foi possível ler a entrada.\n");
        return 1;
    }

    int resultado;
    preparar_frequencias();
    if (strcmp(algoritmo, "cesar") == 0) {
        resultado = executar_cesar(texto, n, cifrar, chave);
    } else if (strcmp(algoritmo, "vigenere") == 0) {
        resultado = executar_vigenere(texto, n, cifrar, chave);
    } else if (strcmp(algoritmo, "atbash") == 0) {
        resultado = executar_atbash(texto, n, chave);
    } else if (strcmp(algoritmo, "xor") == 0) {
        resultado = executar_xor(texto, n, cifrar, chave);
    } else if (strcmp(algoritmo, "avaliar") == 0) {
        resultado = avaliar_texto(texto, n);
    } else if (strcmp(algoritmo, "auto") == 0) {
        if (cifrar) {
            fprintf(stderr, "Erro: o modo automático só descriptografa.\n");
            resultado = 2;
        } else {
            resultado = executar_automatico(texto, n, &op);
        }
    } else {
        fprintf(stderr, "Erro: algoritmo inválido '%s'.\n", algoritmo);
        resultado = 2;
    }

    free(texto);
    return resultado;
}
