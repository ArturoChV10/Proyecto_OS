#include "json_parser.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* ------------------------------------------------------------------ */
/*  Funciones auxiliares                                                */
/* ------------------------------------------------------------------ */

/* Saltarse espacios, tabs, newlines */
static void skip_whitespace(const char *buf, long *pos, long buf_size)
{
    while (*pos < buf_size && isspace((unsigned char)buf[*pos]))
    {
        (*pos)++;
    }
}

/* Registrar una entrada en el resultado */
static void add_entry(IndexResult *result, const char *path, long inicio, long fin)
{
    if (result->count >= MAX_ENTRIES)
    {
        fprintf(stderr, "WARNING: se alcanzo el limite de entradas del indice\n");
        return;
    }
    strncpy(result->entries[result->count].path, path, MAX_PATH_LEN - 1);
    result->entries[result->count].path[MAX_PATH_LEN - 1] = '\0';
    result->entries[result->count].inicio = inicio;
    result->entries[result->count].fin = fin;
    result->count++;
}

/* Avanzar hasta el final de un string JSON (ya estamos parados en la " de apertura).
   Retorna la posicion del " de cierre. */
static long find_string_end(const char *buf, long pos, long buf_size)
{
    pos++; /* saltamos la " de apertura */
    while (pos < buf_size)
    {
        if (buf[pos] == '\\')
        {
            pos += 2; /* escape, saltamos el siguiente char tambien */
            continue;
        }
        if (buf[pos] == '"')
        {
            return pos; /* encontramos el cierre */
        }
        pos++;
    }
    return pos; /* no deberia pasar si el JSON esta bien formado */
}

/* Avanzar hasta el final de un numero JSON.
   Retorna la posicion del ultimo digito/caracter del numero. */
