#!/bin/bash

# Colores para la salida
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # Sin color

# Variables
SERVER="./aws-s3_server"
CLIENT="./aws-s3"
BUCKET="test-bucket-$(date +%s)"  # Nombre único para evitar colisiones
TEST_DIR="./test_temp"
PASS=0
FAIL=0

# Función para imprimir resultados
print_result() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}[PASS]${NC} $2"
        ((PASS++))
    else
        echo -e "${RED}[FAIL]${NC} $2"
        ((FAIL++))
    fi
}

# Función para ejecutar un comando y verificar su salida
run_test() {
    local cmd="$1"
    local expected="$2"
    local output
    output=$(eval "$cmd" 2>&1)
    if echo "$output" | grep -q "$expected"; then
        return 0
    else
        echo "  Salida obtenida: $output"
        echo "  Esperado: $expected"
        return 1
    fi
}

# Limpiar pruebas anteriores
cleanup() {
    echo -e "${YELLOW}Limpiando entorno de pruebas...${NC}"
    pkill -f "$SERVER" 2>/dev/null
    rm -rf "$TEST_DIR"
    rm -f ./buckets/$BUCKET 2>/dev/null
    rm -f ./buckets/${BUCKET}_2 2>/dev/null
}

# Verificar que los ejecutables existen
check_executables() {
    if [ ! -f "$SERVER" ]; then
        echo -e "${RED}Error: $SERVER no encontrado. Compilando...${NC}"
        gcc -o aws-s3_server server.c bucket.c || exit 1
    fi
    if [ ! -f "$CLIENT" ]; then
        echo -e "${RED}Error: $CLIENT no encontrado. Compilando...${NC}"
        gcc -o aws-s3 client.c || exit 1
    fi
}

# Iniciar el servidor en segundo plano
start_server() {
    echo -e "${YELLOW}Iniciando servidor...${NC}"
    $SERVER &
    SERVER_PID=$!
    sleep 1  # Dar tiempo al servidor para iniciar
    if ! kill -0 $SERVER_PID 2>/dev/null; then
        echo -e "${RED}Error: No se pudo iniciar el servidor.${NC}"
        exit 1
    fi
    echo -e "${GREEN}Servidor iniciado con PID $SERVER_PID${NC}"
}

# ----- PRUEBAS -----

# 1. Creación de bucket
test_mb() {
    echo -e "\n${YELLOW}--- Prueba: mb ---${NC}"
    run_test "$CLIENT mb s3://$BUCKET" "Bucket creado exitosamente."
    print_result $? "Crear bucket $BUCKET"
    
    run_test "$CLIENT mb s3://$BUCKET" "Error: El bucket ya existe."
    print_result $? "Intentar crear bucket duplicado"
}

# 2. Listado de buckets
test_ls_buckets() {
    echo -e "\n${YELLOW}--- Prueba: ls (buckets) ---${NC}"
    run_test "$CLIENT ls" "$BUCKET"
    print_result $? "Listar buckets (contiene $BUCKET)"
}

# 3. Listado de objetos (bucket vacío)
test_ls_empty() {
    echo -e "\n${YELLOW}--- Prueba: ls (bucket vacío) ---${NC}"
    run_test "$CLIENT ls s3://$BUCKET/" "Objetos en s3://$BUCKET/:"
    print_result $? "Listar bucket vacío"
}

# 4. Subida de archivo
test_upload() {
    echo -e "\n${YELLOW}--- Prueba: cp (subida) ---${NC}"
    mkdir -p "$TEST_DIR"
    echo "Hola mundo" > "$TEST_DIR/origen.txt"
    run_test "$CLIENT cp $TEST_DIR/origen.txt s3://$BUCKET/destino.txt" "OK"
    print_result $? "Subir archivo origen.txt -> destino.txt"
}

# 5. Descarga de archivo
test_download() {
    echo -e "\n${YELLOW}--- Prueba: cp (bajada) ---${NC}"
    run_test "$CLIENT cp s3://$BUCKET/destino.txt $TEST_DIR/descarga.txt" "Archivo descargado:"
    print_result $? "Descargar destino.txt -> descarga.txt"
    
    # Verificar integridad
    if diff "$TEST_DIR/origen.txt" "$TEST_DIR/descarga.txt" >/dev/null 2>&1; then
        print_result 0 "Integridad del archivo descargado"
    else
        print_result 1 "Integridad del archivo descargado"
    fi
}

# 6. Listar objetos después de subir
test_ls_objects() {
    echo -e "\n${YELLOW}--- Prueba: ls (con objetos) ---${NC}"
    run_test "$CLIENT ls s3://$BUCKET/" "destino.txt"
    print_result $? "Listar objetos (contiene destino.txt)"
}

# 7. Eliminar objeto
test_rm_single() {
    echo -e "\n${YELLOW}--- Prueba: rm (simple) ---${NC}"
    run_test "$CLIENT rm s3://$BUCKET/destino.txt" "Operación completada."
    print_result $? "Eliminar destino.txt"
    
    run_test "$CLIENT cp s3://$BUCKET/destino.txt $TEST_DIR/nada.txt" "Objeto no encontrado"
    print_result $? "Verificar que destino.txt fue eliminado"
}

