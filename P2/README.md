# jqindex — Indexador de archivos JSON

Proyecto 2 — Principios de Sistemas Operativos  
Escuela de Computación, Tecnológico de Costa Rica

---

## ¿Qué hace este programa?

`jqindex` permite indexar archivos JSON grandes y buscar en ellos usando
expresiones regulares, sin necesidad de parsear el archivo completo cada vez.

Funciona en dos pasos:
1. **build**: analiza el JSON, genera un archivo de índice `.jnx` con los
   paths y offsets de cada valor.
2. **search**: aplica una expresión regular sobre el índice y recupera los
   fragmentos del JSON original usando `fseek`.

---

## Estructura del proyecto

```
jqindex_project/
├── src/
│   ├── json_parser.h / .c   <- parser JSON manual, genera paths y offsets
│   ├── indexer.h / .c       <- escribe y lee el archivo .jnx
│   ├── searcher.h / .c      <- aplica regex y reconstruye la salida JSON
│   └── jqindex.cpp          <- programa principal (CLI)
├── tests/
│   ├── datos.json           <- ejemplo del enunciado
│   ├── simple.json          <- prueba básica
│   ├── complejo.json        <- prueba con arrays anidados
│   └── test.sh              <- script de pruebas automático
└── Makefile
```

---

## Requisitos

- GCC y G++ (cualquier versión reciente)
- Make
- Linux (se usa `regex.h` de la librería estándar de POSIX)

---

## Compilación

Desde la raíz del proyecto:

```bash
make
```

Esto genera el binario `jqindex` en la raíz del proyecto.

Para limpiar los archivos compilados:

```bash
make clean
```

---

## Uso

### Generar el índice

```bash
./jqindex build <archivo.json>
```

Ejemplo:

```bash
./jqindex build tests/datos.json
# Indice generado: tests/datos.jnx (14 entradas)
```

Esto genera `tests/datos.jnx` en texto plano con el formato:
```
/usuarios|14|109
/usuarios/0/nombre|37|41
...
```

### Buscar en el índice

```bash
./jqindex search <archivo.json> "<patron>"
```

El patrón debe empezar con `$` (representa la raíz del JSON).

Ejemplos:

```bash
# Buscar todos los nombres de usuarios
./jqindex search tests/datos.json '$/usuarios/[0-9]+/nombre'
# ["Ana", "Luis"]

# Buscar todo el subárbol de config
./jqindex search tests/datos.json '$/config/.*'
# ["1.0", {"modo": "auto"}, "auto"]

# Buscar un valor único
./jqindex search tests/datos.json '$/config/version'
# "1.0"

# Buscar todos los campos activo
./jqindex search tests/datos.json '$/usuarios/[0-9]+/activo'
# [true, false]
```

**Importante:** hay que hacer `build` antes de `search`. Si el `.jnx` no
existe, el programa lo indica con un mensaje de sugerencia.

---

## Formato del archivo .jnx

El archivo `.jnx` es texto plano, una entrada por línea:

```
path|inicio|fin
```

- `path`: ruta absoluta del elemento (ej. `/usuarios/0/nombre`)
- `inicio`: offset en bytes desde el inicio del archivo (para `fseek`)
- `fin`: offset del último byte del valor (inclusive)

El separador `|` se eligió porque no aparece en paths JSON válidos.

---

## Pruebas

### Ejecutar todas las pruebas automáticas

```bash
bash tests/test.sh
```

El script corre 19 casos de prueba sobre tres archivos JSON distintos
y muestra cuáles pasan y cuáles fallan:

```
==============================
 Pruebas de jqindex
==============================

--- Bloque 1: datos.json ---
[PASS] build genera el .jnx
[PASS] busqueda nombres usuarios
[PASS] busqueda valor unico string
...

==============================
 Resultado: 19/19 pruebas pasaron
 Todas las pruebas pasaron!
==============================
```

### Pruebas manuales rápidas

```bash
# 1. Compilar
make

# 2. Generar índice del ejemplo del enunciado
./jqindex build tests/datos.json

# 3. Búsqueda principal del enunciado
./jqindex search tests/datos.json '$/usuarios/[0-9]+/nombre'
# Esperado: ["Ana", "Luis"]

# 4. Ver el índice generado directamente
cat tests/datos.jnx

# 5. Probar con el JSON complejo
./jqindex build tests/complejo.json
./jqindex search tests/complejo.json '$/departamentos/[0-9]+/nombre'
# Esperado: ["Computo", "Matematica"]
```

---

## Notas técnicas

- El parser asume JSON bien formado (no valida el archivo).
- `IndexResult` se declara `static` en todos los módulos porque la
  estructura ocupa ~5MB y desborda el stack si se declara como variable local.
- Los offsets son absolutos desde el inicio del archivo (`SEEK_SET`).
- El patrón de búsqueda puede omitir el `$` inicial; el programa lo maneja
  en ambos casos.