static long find_number_end(const char *buf, long pos, long buf_size)
{
    /* Un numero puede tener: digitos, punto, signo, e/E */
    while (pos < buf_size)
    {
        char c = buf[pos];
        if (isdigit((unsigned char)c) || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E')
        {
            pos++;
        }
        else
        {
            break;
        }
    }
    return pos - 1; /* regresamos al ultimo caracter valido */
}

/* Encontrar el fin de un objeto {} o array [] balanceado.
   pos debe apuntar al { o [ de apertura.
   Retorna la posicion del } o ] de cierre. */
static long find_block_end(const char *buf, long pos, long buf_size)
{
    char open_char = buf[pos];
    char close_char = (open_char == '{') ? '}' : ']';
    int depth = 1;
    pos++;

    while (pos < buf_size && depth > 0)
    {
        /* Si estamos dentro de un string, saltarlo completo para no confundir llaves dentro de strings */
        if (buf[pos] == '"')
        {
            pos = find_string_end(buf, pos, buf_size) + 1;
            continue;
        }
        if (buf[pos] == open_char)
            depth++;
        else if (buf[pos] == close_char)
            depth--;
        pos++;
    }
    return pos - 1; /* el } o ] de cierre */
}

/* ------------------------------------------------------------------ */
/*  Funciones de parseo (recursivas)                                    */
/* ------------------------------------------------------------------ */

/* Declaracion adelantada porque parse_object y parse_array se llaman entre si */
static void parse_value(const char *buf, long *pos, long buf_size,
                        const char *current_path, IndexResult *result);

/* Leer la clave de un objeto: avanza pos mas alla de las comillas y devuelve la clave */
static void read_key(const char *buf, long *pos, long buf_size, char *key_out, int key_max)
{
    skip_whitespace(buf, pos, buf_size);
    if (buf[*pos] != '"')
        return; /* no deberia pasar */

    long end = find_string_end(buf, *pos, buf_size);
    long key_start = *pos + 1;
    long key_len = end - key_start;

    if (key_len >= key_max)
        key_len = key_max - 1;
    strncpy(key_out, buf + key_start, key_len);
    key_out[key_len] = '\0';

    *pos = end + 1; /* avanzar mas alla de la " de cierre */
}

/* Parsear un objeto JSON {} */
static void parse_object(const char *buf, long *pos, long buf_size,
                         const char *current_path, IndexResult *result)
{
    (*pos)++; /* saltamos el { */

    while (*pos < buf_size)
    {
        skip_whitespace(buf, pos, buf_size);

        if (buf[*pos] == '}')
        {
            (*pos)++;
            break;
        }

        /* Leer la clave */
        char key[256] = {0};
        read_key(buf, pos, buf_size, key, sizeof(key));

        /* Saltar el : */
        skip_whitespace(buf, pos, buf_size);
        if (buf[*pos] == ':')
            (*pos)++;

        /* Construir el nuevo path */
        char new_path[MAX_PATH_LEN];
        snprintf(new_path, sizeof(new_path), "%s/%s", current_path, key);

        /* Parsear el valor */
        skip_whitespace(buf, pos, buf_size);
        parse_value(buf, pos, buf_size, new_path, result);

        /* Avanzar la coma si hay */
        skip_whitespace(buf, pos, buf_size);
        if (*pos < buf_size && buf[*pos] == ',')
        {
            (*pos)++;
        }
    }
}

/* Parsear un array JSON [] */
static void parse_array(const char *buf, long *pos, long buf_size,
                        const char *current_path, IndexResult *result)
{
    (*pos)++; /* saltamos el [ */
    int index = 0;

    while (*pos < buf_size)
    {
        skip_whitespace(buf, pos, buf_size);

        if (buf[*pos] == ']')
        {
            (*pos)++;
            break;
        }

        /* El path para este elemento es current_path/0, /1, etc */
        char new_path[MAX_PATH_LEN];
        snprintf(new_path, sizeof(new_path), "%s/%d", current_path, index);

        /* Parsear el valor */
        parse_value(buf, pos, buf_size, new_path, result);
        index++;

        /* Avanzar la coma si hay */
        skip_whitespace(buf, pos, buf_size);
        if (*pos < buf_size && buf[*pos] == ',')
        {
            (*pos)++;
        }
    }
}

/* Parsear cualquier valor JSON y registrarlo en el indice */
static void parse_value(const char *buf, long *pos, long buf_size,
                        const char *current_path, IndexResult *result)
{
    skip_whitespace(buf, pos, buf_size);
    if (*pos >= buf_size)
        return;

    char c = buf[*pos];
    long inicio = *pos;
    long fin = *pos;

    if (c == '{')
    {
        fin = find_block_end(buf, *pos, buf_size);
        add_entry(result, current_path, inicio, fin);
        /* Ahora parsear el contenido del objeto para agregar sus hijos al indice */
        parse_object(buf, pos, buf_size, current_path, result);
    }
    else if (c == '[')
    {
        fin = find_block_end(buf, *pos, buf_size);
        add_entry(result, current_path, inicio, fin);
        /* Parsear el contenido del array */
        parse_array(buf, pos, buf_size, current_path, result);
    }
    else if (c == '"')
    {
        fin = find_string_end(buf, *pos, buf_size);
        add_entry(result, current_path, inicio, fin);
        *pos = fin + 1;
    }
    else if (isdigit((unsigned char)c) || c == '-')
    {
        fin = find_number_end(buf, *pos, buf_size);
        add_entry(result, current_path, inicio, fin);
        *pos = fin + 1;
    }
    else if (strncmp(buf + *pos, "true", 4) == 0)
    {
        fin = *pos + 3;
        add_entry(result, current_path, inicio, fin);
        *pos = fin + 1;
    }
    else if (strncmp(buf + *pos, "false", 5) == 0)
    {
        fin = *pos + 4;
        add_entry(result, current_path, inicio, fin);
        *pos = fin + 1;
    }
    else if (strncmp(buf + *pos, "null", 4) == 0)
    {
        fin = *pos + 3;
        add_entry(result, current_path, inicio, fin);
        *pos = fin + 1;
    }
    /* Si no reconoce el caracter, simplemente avanza (safe fallback) */
}

/* ------------------------------------------------------------------ */
/*  Funcion publica                                                     */
/* ------------------------------------------------------------------ */

int parse_json(const char *buffer, long buf_size, IndexResult *result)
{
    if (!buffer || buf_size <= 0 || !result)
        return -1;

    result->count = 0;
    long pos = 0;

    skip_whitespace(buffer, &pos, buf_size);
    if (pos >= buf_size)
        return -1;

    /* El JSON debe empezar con { o [ */
    char first = buffer[pos];
    if (first != '{' && first != '[')
    {
        fprintf(stderr, "ERROR: el JSON no empieza con { ni [\n");
        return -1;
    }

    /* Parsear desde la raiz - el path raiz es "" (vacio, los hijos seran /clave) */
    parse_value(buffer, &pos, buf_size, "", result);

    return 0;
}