#ifndef SEARCHER_H
#define SEARCHER_H

/* Funcion principal de busqueda.
   - json_path: ruta al archivo .json original
   - jnx_path:  ruta al archivo .jnx del indice
   - patron:    expresion regular (puede empezar con $ que se ignora)
   
   Imprime el resultado en stdout:
   - Si hay un solo resultado: imprime el valor directamente
   - Si hay varios: imprime un array JSON [ val1, val2, ... ]
   - Si no hay nada: imprime un mensaje informativo
   
   Retorna el numero de coincidencias encontradas, o -1 si hubo error. */
int search_index(const char *json_path, const char *jnx_path, const char *patron);

#endif
