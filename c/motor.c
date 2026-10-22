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

#define MAX_CHAVE_XOR 16
#define MAX_BYTES_BUSCA_XOR 5     /* 40 bits: maior orcamento do torneio */
#define TOLERANCIA_TAMANHO 0.02   /* ver buscar_chave_xor() */
#define LIMIAR_PORTUGUES -3.1     /* portugues fica perto de -2.7 */
#define MAX_CHAVE_VIGENERE 8      /* maximo de letras permitido no regulamento */
#define TOLERANCIA_VIGENERE 0.03  /* ver buscar_vigenere() */
#define TOLERANCIA_SIMPLES 0.01   /* ver executar_automatico() */
#define PESO_PALAVRA 1.0          /* pontos por letra que faz parte de uma palavra comum */
#define MAX_PONTUADO 4096
#define PREVIA 70                 /* caracteres mostrados em cada possibilidade */
#define MAX_CANDIDATOS 4
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

/* Media do log da frequencia das letras: quanto maior, mais "portugues". */
static double pontuar(const unsigned char *texto, size_t n) {
    double total = 0;
    size_t letras = 0;

    for (size_t i = 0; i < n; i++) {
        if (texto[i] >= 'a' && texto[i] <= 'z') {
            total += log_frequencia[texto[i] - 'a'];
            letras++;
        }
    }
    return letras > 0 ? total / (double) letras : -100.0;
}

/*
 * Pontuacao total do texto: soma do log da frequencia de cada letra mais
 * PESO_PALAVRA por letra que pertence a uma palavra comum do portugues.
 * Texto sem sentido (chave errada) pontua bem menos que o texto claro.
 */
