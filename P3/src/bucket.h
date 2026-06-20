#ifndef BUCKET_H
#define BUCKET_H

#include <stdint.h>
#include <stdio.h>

#define MAGIC_NUMBER 0x424B5433  // "BKT3"
#define MAX_KEY_LEN 256

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t data_start;
    uint32_t free_list_start;
    uint32_t total_size;
    uint32_t entry_count;
} bucket_header_t;

typedef struct {
    char key[MAX_KEY_LEN];
    uint32_t data_offset;
    uint32_t data_size;
    uint8_t is_deleted;
} dir_entry_t;

typedef struct {
    uint32_t offset;
    uint32_t size;
} free_block_t;

int create_bucket(const char *bucket_name);
int open_bucket(const char *bucket_name, bucket_header_t *header);
int find_entry(const char *bucket_name, const char *key, dir_entry_t *entry);
int add_object(const char *bucket_name, const char *key, const void *data, size_t size);
int get_object(const char *bucket_name, const char *key, void **out_data, size_t *out_size);
int delete_object(const char *bucket_name, const char *key);
int delete_objects_by_prefix(const char *bucket_name, const char *prefix);

#endif