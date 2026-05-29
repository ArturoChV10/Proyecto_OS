#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <stddef.h>

/* Cuantas entradas maximas puede tener el indice.
   Para un proyecto universitario 10000 esta bien. */
#define MAX_ENTRIES 10000
#define MAX_PATH_LEN 512

/* Una entrada del indice: path + donde empieza y termina el valor en el archivo */
typedef struct
{
    char path[MAX_PATH_LEN];
    long inicio;
    long fin;
} IndexEntry;

/* Resultado del parseo - lista de entradas y cuantas hay */
typedef struct
{
    IndexEntry entries[MAX_ENTRIES];
    int count;
} IndexResult;

/* Funcion principal: recibe el contenido del archivo JSON como string,
   su tamanio, y llena el resultado con todos los paths e indices.
   Retorna 0 si todo bien, -1 si hubo error. */
int parse_json(const char *buffer, long buf_size, IndexResult *result);

#endif