static double pontuacao(const unsigned char *texto, size_t n) {
    static unsigned char coberto[MAX_PONTUADO];
    size_t m = n < MAX_PONTUADO ? n : MAX_PONTUADO;
    size_t cobertos = 0;
    double total = 0;

    for (size_t i = 0; i < n; i++) {
        if (texto[i] >= 'a' && texto[i] <= 'z') {
            total += log_frequencia[texto[i] - 'a'];
        }
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
    return total + PESO_PALAVRA * (double) cobertos;
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
    if (pontuar(texto, n) < LIMIAR_PORTUGUES) {
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
    int desl[MAX_CHAVE_VIGENERE];   /* 0 a 25: a=0, b=1, ... z=25 */
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
 * Testa todas as chaves de 1 a 8 letras. Para cada tamanho, cada letra da
 * chave e uma "coluna" do texto: os 26 deslocamentos de cada coluna sao
 * testados e os 2 melhores (pela frequencia das letras) entram nas
 * combinacoes, que sao decididas pela pontuacao do texto inteiro.
 * Chaves maiores sempre se ajustam um pouco melhor, entao fica o menor
 * tamanho que chega perto da melhor pontuacao.
 * Retorna quantos tamanhos foram testados (0 se faltar memoria).
 */
static int buscar_vigenere(const unsigned char *texto, size_t n, ChaveVigenere *escolhida,
                           ChaveVigenere por_tamanho[]) {
    unsigned char *tentativa = malloc(n > 0 ? n : 1);
    double melhor_geral = -HUGE_VAL;
    int max_tam = n < MAX_CHAVE_VIGENERE ? (int) n : MAX_CHAVE_VIGENERE;

    if (tentativa == NULL) {
        return 0;
    }
    for (int tam = 1; tam <= max_tam; tam++) {
        int primeiro[MAX_CHAVE_VIGENERE], segundo[MAX_CHAVE_VIGENERE];
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
        for (unsigned mascara = 0; mascara < (1u << tam); mascara++) {
            int desl[MAX_CHAVE_VIGENERE];
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
    max_tam = buscar_vigenere(texto, n, &escolhida, por_tamanho);
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
/* AUTOMATICO - fases 3 e 4: ate duas cifras diferentes, XOR sempre a  */
/* ultima. Desfaz o XOR (se houver) e testa o que sobra.               */
/*                                                                     */
/* Atbash, Cesar e Vigenere se reduzem a poucos casos:                 */
/*   - Cesar e um Vigenere de 1 letra;                                 */
/*   - Cesar + Vigenere e Vigenere com a chave deslocada;              */
/*   - Atbash + Cesar/Vigenere equivale a Cesar/Vigenere seguido de    */
/*     Atbash (com a chave trocada de sinal).                          */
/* Entao basta testar o texto como esta e invertido por Atbash, cada   */
/* um com a busca de Vigenere (1 a 8 letras).                          */
/* ------------------------------------------------------------------ */
typedef struct {
    double pontos;
    int complexidade;                /* menos cifras = menor */
    char cifras[96];                 /* vazio = nenhuma cifra alem do XOR */
    char chaves[128];
    int tam_vigenere;                /* 0 = sem Vigenere/Cesar */
    int desl[MAX_CHAVE_VIGENERE];
    unsigned char *texto;
    size_t n;
} Candidato;

static int novo_candidato(Candidato *c, const unsigned char *texto, size_t n, int complexidade,
                          const char *cifras, const char *chaves) {
    c->texto = malloc(n > 0 ? n : 1);
    if (c->texto == NULL) {
        return 0;
    }
    memcpy(c->texto, texto, n);
    c->n = n;
    c->pontos = pontuacao(texto, n);
    c->complexidade = complexidade;
    c->tam_vigenere = 0;
    snprintf(c->cifras, sizeof c->cifras, "%s", cifras);
    snprintf(c->chaves, sizeof c->chaves, "%s", chaves);
    return 1;
}

static void liberar_candidatos(Candidato *lista, int qtd) {
    for (int i = 0; i < qtd; i++) {
        free(lista[i].texto);
    }
}

/* Candidatos: texto como esta, Atbash, Vigenere/Cesar e Vigenere/Cesar + Atbash
   (esta ultima so sem XOR: com o XOR so cabe mais uma cifra). */
static int analisar_letras(const unsigned char *y, size_t n, int com_xor, Candidato *lista, int *qtd) {
    unsigned char *invertido = malloc(n > 0 ? n : 1);
    unsigned char *tentativa = malloc(n > 0 ? n : 1);
    int ok = invertido != NULL && tentativa != NULL;

    *qtd = 0;
    if (ok) {
        for (size_t i = 0; i < n; i++) {
            invertido[i] = (unsigned char) ('z' - (y[i] - 'a'));
        }
        ok = novo_candidato(&lista[*qtd], y, n, 0, "", "");
        (*qtd) += ok;
    }
    if (ok) {
        ok = novo_candidato(&lista[*qtd], invertido, n, 1, "Atbash", "Atbash: sem chave");
        (*qtd) += ok;
    }
    for (int perfil = 0; ok && perfil < (com_xor ? 1 : 2); perfil++) {
        const unsigned char *base = perfil ? invertido : y;
        ChaveVigenere por_tamanho[MAX_CHAVE_VIGENERE + 1], k;
        char nome[16], valor[MAX_CHAVE_VIGENERE + 1], cifras[96], chaves[128];
        int todas_a = 1;

        if (!buscar_vigenere(base, n, &k, por_tamanho)) {
            ok = 0;
            break;
        }
        for (int i = 0; i < k.tam; i++) {
            todas_a = todas_a && k.desl[i] == 0;
        }
        if (todas_a) {
            continue;       /* chave "a" = nenhuma cifra */
        }
        vigenere_decifrar_buffer(base, n, k.desl, k.tam, tentativa);
        if (k.tam == 1) {
            snprintf(nome, sizeof nome, "César");
            snprintf(valor, sizeof valor, "%d", k.desl[0]);
        } else {
            snprintf(nome, sizeof nome, "Vigenère");
            vigenere_para_texto(&k, valor);
        }
        snprintf(cifras, sizeof cifras, perfil ? "%s → Atbash" : "%s", nome);
        snprintf(chaves, sizeof chaves, perfil ? "%s: %s · Atbash: sem chave" : "%s: %s", nome, valor);
        ok = novo_candidato(&lista[*qtd], tentativa, n, (perfil ? 3 : 2) + (k.tam > 1), cifras, chaves);
        if (ok) {
            lista[*qtd].tam_vigenere = k.tam;
            memcpy(lista[*qtd].desl, k.desl, sizeof(int) * (size_t) k.tam);
            (*qtd)++;
        }
    }
    free(invertido);
    free(tentativa);
    return ok;
}

/* Junta as cifras da camada de letras com o XOR (sempre a ultima). */
static void montar_cadeia(const Candidato *c, int com_xor, const char *chave_xor,
                          char *cifras, size_t cap_cifras, char *chaves, size_t cap_chaves) {
    if (c->cifras[0] == '\0') {
        snprintf(cifras, cap_cifras, "%s", com_xor ? "XOR" : "Nenhuma (o texto já está em claro)");
        snprintf(chaves, cap_chaves, "%s%s", com_xor ? "XOR: " : "sem chave", com_xor ? chave_xor : "");
    } else if (com_xor) {
        snprintf(cifras, cap_cifras, "%s → XOR", c->cifras);
        snprintf(chaves, cap_chaves, "%s · XOR: %s", c->chaves, chave_xor);
    } else {
        snprintf(cifras, cap_cifras, "%s", c->cifras);
        snprintf(chaves, cap_chaves, "%s", c->chaves);
    }
}

/* Maior pontuacao; se uma cifra mais simples chega perto, prefere a simples. */
static int escolher_candidato(const Candidato *lista, int qtd, size_t n) {
    int melhor = 0, escolhido;

    for (int i = 0; i < qtd; i++) {
        if (lista[i].pontos > lista[melhor].pontos) {
            melhor = i;
        }
    }
    escolhido = melhor;
    for (int i = 0; i < qtd; i++) {
        if (lista[i].pontos >= lista[melhor].pontos - TOLERANCIA_SIMPLES * (double) n
            && lista[i].complexidade < lista[escolhido].complexidade) {
            escolhido = i;
        }
    }
    return escolhido;
}

static int executar_automatico(unsigned char *entrada, size_t n_bruto) {
    Candidato lista[MAX_CANDIDATOS];
    XorBusca alternativas[MAX_ALTERNATIVAS_XOR], xr;
    int qtd = 0, escolhido = 0, ordem[MAX_CANDIDATOS];
    int so_letras = 1, com_xor = 0, total_alt = 1, resultado = 1;
    size_t n = sem_quebra_final(entrada, n_bruto), nb = n;
    long qtd_cod = 0;
    unsigned char *cod = NULL, *base = entrada;
    char chave_xor[96] = "";

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

    /* Cada chave XOR finalista e analisada por inteiro; vence a que der o texto mais portugues. */
    for (int a = 0; a < total_alt; a++) {
        Candidato atual[MAX_CANDIDATOS];
        int qtd_atual = 0, idx;

        if (com_xor) {
            nb = xor_para_buffer(cod, (size_t) qtd_cod, alternativas[a].chave, alternativas[a].tam, base);
            if (nb == 0) {
                continue;
            }
        }
        if (!analisar_letras(base, nb, com_xor, atual, &qtd_atual)) {
            fprintf(stderr, "Erro: memória insuficiente.\n");
            liberar_candidatos(atual, qtd_atual);
            liberar_candidatos(lista, qtd);
            goto fim;
        }
        idx = escolher_candidato(atual, qtd_atual, nb);
        if (qtd == 0 || atual[idx].pontos > lista[escolhido].pontos) {
            liberar_candidatos(lista, qtd);
            memcpy(lista, atual, sizeof atual);
            qtd = qtd_atual;
            escolhido = idx;
            xr = alternativas[a];
        } else {
            liberar_candidatos(atual, qtd_atual);
        }
    }
    if (qtd == 0) {
        fprintf(stderr, "Erro: o texto/arquivo está vazio.\n");
        goto fim;
    }
    nb = lista[escolhido].n;
    if (com_xor) {
        unsigned char ultimo = cod[qtd_cod - 1];
        unsigned char letra = ultimo ^ xr.chave[(size_t) (qtd_cod - 1) % xr.tam];

        chave_xor_para_texto(xr.chave, xr.tam, chave_xor, sizeof chave_xor);
        if (ultimo == '\n' && letra >= 'a' && letra <= 'z') {
            fprintf(stderr, "Aviso: a quebra de linha no fim do arquivo foi ignorada (regulamento). "
                            "Se ela fosse um byte cifrado, a última letra seria '%c'.\n", letra);
        }
    }

    ordem[0] = escolhido;      /* escolhida primeiro, depois as demais por pontuacao */
    for (int i = 0, k = 1; i < qtd; i++) {
        if (i != escolhido) {
            ordem[k++] = i;
        }
    }
    for (int i = 2; i < qtd; i++) {
        int atual = ordem[i], j = i - 1;
        while (j >= 1 && lista[ordem[j]].pontos < lista[atual].pontos) {
            ordem[j + 1] = ordem[j];
            j--;
        }
        ordem[j + 1] = atual;
    }

    fwrite(lista[escolhido].texto, 1, lista[escolhido].n, stdout);
    {
        char cifras[160], chaves[200];

        montar_cadeia(&lista[escolhido], com_xor, chave_xor, cifras, sizeof cifras, chaves, sizeof chaves);
        fprintf(stderr, "@cifras %s\n@chave %s\n", cifras, chaves);
    }
    if (lista[escolhido].tam_vigenere == 1) {
        emitir_valor("César deslocamento", lista[escolhido].desl[0], 5);
    } else if (lista[escolhido].tam_vigenere > 1) {
        for (int i = 0; i < lista[escolhido].tam_vigenere; i++) {
            char nome[40];

            snprintf(nome, sizeof nome, "Vigenère letra %d (%c)", i + 1, 'a' + lista[escolhido].desl[i]);
            emitir_valor(nome, lista[escolhido].desl[i], 5);
        }
    }
    if (com_xor) {
        emitir_valores_xor(xr.chave, xr.tam);
    }
    avisar_se_nao_parece_portugues(lista[escolhido].texto, lista[escolhido].n);

    for (int i = 0; i < qtd; i++) {
        char cifras[160], chaves[200], descricao[380];
        const Candidato *c = &lista[ordem[i]];

        montar_cadeia(c, com_xor, chave_xor, cifras, sizeof cifras, chaves, sizeof chaves);
        snprintf(descricao, sizeof descricao, "%s (%s)", cifras, chaves);
        emitir_opcao(descricao, c->texto, c->n);
    }
    fprintf(stderr, "Foram testados: texto como está, Atbash e Vigenère/César (chaves de 1 a %d letras)",
            MAX_CHAVE_VIGENERE);
    if (com_xor) {
        fprintf(stderr, ", depois de desfazer o XOR (%d chave%s candidata%s)", total_alt,
                total_alt == 1 ? "" : "s", total_alt == 1 ? "" : "s");
    }
    fprintf(stderr, ".\n");
    liberar_candidatos(lista, qtd);
    resultado = 0;

fim:
    free(cod);
    if (com_xor) {
        free(base);
    }
    return resultado;
}

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Uso: %s <cesar|vigenere|atbash|xor|auto> <cifrar|decifrar> [chave]\n", argv[0]);
        return 2;
    }

    const char *algoritmo = argv[1];
    const char *modo = argv[2];
    const char *chave = argc == 4 ? argv[3] : "";
    int cifrar;

    if (strcmp(modo, "cifrar") == 0) {
        cifrar = 1;
    } else if (strcmp(modo, "decifrar") == 0) {
        cifrar = 0;
    } else {
        fprintf(stderr, "Erro: modo inválido '%s'.\n", modo);
        return 2;
    }

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
    } else if (strcmp(algoritmo, "auto") == 0) {
        if (cifrar) {
            fprintf(stderr, "Erro: o modo automático só descriptografa.\n");
            resultado = 2;
        } else {
            resultado = executar_automatico(texto, n);
        }
    } else {
        fprintf(stderr, "Erro: algoritmo inválido '%s'.\n", algoritmo);
        resultado = 2;
    }

    free(texto);
    return resultado;
}
