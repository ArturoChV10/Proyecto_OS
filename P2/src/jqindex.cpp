#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Incluimos los modulos de la libreria */
extern "C" {
    #include "json_parser.h"
    #include "indexer.h"
    #include "searcher.h"
}

/* ------------------------------------------------------------------ */
/*  Helpers                                                             */
/* ------------------------------------------------------------------ */

static void print_uso() {
    printf("Uso:\n");
    printf("  jqindex build  <archivo.json>            - genera el indice .jnx\n");
    printf("  jqindex search <archivo.json> <patron>   - busca usando expresion regular\n");
    printf("\nEjemplos:\n");
    printf("  jqindex build datos.json\n");
    printf("  jqindex search datos.json \"$/usuarios/[0-9]+/nombre\"\n");
}

/* ------------------------------------------------------------------ */
/*  Comando build                                                       */
/* ------------------------------------------------------------------ */

/* IndexResult es muy grande para el stack, va estatica */
static IndexResult resultado;

static int cmd_build(const char *json_path) {
    /* 1. Abrir y leer el archivo JSON */
    FILE *f = fopen(json_path, "r");
    if (!f) {
        perror("Error: no se pudo abrir el archivo JSON");
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    if (size <= 0) {
        fprintf(stderr, "Error: el archivo esta vacio\n");
        fclose(f);
        return 1;
    }

    char *buffer = (char *)malloc(size + 1);
    if (!buffer) {
        fprintf(stderr, "Error: no hay memoria suficiente\n");
        fclose(f);
        return 1;
    }

    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);

    /* 2. Parsear el JSON */
    if (parse_json(buffer, size, &resultado) != 0) {
        fprintf(stderr, "Error: no se pudo parsear el archivo JSON\n");
        free(buffer);
        return 1;
    }
    free(buffer);

    /* 3. Escribir el indice .jnx */
    char jnx_path[512];
    get_jnx_path(json_path, jnx_path, sizeof(jnx_path));

    if (write_index(jnx_path, &resultado) != 0) {
        fprintf(stderr, "Error: no se pudo escribir el indice\n");
        return 1;
    }

    printf("Indice generado: %s (%d entradas)\n", jnx_path, resultado.count);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Comando search                                                      */
/* ------------------------------------------------------------------ */

static int cmd_search(const char *json_path, const char *patron) {
    /* Verificar que el .jnx existe */
    char jnx_path[512];
    get_jnx_path(json_path, jnx_path, sizeof(jnx_path));

    FILE *test = fopen(jnx_path, "r");
    if (!test) {
        fprintf(stderr, "Error: no se encontro el indice '%s'\n", jnx_path);
        fprintf(stderr, "Sugerencia: ejecute primero 'jqindex build %s'\n", json_path);
        return 1;
    }
    fclose(test);

    /* Hacer la busqueda */
    int encontrados = search_index(json_path, jnx_path, patron);

    if (encontrados < 0) {
        fprintf(stderr, "Error durante la busqueda\n");
        return 1;
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Main                                                                */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[]) {
    if (argc < 3) {
        print_uso();
        return 1;
    }

    const char *comando   = argv[1];
    const char *json_path = argv[2];

    if (strcmp(comando, "build") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Error: 'build' requiere exactamente un archivo\n");
            print_uso();
            return 1;
        }
        return cmd_build(json_path);

    } else if (strcmp(comando, "search") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Error: 'search' requiere un archivo y un patron\n");
            print_uso();
            return 1;
        }
        const char *patron = argv[3];
        return cmd_search(json_path, patron);

    } else {
        fprintf(stderr, "Error: comando desconocido '%s'\n", comando);
        print_uso();
        return 1;
    }
}
