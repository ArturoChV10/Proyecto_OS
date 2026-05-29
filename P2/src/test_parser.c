#include <stdio.h>
#include <stdlib.h>
#include "json_parser.h"

/* IndexResult ocupa mucho stack (10000 entradas * 512 bytes).
   Siempre declarar como static o global. */
static IndexResult result;

int main(int argc, char *argv[])
{
    const char *filename = (argc > 1) ? argv[1] : "../tests/datos.json";

    FILE *f = fopen(filename, "r");
    if (!f)
    {
        perror("No se pudo abrir el archivo");
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *buffer = (char *)malloc(size + 1);
    if (!buffer)
    {
        fprintf(stderr, "Error de memoria\n");
        fclose(f);
        return 1;
    }
    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);

    if (parse_json(buffer, size, &result) != 0)
    {
        fprintf(stderr, "Error al parsear el JSON\n");
        free(buffer);
        return 1;
    }

    printf("Entradas generadas: %d\n\n", result.count);
    printf("%-40s %8s %8s\n", "PATH", "INICIO", "FIN");
    printf("%-40s %8s %8s\n", "----------------------------------------", "--------", "--------");
    for (int i = 0; i < result.count; i++)
    {
        printf("%-40s %8ld %8ld\n",
               result.entries[i].path,
               result.entries[i].inicio,
               result.entries[i].fin);
    }

    printf("\n--- Verificacion de fragmentos ---\n");
    f = fopen(filename, "r");
    for (int i = 0; i < result.count; i++)
    {
        long len = result.entries[i].fin - result.entries[i].inicio + 1;
        char *frag = (char *)malloc(len + 1);
        fseek(f, result.entries[i].inicio, SEEK_SET);
        fread(frag, 1, len, f);
        frag[len] = '\0';
        printf("%s => [%s]\n", result.entries[i].path, frag);
        free(frag);
    }
    fclose(f);

    free(buffer);
    return 0;
}