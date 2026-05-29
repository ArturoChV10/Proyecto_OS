#include "indexer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/*  Formato del .jnx                                                    */
/* ------------------------------------------------------------------ */
/*
   El archivo .jnx es texto plano, una entrada por linea.
   Formato: path|inicio|fin
   Ejemplo:
       /|0|175
       /usuarios|14|109
       /usuarios/0|16|59
       /usuarios/0/id|24|24
       /usuarios/0/nombre|37|41

   - path: ruta absoluta del elemento, empieza con /
   - inicio: offset en bytes desde el inicio del archivo .json (SEEK_SET)
   - fin: offset en bytes del ultimo caracter del valor (inclusive)

   El separador | se eligio porque no aparece en paths JSON validos.
*/

void get_jnx_path(const char *json_path, char *out_path, int out_size)
{
    strncpy(out_path, json_path, out_size - 1);
    out_path[out_size - 1] = '\0';

    /* Buscar la extension .json y reemplazarla por .jnx */
    char *ext = strstr(out_path, ".json");
    if (ext != NULL)
    {
        strncpy(ext, ".jnx", 5);
    }
    else
    {
        /* Si no tiene extension .json, simplemente agregar .jnx al final */
        strncat(out_path, ".jnx", out_size - strlen(out_path) - 1);
    }
}

int write_index(const char *jnx_path, const IndexResult *result)
{
    FILE *f = fopen(jnx_path, "w");
    if (!f)
    {
        perror("No se pudo crear el archivo .jnx");
        return -1;
    }

    for (int i = 0; i < result->count; i++)
    {
        const char *path = result->entries[i].path;

        /* La entrada raiz tiene path vacio "", la guardamos como "/" */
        if (path[0] == '\0')
        {
            fprintf(f, "/|%ld|%ld\n",
                    result->entries[i].inicio,
                    result->entries[i].fin);
        }
        else
        {
            fprintf(f, "%s|%ld|%ld\n",
                    path,
                    result->entries[i].inicio,
                    result->entries[i].fin);
        }
    }

    fclose(f);
    return 0;
}

int read_index(const char *jnx_path, IndexResult *result)
{
    FILE *f = fopen(jnx_path, "r");
    if (!f)
    {
        perror("No se pudo abrir el archivo .jnx");
        return -1;
    }

    result->count = 0;
    char line[MAX_PATH_LEN + 64]; /* path + separadores + dos numeros */

    while (fgets(line, sizeof(line), f) != NULL)
    {
        if (result->count >= MAX_ENTRIES)
        {
            fprintf(stderr, "WARNING: indice muy grande, se trunca en %d entradas\n", MAX_ENTRIES);
            break;
        }

        /* Eliminar el newline del final */
        int len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
        {
            line[len - 1] = '\0';
            len--;
        }

        /* Buscar el primer | para separar path de los numeros */
        char *sep1 = strchr(line, '|');
        if (!sep1)
            continue; /* linea malformada, saltar */

        char *sep2 = strchr(sep1 + 1, '|');
        if (!sep2)
            continue;

        /* Extraer los tres campos */
        *sep1 = '\0';
        *sep2 = '\0';

        char *path_str = line;
        char *inicio_str = sep1 + 1;
        char *fin_str = sep2 + 1;

        strncpy(result->entries[result->count].path, path_str, MAX_PATH_LEN - 1);
        result->entries[result->count].path[MAX_PATH_LEN - 1] = '\0';
        result->entries[result->count].inicio = atol(inicio_str);
        result->entries[result->count].fin = atol(fin_str);
        result->count++;
    }

    fclose(f);
    return 0;
}