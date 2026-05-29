#ifndef INDEXER_H
#define INDEXER_H

#include "json_parser.h"

/* Escribe el indice en un archivo .jnx.
   json_path es la ruta al .json original (para derivar el nombre del .jnx).
   Retorna 0 si todo bien, -1 si hubo error. */
int write_index(const char *jnx_path, const IndexResult *result);

/* Lee un archivo .jnx y llena el resultado.
   Retorna 0 si todo bien, -1 si hubo error. */
int read_index(const char *jnx_path, IndexResult *result);

/* Dado el path de un .json, genera el path del .jnx correspondiente.
   Ejemplo: "datos.json" -> "datos.jnx"
   El buffer out_path debe tener al menos out_size bytes. */
void get_jnx_path(const char *json_path, char *out_path, int out_size);

#endif