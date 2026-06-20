#include "bucket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Crea un nuevo bucket (archivo vacío con cabecera)
int create_bucket(const char *bucket_name) {
    char path[512];
    snprintf(path, sizeof(path), "./buckets/%s", bucket_name);

    // Verificar si ya existe
    FILE *f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return -1; // ya existe
    }

    f = fopen(path, "wb");
    if (!f) return -2;

    bucket_header_t header;
    header.magic = MAGIC_NUMBER;
    header.version = 1;
    header.data_start = sizeof(bucket_header_t);
    header.free_list_start = header.data_start;
    header.total_size = header.data_start;
    header.entry_count = 0;

    fwrite(&header, sizeof(header), 1, f);
    fclose(f);
    return 0;
}

int open_bucket(const char *bucket_name, bucket_header_t *header) {
    char path[512];
    snprintf(path, sizeof(path), "./buckets/%s", bucket_name);
    FILE *f = fopen(path, "rb+");
    if (!f) return -1;
    fread(header, sizeof(bucket_header_t), 1, f);
    if (header->magic != MAGIC_NUMBER) {
        fclose(f);
        return -2;
    }
    fclose(f);
    return 0;
}

int find_entry(const char *bucket_name, const char *key, dir_entry_t *entry) {
    char path[512];
    snprintf(path, sizeof(path), "./buckets/%s", bucket_name);
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    bucket_header_t header;
    fread(&header, sizeof(header), 1, f);
    if (header.magic != MAGIC_NUMBER) { fclose(f); return -2; }

    fseek(f, sizeof(header), SEEK_SET);
    dir_entry_t e;
    for (int i = 0; i < header.entry_count; i++) {
        fread(&e, sizeof(dir_entry_t), 1, f);
        if (!e.is_deleted && strcmp(e.key, key) == 0) {
            *entry = e;
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

int add_object(const char *bucket_name, const char *key, const void *data, size_t size) {
    char path[512];
    snprintf(path, sizeof(path), "./buckets/%s", bucket_name);
    FILE *f = fopen(path, "rb+");
    if (!f) return -1;

    bucket_header_t header;
    fread(&header, sizeof(header), 1, f);
    if (header.magic != MAGIC_NUMBER) { fclose(f); return -2; }

    // Buscar si la clave ya existe
    dir_entry_t existing_entry;
    int found = 0;
    long entry_pos = -1;
    fseek(f, sizeof(header), SEEK_SET);
    for (int i = 0; i < header.entry_count; i++) {
        long pos = ftell(f);
        dir_entry_t e;
        fread(&e, sizeof(dir_entry_t), 1, f);
        if (!e.is_deleted && strcmp(e.key, key) == 0) {
            found = 1;
            existing_entry = e;
            entry_pos = pos;
            break;
        }
    }

    // Si existe y el tamaño es igual, reemplazar en el mismo lugar
    if (found && existing_entry.data_size == size) {
        fseek(f, existing_entry.data_offset, SEEK_SET);
        fwrite(data, 1, size, f);
        fclose(f);
        return 0;
    }

    // Si existe pero tamaño diferente, marcar como eliminado y agregar hueco
    if (found) {
        fseek(f, entry_pos, SEEK_SET);
        dir_entry_t deleted = existing_entry;
        deleted.is_deleted = 1;
        fwrite(&deleted, sizeof(dir_entry_t), 1, f);

        // Agregar hueco a la lista de libres
        fseek(f, header.free_list_start, SEEK_SET);
        free_block_t fb;
        while (1) {
            fread(&fb, sizeof(free_block_t), 1, f);
            if (fb.size == 0) {
                free_block_t new_block = { existing_entry.data_offset, existing_entry.data_size };
                fseek(f, -sizeof(free_block_t), SEEK_CUR);
                fwrite(&new_block, sizeof(free_block_t), 1, f);
                free_block_t sentinel = { 0, 0 };
                fwrite(&sentinel, sizeof(free_block_t), 1, f);
                break;
            }
        }
    }

    // Asignar nuevo espacio (primer ajuste)
    fseek(f, header.free_list_start, SEEK_SET);
    free_block_t fb;
    long free_block_pos = -1;
    uint32_t offset = -1;
    while (1) {
        long pos = ftell(f);
        fread(&fb, sizeof(free_block_t), 1, f);
        if (fb.size == 0) break;
        if (fb.size >= size) {
            offset = fb.offset;
            free_block_pos = pos;
            break;
        }
    }

    if (offset != -1) {
        if (fb.size > size) {
            free_block_t rem = { fb.offset + size, fb.size - size };
            fseek(f, free_block_pos, SEEK_SET);
            fwrite(&rem, sizeof(free_block_t), 1, f);
        } else {
            fseek(f, free_block_pos, SEEK_SET);
            free_block_t sentinel = { 0, 0 };
            fwrite(&sentinel, sizeof(free_block_t), 1, f);
        }
    } else {
        fseek(f, 0, SEEK_END);
        offset = ftell(f);
        fwrite(data, 1, size, f);
        header.total_size = ftell(f);
    }

    // Escribir datos en la posición elegida
    fseek(f, offset, SEEK_SET);
    fwrite(data, 1, size, f);

    // Añadir nueva entrada en el directorio
    dir_entry_t new_entry;
    memset(&new_entry, 0, sizeof(dir_entry_t));
    strncpy(new_entry.key, key, MAX_KEY_LEN - 1);
    new_entry.data_offset = offset;
    new_entry.data_size = size;
    new_entry.is_deleted = 0;

    fseek(f, sizeof(header) + header.entry_count * sizeof(dir_entry_t), SEEK_SET);
    fwrite(&new_entry, sizeof(dir_entry_t), 1, f);

    // Actualizar cabecera
    header.entry_count++;
    fseek(f, 0, SEEK_SET);
    fwrite(&header, sizeof(header), 1, f);

    fclose(f);
    return 0;
}

int get_object(const char *bucket_name, const char *key, void **out_data, size_t *out_size) {
    dir_entry_t entry;
    int found = find_entry(bucket_name, key, &entry);
    if (found != 1) return -1;

    char path[512];
    snprintf(path, sizeof(path), "./buckets/%s", bucket_name);
    FILE *f = fopen(path, "rb");
    if (!f) return -2;

    *out_data = malloc(entry.data_size);
    fseek(f, entry.data_offset, SEEK_SET);
    fread(*out_data, 1, entry.data_size, f);
    *out_size = entry.data_size;
    fclose(f);
    return 0;
}

int delete_object(const char *bucket_name, const char *key) {
    char path[512];
    snprintf(path, sizeof(path), "./buckets/%s", bucket_name);
    FILE *f = fopen(path, "rb+");
    if (!f) return -2;

    bucket_header_t header;
    fread(&header, sizeof(header), 1, f);
    if (header.magic != MAGIC_NUMBER) { fclose(f); return -3; }

    // Buscar la entrada
    fseek(f, sizeof(header), SEEK_SET);
    dir_entry_t e;
    long entry_pos = -1;
    for (int i = 0; i < header.entry_count; i++) {
        long pos = ftell(f);
        fread(&e, sizeof(dir_entry_t), 1, f);
        if (!e.is_deleted && strcmp(e.key, key) == 0) {
            entry_pos = pos;
            break;
        }
    }
    if (entry_pos == -1) {
        fclose(f);
        return -1; // no encontrado
    }

    // Marcar como eliminado
    e.is_deleted = 1;
    fseek(f, entry_pos, SEEK_SET);
    fwrite(&e, sizeof(dir_entry_t), 1, f);

    // Agregar el espacio a la lista de libres
    fseek(f, header.free_list_start, SEEK_SET);
    free_block_t fb;
    while (1) {
        fread(&fb, sizeof(free_block_t), 1, f);
        if (fb.size == 0) {
            // Reemplazar centinela por este bloque
            free_block_t new_block = { e.data_offset, e.data_size };
            fseek(f, -sizeof(free_block_t), SEEK_CUR);
            fwrite(&new_block, sizeof(free_block_t), 1, f);
            free_block_t sentinel = { 0, 0 };
            fwrite(&sentinel, sizeof(free_block_t), 1, f);
            break;
        }
    }

    fclose(f);
    return 0;
}

int delete_objects_by_prefix(const char *bucket_name, const char *prefix) {
    char path[512];
    snprintf(path, sizeof(path), "./buckets/%s", bucket_name);
    FILE *f = fopen(path, "rb+");
    if (!f) return -2;

    bucket_header_t header;
    fread(&header, sizeof(header), 1, f);
    if (header.magic != MAGIC_NUMBER) { fclose(f); return -3; }

    // Primera pasada: recopilar entradas que coinciden con el prefijo
    fseek(f, sizeof(header), SEEK_SET);
    dir_entry_t *entries = malloc(header.entry_count * sizeof(dir_entry_t));
    long *positions = malloc(header.entry_count * sizeof(long));
    int count = 0;
    for (int i = 0; i < header.entry_count; i++) {
        long pos = ftell(f);
        dir_entry_t e;
        fread(&e, sizeof(dir_entry_t), 1, f);
        if (!e.is_deleted && strncmp(e.key, prefix, strlen(prefix)) == 0) {
            entries[count] = e;
            positions[count] = pos;
            count++;
        }
    }

    if (count == 0) {
        free(entries);
        free(positions);
        fclose(f);
        return -1; // no se encontraron objetos
    }

    // Segunda pasada: marcar como eliminados y agregar huecos
    for (int i = 0; i < count; i++) {
        // Marcar entrada
        fseek(f, positions[i], SEEK_SET);
        dir_entry_t e = entries[i];
        e.is_deleted = 1;
        fwrite(&e, sizeof(dir_entry_t), 1, f);

        // Agregar hueco a la lista de libres
        fseek(f, header.free_list_start, SEEK_SET);
        free_block_t fb;
        while (1) {
            fread(&fb, sizeof(free_block_t), 1, f);
            if (fb.size == 0) {
                free_block_t new_block = { e.data_offset, e.data_size };
                fseek(f, -sizeof(free_block_t), SEEK_CUR);
                fwrite(&new_block, sizeof(free_block_t), 1, f);
                free_block_t sentinel = { 0, 0 };
                fwrite(&sentinel, sizeof(free_block_t), 1, f);
                break;
            }
        }
    }

    free(entries);
    free(positions);
    fclose(f);
    return 0;
}