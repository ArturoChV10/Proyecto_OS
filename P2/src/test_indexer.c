#include <stdio.h>
#include <stdlib.h>
#include "json_parser.h"
#include "indexer.h"

/* IndexResult es muy grande para el stack (10000 * 512 bytes de path).
   Las declaramos static para que vivan en el segmento de datos. */
static IndexResult result;
static IndexResult result2;

int main(int argc, char *argv[])
{
    const char *json_path = (argc > 1) ? argv[1] : "../tests/datos.json";

    /* 1. Leer el archivo JSON */
    FILE *f = fopen(json_path, "r");
    if (!f)
    {
        perror("No se pudo abrir el JSON");
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *buffer = (char *)malloc(size + 1);
    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);

    /* 2. Parsear */
    if (parse_json(buffer, size, &result) != 0)
    {
        fprintf(stderr, "Error al parsear\n");
        free(buffer);
        return 1;
    }
    free(buffer);

    printf("Parser genero %d entradas\n", result.count);

    /* 3. Escribir el .jnx */
    char jnx_path[512];
    get_jnx_path(json_path, jnx_path, sizeof(jnx_path));
    printf("Escribiendo indice en: %s\n", jnx_path);

    if (write_index(jnx_path, &result) != 0)
    {
        fprintf(stderr, "Error escribiendo el .jnx\n");
        return 1;
    }

    /* 4. Leer el .jnx de vuelta y verificar que coincide */
    if (read_index(jnx_path, &result2) != 0)
    {
        fprintf(stderr, "Error leyendo el .jnx\n");
        return 1;
    }

    printf("Indice leido de vuelta: %d entradas\n\n", result2.count);

    /* 5. Mostrar el contenido del .jnx */
    printf("Contenido del archivo .jnx:\n");
    printf("%-40s %8s %8s\n", "PATH", "INICIO", "FIN");
    printf("%-40s %8s %8s\n",
           "----------------------------------------", "--------", "--------");
    for (int i = 0; i < result2.count; i++)
    {
        printf("%-40s %8ld %8ld\n",
               result2.entries[i].path,
               result2.entries[i].inicio,
               result2.entries[i].fin);
    }

    /* 6. Verificar que los counts coinciden */
    if (result.count == result2.count)
    {
        printf("\nOK: el indice se escribio y leyo correctamente (%d entradas)\n", result.count);
    }
    else
    {
        printf("\nERROR: se escribieron %d entradas pero se leyeron %d\n",
               result.count, result2.count);
    }

    return 0;
}