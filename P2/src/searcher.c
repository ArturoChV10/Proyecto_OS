#include "searcher.h"
#include "indexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

/* Tamanio inicial del buffer de salida. Se crece con realloc si hace falta. */
#define OUTPUT_BUF_INIT 4096

/* ------------------------------------------------------------------ */
/*  Manejo del patron regex                                             */
/* ------------------------------------------------------------------ */

/* El patron viene del CLI como "$/usuarios/[0-9]+/nombre".
   El $ es la raiz (convencion del proyecto), no es el $ de regex.
   Lo reemplazamos por ^ para anclar al inicio del path.
   Si no empieza con $, igual le ponemos ^ para que el match sea desde el inicio. */
static void preparar_patron(const char *patron_input, char *patron_out, int out_size) {
    if (patron_input[0] == '$') {
        /* Saltar el $ y poner ^ en su lugar */
        snprintf(patron_out, out_size, "^%s$", patron_input + 1);
    } else {
        snprintf(patron_out, out_size, "^%s$", patron_input);
    }
}

/* ------------------------------------------------------------------ */
/*  Lectura de fragmento del JSON original                              */
/* ------------------------------------------------------------------ */

/* Lee los bytes [inicio, fin] del archivo y los retorna en un buffer
   recien allocado. El llamador debe hacer free() del resultado.
   Retorna NULL si falla. */
static char *leer_fragmento(FILE *json_file, long inicio, long fin) {
    long len = fin - inicio + 1;
    if (len <= 0) return NULL;

    char *frag = (char *)malloc(len + 1);
    if (!frag) return NULL;

    fseek(json_file, inicio, SEEK_SET);
    long leido = fread(frag, 1, len, json_file);
    frag[leido] = '\0';

    return frag;
}

/* ------------------------------------------------------------------ */
/*  Construccion del output JSON                                        */
/* ------------------------------------------------------------------ */

/* Agrega un fragmento al buffer de salida, creciendo el buffer si hace falta.
   Retorna el nuevo puntero al buffer (puede cambiar por realloc). */
static char *append_to_output(char *output, size_t *out_len, size_t *out_cap,
                               const char *texto) {
    size_t texto_len = strlen(texto);

    /* Ver si hay espacio, si no, duplicar capacidad */
    while (*out_len + texto_len + 1 > *out_cap) {
        *out_cap *= 2;
        output = (char *)realloc(output, *out_cap);
        if (!output) {
            fprintf(stderr, "ERROR: no hay memoria para el buffer de salida\n");
            return NULL;
        }
    }

    memcpy(output + *out_len, texto, texto_len);
    *out_len += texto_len;
    output[*out_len] = '\0';

    return output;
}

/* ------------------------------------------------------------------ */
/*  Funcion principal de busqueda                                       */
/* ------------------------------------------------------------------ */

/* IndexResult es grande, no va en el stack */
static IndexResult indice;

int search_index(const char *json_path, const char *jnx_path, const char *patron) {

    /* 1. Leer el indice */
    if (read_index(jnx_path, &indice) != 0) {
        fprintf(stderr, "ERROR: no se pudo leer el indice %s\n", jnx_path);
        return -1;
    }

    /* 2. Preparar la regex */
    char patron_preparado[1024];
    preparar_patron(patron, patron_preparado, sizeof(patron_preparado));

    regex_t regex;
    int ret = regcomp(&regex, patron_preparado, REG_EXTENDED | REG_NOSUB);
    if (ret != 0) {
        char errbuf[256];
        regerror(ret, &regex, errbuf, sizeof(errbuf));
        fprintf(stderr, "ERROR: regex invalida: %s\n", errbuf);
        return -1;
    }

    /* 3. Abrir el JSON original para leer fragmentos */
    FILE *json_file = fopen(json_path, "r");
    if (!json_file) {
        perror("ERROR: no se pudo abrir el archivo JSON");
        regfree(&regex);
        return -1;
    }

    /* 4. Buscar coincidencias y recolectar fragmentos */
    /* Usamos un array de punteros a los fragmentos encontrados */
    char **fragmentos = (char **)malloc(indice.count * sizeof(char *));
    if (!fragmentos) {
        fprintf(stderr, "ERROR: no hay memoria\n");
        fclose(json_file);
        regfree(&regex);
        return -1;
    }
    int num_encontrados = 0;

    for (int i = 0; i < indice.count; i++) {
        /* Intentar el match */
        if (regexec(&regex, indice.entries[i].path, 0, NULL, 0) == 0) {
            /* Match! Leer el fragmento del JSON */
            char *frag = leer_fragmento(json_file,
                                        indice.entries[i].inicio,
                                        indice.entries[i].fin);
            if (frag) {
                fragmentos[num_encontrados++] = frag;
            }
        }
    }

    fclose(json_file);
    regfree(&regex);

    /* 5. Construir la salida */
    if (num_encontrados == 0) {
        printf("No se encontraron resultados para: %s\n", patron);
        free(fragmentos);
        return 0;
    }

    /* Inicializar buffer de salida */
    size_t out_cap = OUTPUT_BUF_INIT;
    size_t out_len = 0;
    char *output = (char *)malloc(out_cap);
    if (!output) {
        fprintf(stderr, "ERROR: no hay memoria para la salida\n");
        for (int i = 0; i < num_encontrados; i++) free(fragmentos[i]);
        free(fragmentos);
        return -1;
    }
    output[0] = '\0';

    if (num_encontrados == 1) {
        /* Un solo resultado: imprimir directo */
        output = append_to_output(output, &out_len, &out_cap, fragmentos[0]);
    } else {
        /* Multiples resultados: construir array JSON */
        output = append_to_output(output, &out_len, &out_cap, "[");
        for (int i = 0; i < num_encontrados; i++) {
            output = append_to_output(output, &out_len, &out_cap, fragmentos[i]);
            if (i < num_encontrados - 1) {
                output = append_to_output(output, &out_len, &out_cap, ", ");
            }
        }
        output = append_to_output(output, &out_len, &out_cap, "]");
    }

    if (output) {
        printf("%s\n", output);
    }

    /* 6. Liberar memoria */
    free(output);
    for (int i = 0; i < num_encontrados; i++) {
        free(fragmentos[i]);
    }
    free(fragmentos);

    return num_encontrados;
}
