#include <stdio.h>
#include <string.h>

int main(void) {
    char mensagem[] = "abcabc";
    char senha[] = "abc";

    int tamanho_mensagem = strlen(mensagem);
    int tamanho_senha = strlen(senha);

    // Criptografia de Vigenere
    for (int i = 0; i < tamanho_mensagem; i++) {
        int letra = mensagem[i] - 'a';
        int chave = senha[i % tamanho_senha] - 'a';

        mensagem[i] = 'a' + (letra + chave) % 26;
    }

    printf("Mensagem criptografada: %s\n", mensagem);

    // Descriptografia de Vigenere
    char segredo[] = "aceace";

    tamanho_mensagem = strlen(segredo);

    for (int i = 0; i < tamanho_mensagem; i++) {
        int letra = segredo[i] - 'a';
        int chave = senha[i % tamanho_senha] - 'a';

        segredo[i] = 'a' + (letra - chave + 26) % 26;
    }

    printf("Mensagem descriptografada: %s\n", segredo);

    return 0;
}

