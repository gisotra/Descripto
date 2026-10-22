#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main()
{
FILE *pont_arq;
char c;

pont_arq = fopen("../xor_criptografado.txt","r");
if (pont_arq == NULL)
{
printf("Erro ao tentar abrir o arquivo!");
exit(1);
}
printf("Lendo e exibindo os dados do arquivo \n\n");


for (int j = 1; j <= 10; j++)
{
printf("\nDeslocamento %d: ", j);

do
{

c = fgetc(pont_arq);

unsigned char letra = c ^ j;
printf("%c", letra);


}while (c != EOF);
}

fclose(pont_arq);

return 0;
}
