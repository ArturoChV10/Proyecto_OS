#!/bin/bash
# test.sh - Script de pruebas para jqindex
# Ejecutar desde la raiz del proyecto: bash tests/test.sh

JQINDEX=./jqindex
PASS=0
FAIL=0

# Colores para la salida
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # sin color

# ----------------------------------------------------------------
# Helper: compara salida obtenida con esperada
# ----------------------------------------------------------------
check() {
    local descripcion="$1"
    local esperado="$2"
    local obtenido="$3"

    if [ "$obtenido" = "$esperado" ]; then
        echo -e "${GREEN}[PASS]${NC} $descripcion"
        PASS=$((PASS + 1))
    else
        echo -e "${RED}[FAIL]${NC} $descripcion"
        echo "       Esperado: $esperado"
        echo "       Obtenido: $obtenido"
        FAIL=$((FAIL + 1))
    fi
}

# ----------------------------------------------------------------
# Verificar que el binario existe
# ----------------------------------------------------------------
if [ ! -f "$JQINDEX" ]; then
    echo "ERROR: no se encontro el binario '$JQINDEX'"
    echo "Ejecute 'make' primero"
    exit 1
fi

echo "=============================="
echo " Pruebas de jqindex"
echo "=============================="
echo ""

# ================================================================
# BLOQUE 1: datos.json (ejemplo del PDF)
# ================================================================
echo "--- Bloque 1: datos.json ---"

$JQINDEX build tests/datos.json > /dev/null
check "build genera el .jnx" "0" "$?"

resultado=$($JQINDEX search tests/datos.json '$/usuarios/[0-9]+/nombre')
check "busqueda nombres usuarios" '["Ana", "Luis"]' "$resultado"

resultado=$($JQINDEX search tests/datos.json '$/config/version')
check "busqueda valor unico string" '"1.0"' "$resultado"

resultado=$($JQINDEX search tests/datos.json '$/usuarios/[0-9]+/activo')
check "busqueda booleanos" '[true, false]' "$resultado"

resultado=$($JQINDEX search tests/datos.json '$/usuarios/[0-9]+/id')
check "busqueda numeros" '[1, 2]' "$resultado"

resultado=$($JQINDEX search tests/datos.json '$/noexiste' 2>/dev/null)
check "busqueda sin resultados" 'No se encontraron resultados para: $/noexiste' "$resultado"

echo ""

# ================================================================
# BLOQUE 2: simple.json
# ================================================================
echo "--- Bloque 2: simple.json ---"

$JQINDEX build tests/simple.json > /dev/null
check "build simple.json" "0" "$?"

resultado=$($JQINDEX search tests/simple.json '$/nombre')
check "busqueda campo nombre" '"Carlos"' "$resultado"

resultado=$($JQINDEX search tests/simple.json '$/edad')
check "busqueda campo numerico" '30' "$resultado"

resultado=$($JQINDEX search tests/simple.json '$/activo')
check "busqueda booleano true" 'true' "$resultado"

resultado=$($JQINDEX search tests/simple.json '$/direccion/ciudad')
check "busqueda campo anidado" '"San Jose"' "$resultado"

echo ""

# ================================================================
# BLOQUE 3: complejo.json (arrays anidados)
# ================================================================
echo "--- Bloque 3: complejo.json (arrays anidados) ---"

$JQINDEX build tests/complejo.json > /dev/null
check "build complejo.json" "0" "$?"

resultado=$($JQINDEX search tests/complejo.json '$/departamentos/[0-9]+/nombre')
check "nombres de departamentos" '["Computo", "Matematica"]' "$resultado"

resultado=$($JQINDEX search tests/complejo.json '$/departamentos/[0-9]+/jefe')
check "jefes de departamentos" '["Maria", "Roberto"]' "$resultado"

resultado=$($JQINDEX search tests/complejo.json '$/departamentos/0/empleados/[0-9]+/nombre')
check "empleados del primer departamento" '["Pedro", "Laura"]' "$resultado"

resultado=$($JQINDEX search tests/complejo.json '$/empresa')
check "campo empresa" '"TEC"' "$resultado"

echo ""

# ================================================================
# BLOQUE 4: manejo de errores
# ================================================================
echo "--- Bloque 4: manejo de errores ---"

# Buscar sin haber hecho build (borrar el .jnx primero)
rm -f tests/simple.jnx
$JQINDEX search tests/simple.json '$/nombre' > /dev/null 2>&1
check "search sin .jnx retorna error" "1" "$?"

# Archivo que no existe
$JQINDEX build tests/noexiste.json > /dev/null 2>&1
check "build con archivo inexistente retorna error" "1" "$?"

# Sin argumentos
$JQINDEX > /dev/null 2>&1
check "sin argumentos retorna error" "1" "$?"

echo ""

# ================================================================
# Resumen
# ================================================================
echo "=============================="
TOTAL=$((PASS + FAIL))
echo " Resultado: $PASS/$TOTAL pruebas pasaron"
if [ $FAIL -eq 0 ]; then
    echo -e " ${GREEN}Todas las pruebas pasaron!${NC}"
else
    echo -e " ${RED}$FAIL prueba(s) fallaron${NC}"
fi
echo "=============================="

# Limpiar .jnx generados por las pruebas
rm -f tests/*.jnx

exit $FAIL
