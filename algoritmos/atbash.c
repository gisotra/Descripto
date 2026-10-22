#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void cifrar(char texto[]);
int posicaoAlfabeto(char letra);

int main() {
    char texto[] = "abcdefghijklmnopqrstuvwxyz";
    cifrar(texto);
    return 0;
}
void cifrar(char texto[]) {
    for (size_t i = 0; texto[i] != '\0'; i++) {
        int pos = posicaoAlfabeto(texto[i]);

        if (pos == -1) {
            continue;
        }

        if (texto[i] >= 'a' && texto[i] <= 'z') {
            texto[i] = 'a' + (26 - pos);
        } else {
            texto[i] = 'A' + (26 - pos);
        }
    }
    printf("%s\n", texto);
}

int posicaoAlfabeto(char letra) {
    if (letra >= 'a' && letra <= 'z') {
        return letra - 'a' + 1;
    } else if (letra >= 'A' && letra <= 'Z') {
        return letra - 'A' + 1;
    }
    return -1;
}
