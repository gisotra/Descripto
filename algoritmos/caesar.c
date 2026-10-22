#include <stdio.h>
#include <stdlib.h>
#include <string.h>



void main(){

    char mensagem[] = {"abcxyz"};

    int senha = 3;
    int i = 0;

    for (i = 0; i < strlen(mensagem); i++)
    {
        if(((int) mensagem[i] + senha) > 122){
            mensagem[i] += senha - 26;
        }else{
            mensagem[i] += senha;
        }
        printf("ASCII: %d ", (int) mensagem[i]);
    
    }
    
    printf("\n%s\n", mensagem);

    char segredo[] = {"defabc"};

    senha = -3;
    i = 0;
    
    for (i = 0; i < strlen(segredo); i++)
    {
        if(((int) segredo[i] + senha) < 97){
            segredo[i] += senha + 26;
        }else{
            segredo[i] += senha;
        }
        printf("ASCII: %d ", (int) segredo[i]);
    
    }

    printf("\n%s\n", segredo);

}


// void desencriptador(char mensagem, int senha, int i){
    
    

// }


void encriptador(char mensagem, int senha){


}