# 8. Eliminación recursiva
test_rm_recursive() {
    echo -e "\n${YELLOW}--- Prueba: rm --recursive ---${NC}"
    # Crear estructura de carpetas
    echo "Archivo 1" > "$TEST_DIR/a.txt"
    echo "Archivo 2" > "$TEST_DIR/b.txt"
    mkdir -p "$TEST_DIR/subdir"
    echo "Archivo 3" > "$TEST_DIR/subdir/c.txt"
    
    $CLIENT cp "$TEST_DIR/a.txt" "s3://$BUCKET/carpeta/a.txt" >/dev/null 2>&1
    $CLIENT cp "$TEST_DIR/b.txt" "s3://$BUCKET/carpeta/b.txt" >/dev/null 2>&1
    $CLIENT cp "$TEST_DIR/subdir/c.txt" "s3://$BUCKET/carpeta/sub/c.txt" >/dev/null 2>&1
    
    run_test "$CLIENT rm s3://$BUCKET/carpeta/ --recursive" "Operación completada."
    print_result $? "Eliminar recursivo carpeta/"
    
    # Verificar que los objetos fueron eliminados
    run_test "$CLIENT ls s3://$BUCKET/" "carpeta"
    if [ $? -eq 1 ]; then
        print_result 0 "Verificar que carpeta/ ya no aparece en ls"
    else
        print_result 1 "Verificar que carpeta/ ya no aparece en ls"
    fi
}

# 9. Movimiento de objetos (mv)
test_mv() {
    echo -e "\n${YELLOW}--- Prueba: mv (entre S3) ---${NC}"
    echo "Origen" > "$TEST_DIR/origen_mv.txt"
    $CLIENT cp "$TEST_DIR/origen_mv.txt" "s3://$BUCKET/origen.txt" >/dev/null 2>&1
    
    run_test "$CLIENT mv s3://$BUCKET/origen.txt s3://$BUCKET/destino_mv.txt" "Movido exitosamente"
    print_result $? "Mover origen.txt -> destino_mv.txt"
    
    # Verificar que origen ya no existe
    run_test "$CLIENT cp s3://$BUCKET/origen.txt $TEST_DIR/check.txt" "Objeto no encontrado"
    print_result $? "Verificar que origen.txt fue eliminado"
}

# 10. Eliminación de bucket (vacío)
test_rb_empty() {
    echo -e "\n${YELLOW}--- Prueba: rb (bucket vacío) ---${NC}"
    $CLIENT mb "s3://${BUCKET}_vacio" >/dev/null 2>&1
    run_test "$CLIENT rb s3://${BUCKET}_vacio" "Bucket eliminado."
    print_result $? "Eliminar bucket vacío"
}

# 11. Eliminación de bucket con contenido (sin --force)
test_rb_nonempty() {
    echo -e "\n${YELLOW}--- Prueba: rb (bucket con contenido, sin --force) ---${NC}"
    run_test "$CLIENT rb s3://$BUCKET" "Bucket no vacío. Use --force para eliminar"
    print_result $? "Intentar eliminar bucket con contenido (sin --force)"
}

# 12. Eliminación de bucket con --force
test_rb_force() {
    echo -e "\n${YELLOW}--- Prueba: rb --force ---${NC}"
    run_test "$CLIENT rb s3://$BUCKET --force" "Bucket eliminado (con --force)."
    print_result $? "Eliminar bucket con --force"
    
    # Verificar que el bucket ya no existe
    run_test "$CLIENT ls s3://$BUCKET/" "Bucket no encontrado"
    print_result $? "Verificar que el bucket fue eliminado"
}

# ----- EJECUCIÓN PRINCIPAL -----

main() {
    echo -e "${YELLOW}=== INICIANDO PRUEBAS DEL PROYECTO AWS-S3 ===${NC}"
    cleanup
    check_executables
    start_server
    
    # Ejecutar pruebas
    test_mb
    test_ls_buckets
    test_ls_empty
    test_upload
    test_download
    test_ls_objects
    test_rm_single
    test_rm_recursive
    test_mv
    test_rb_empty
    test_rb_nonempty
    test_rb_force
    
    # Limpiar
    cleanup
    echo -e "\n${YELLOW}=== RESUMEN ===${NC}"
    echo -e "Pruebas exitosas: ${GREEN}$PASS${NC}"
    echo -e "Pruebas fallidas: ${RED}$FAIL${NC}"
    
    if [ $FAIL -eq 0 ]; then
        echo -e "${GREEN}¡Todas las pruebas pasaron exitosamente!${NC}"
        exit 0
    else
        echo -e "${RED}Algunas pruebas fallaron. Revisa los detalles.${NC}"
        exit 1
    fi
}

# Capturar señal de interrupción para limpiar
trap cleanup EXIT

# Ejecutar
main