#include <stdio.h>
#include "searcher.h"
#include "indexer.h"

int main(int argc, char *argv[]) {
    const char *json_path = (argc > 1) ? argv[1] : "../tests/datos.json";

    /* Derivar el path del .jnx automaticamente */
    char jnx_path[512];
    get_jnx_path(json_path, jnx_path, sizeof(jnx_path));

    printf("=== Test del buscador ===\n");
    printf("JSON: %s\n", json_path);
    printf("JNX:  %s\n\n", jnx_path);

    /* --- Caso 1: busqueda del ejemplo principal del PDF --- */
    printf("--- Busqueda 1: $/usuarios/[0-9]+/nombre ---\n");
    int r1 = search_index(json_path, jnx_path, "$/usuarios/[0-9]+/nombre");
    printf("(encontrados: %d)\n\n", r1);

    /* --- Caso 2: subárbol completo de config --- */
    printf("--- Busqueda 2: $/config/.* ---\n");
    int r2 = search_index(json_path, jnx_path, "$/config/.*");
    printf("(encontrados: %d)\n\n", r2);

    /* --- Caso 3: todos los campos activo --- */
    printf("--- Busqueda 3: $/usuarios/[0-9]+/activo ---\n");
    int r3 = search_index(json_path, jnx_path, "$/usuarios/[0-9]+/activo");
    printf("(encontrados: %d)\n\n", r3);

    /* --- Caso 4: valor unico exacto --- */
    printf("--- Busqueda 4: $/config/version ---\n");
    int r4 = search_index(json_path, jnx_path, "$/config/version");
    printf("(encontrados: %d)\n\n", r4);

    /* --- Caso 5: sin resultados --- */
    printf("--- Busqueda 5: $/noexiste ---\n");
    int r5 = search_index(json_path, jnx_path, "$/noexiste");
    printf("(encontrados: %d)\n\n", r5);

    /* --- Caso 6: todos los ids --- */
    printf("--- Busqueda 6: $/usuarios/[0-9]+/id ---\n");
    int r6 = search_index(json_path, jnx_path, "$/usuarios/[0-9]+/id");
    printf("(encontrados: %d)\n\n", r6);

    return 0;
}
