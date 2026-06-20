#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <dirent.h>
#include "bucket.h"

#define PORT 8081
#define BUFFER_SIZE 4096

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        close(client_socket);
        return;
    }
    buffer[bytes_read] = '\0';
    printf("Comando recibido: %s\n", buffer);

    char *cmd = strtok(buffer, " ");
    if (!cmd) {
        send(client_socket, "Comando vacío\n", 14, 0);
        close(client_socket);
        return;
    }

    // ==================== ls ====================
    if (strcmp(cmd, "ls") == 0) {
        char *arg = strtok(NULL, " ");
        if (arg && strncmp(arg, "s3://", 5) == 0) {
            // Listar objetos dentro de un bucket/prefix
            char *bucket_name = arg + 5;
            char *prefix = "";
            char *slash = strchr(bucket_name, '/');
            if (slash) {
                *slash = '\0';
                prefix = slash + 1;
            }
            char path[512];
            snprintf(path, sizeof(path), "./buckets/%s", bucket_name);
            FILE *f = fopen(path, "rb");
            if (!f) {
                send(client_socket, "Bucket no encontrado.\n", 22, 0);
                close(client_socket);
                return;
            }
            bucket_header_t header;
            fread(&header, sizeof(header), 1, f);
            if (header.magic != MAGIC_NUMBER) {
                fclose(f);
                send(client_socket, "Bucket corrupto.\n", 17, 0);
                close(client_socket);
                return;
            }
            fseek(f, sizeof(header), SEEK_SET);
            char response[BUFFER_SIZE] = {0};
            snprintf(response, sizeof(response), "Objetos en s3://%s/%s:\n", bucket_name, prefix);
            dir_entry_t e;
            for (int i = 0; i < header.entry_count; i++) {
                fread(&e, sizeof(dir_entry_t), 1, f);
                if (e.is_deleted) continue;
                if (strncmp(e.key, prefix, strlen(prefix)) == 0) {
                    char line[512];
                    snprintf(line, sizeof(line), " - %s (%u bytes)\n", e.key, e.data_size);
                    strncat(response, line, BUFFER_SIZE - strlen(response) - 1);
                    if (strlen(response) >= BUFFER_SIZE - 100) break;
                }
            }
            fclose(f);
            send(client_socket, response, strlen(response), 0);
        } else {
            // Listar buckets
            DIR *d = opendir("./buckets");
            if (d) {
                struct dirent *dir;
                char response[BUFFER_SIZE] = "Buckets disponibles:\n";
                while ((dir = readdir(d)) != NULL) {
                    if (dir->d_type == DT_REG) {
                        strcat(response, " - ");
                        strcat(response, dir->d_name);
                        strcat(response, "\n");
                    }
                }
                closedir(d);
                send(client_socket, response, strlen(response), 0);
            } else {
                char *err = "No hay buckets o carpeta buckets/ no existe.\n";
                send(client_socket, err, strlen(err), 0);
            }
        }
        close(client_socket);
        return;
    }

    // ==================== mb ====================
    if (strcmp(cmd, "mb") == 0) {
        char *arg = strtok(NULL, " ");
        if (!arg || strncmp(arg, "s3://", 5) != 0) {
            send(client_socket, "Uso: mb s3://nombre-bucket\n", 28, 0);
            close(client_socket);
            return;
        }
        char *bucket_name = arg + 5;
        char *p = bucket_name + strlen(bucket_name) - 1;
        while (p > bucket_name && *p == '/') { *p = '\0'; p--; }
        int result = create_bucket(bucket_name);
        if (result == 0) {
            send(client_socket, "Bucket creado exitosamente.\n", 28, 0);
        } else if (result == -1) {
            send(client_socket, "Error: El bucket ya existe.\n", 28, 0);
        } else {
            send(client_socket, "Error: No se pudo crear el bucket.\n", 34, 0);
        }
        close(client_socket);
        return;
    }

    // ==================== cp ====================
    if (strcmp(cmd, "cp") == 0) {
        char *src = strtok(NULL, " ");
        char *dst = strtok(NULL, " ");
        if (!src || !dst) {
            send(client_socket, "Uso: cp <origen> <destino>\n", 28, 0);
            close(client_socket);
            return;
        }

        int is_upload = (strncmp(src, "s3://", 5) != 0);

        if (is_upload) {
            // Subida
            if (strncmp(dst, "s3://", 5) != 0) {
                send(client_socket, "Destino debe ser s3://...\n", 26, 0);
                close(client_socket);
                return;
            }
            char *bucket_name = dst + 5;
            char *key = strchr(bucket_name, '/');
            if (!key) {
                send(client_socket, "Formato: s3://bucket/ruta\n", 26, 0);
                close(client_socket);
                return;
            }
            *key = '\0';
            key++;

            send(client_socket, "READY", 5, 0);

            uint32_t net_size;
            int recv_bytes = recv(client_socket, &net_size, 4, MSG_WAITALL);
            if (recv_bytes != 4) {
                send(client_socket, "Error recibiendo tamaño\n", 24, 0);
                close(client_socket);
                return;
            }
            uint32_t file_size = ntohl(net_size);
            char *data = malloc(file_size);
            recv_bytes = recv(client_socket, data, file_size, MSG_WAITALL);
            if (recv_bytes != file_size) {
                free(data);
                send(client_socket, "Error recibiendo datos\n", 24, 0);
                close(client_socket);
                return;
            }

            int result = add_object(bucket_name, key, data, file_size);
            free(data);
            if (result == 0) {
                send(client_socket, "OK\n", 3, 0);
            } else {
                send(client_socket, "Error al guardar en bucket\n", 27, 0);
            }
        } else {
            // Bajada
            if (strncmp(src, "s3://", 5) != 0) {
                send(client_socket, "Origen debe ser s3://...\n", 26, 0);
                close(client_socket);
                return;
            }
            char *bucket_name = src + 5;
            char *key = strchr(bucket_name, '/');
            if (!key) {
                send(client_socket, "Formato: s3://bucket/ruta\n", 26, 0);
                close(client_socket);
                return;
            }
            *key = '\0';
            key++;

            void *data;
            size_t data_size;
            int result = get_object(bucket_name, key, &data, &data_size);
            if (result != 0) {
                char status = 0;
                send(client_socket, &status, 1, 0);
                char *err_msg = "Objeto no encontrado\n";
                send(client_socket, err_msg, strlen(err_msg), 0);
                close(client_socket);
                return;
            }

            char status = 1;
            send(client_socket, &status, 1, 0);
            uint32_t net_size = htonl(data_size);
            send(client_socket, &net_size, 4, 0);
            send(client_socket, data, data_size, 0);
            free(data);
        }
        close(client_socket);
        return;
    }

    // ==================== rm ====================
    if (strcmp(cmd, "rm") == 0) {
        char *arg1 = strtok(NULL, " ");
        char *arg2 = strtok(NULL, " ");
        if (!arg1 || strncmp(arg1, "s3://", 5) != 0) {
            send(client_socket, "Uso: rm s3://bucket/ruta [--recursive]\n", 40, 0);
            close(client_socket);
            return;
        }
        int recursive = 0;
        if (arg2 && strcmp(arg2, "--recursive") == 0) {
            recursive = 1;
        }
        char *bucket_name = arg1 + 5;
        char *key = strchr(bucket_name, '/');
        if (!key) {
            send(client_socket, "Formato: s3://bucket/ruta\n", 26, 0);
            close(client_socket);
            return;
        }
        *key = '\0';
        key++;

        int result;
        if (recursive) {
            result = delete_objects_by_prefix(bucket_name, key);
        } else {
            result = delete_object(bucket_name, key);
        }

        if (result == 0) {
            send(client_socket, "Operación completada.\n", 22, 0);
        } else if (result == -1) {
            send(client_socket, "No se encontraron objetos.\n", 27, 0);
        } else {
            send(client_socket, "Error en la operación.\n", 24, 0);
        }
        close(client_socket);
        return;
    }

    // ==================== mv ====================
    if (strcmp(cmd, "mv") == 0) {
        char *src = strtok(NULL, " ");
        char *dst = strtok(NULL, " ");
        if (!src || !dst) {
            send(client_socket, "Uso: mv <origen> <destino>\n", 28, 0);
            close(client_socket);
            return;
        }
        // Solo implementamos mv entre S3
        if (strncmp(src, "s3://", 5) != 0 || strncmp(dst, "s3://", 5) != 0) {
            send(client_socket, "mv entre local y S3 no implementado (use cp + rm manual)\n", 58, 0);
            close(client_socket);
            return;
        }
        // Extraer bucket y key de origen
        char *src_bucket = src + 5;
        char *src_key = strchr(src_bucket, '/');
        if (!src_key) {
            send(client_socket, "Formato origen: s3://bucket/ruta\n", 33, 0);
            close(client_socket);
            return;
        }
        *src_key = '\0';
        src_key++;

        // Extraer bucket y key de destino
        char *dst_bucket = dst + 5;
        char *dst_key = strchr(dst_bucket, '/');
        if (!dst_key) {
            send(client_socket, "Formato destino: s3://bucket/ruta\n", 34, 0);
            close(client_socket);
            return;
        }
        *dst_key = '\0';
        dst_key++;

        // 1. Obtener el objeto de origen
        void *data;
        size_t data_size;
        int result = get_object(src_bucket, src_key, &data, &data_size);
        if (result != 0) {
            send(client_socket, "Origen no encontrado\n", 22, 0);
            close(client_socket);
            return;
        }

        // 2. Agregar el objeto en destino
        int add_result = add_object(dst_bucket, dst_key, data, data_size);
        free(data);
        if (add_result != 0) {
            send(client_socket, "Error al copiar a destino\n", 26, 0);
            close(client_socket);
            return;
        }

        // 3. Eliminar el origen
        int del_result = delete_object(src_bucket, src_key);
        if (del_result != 0) {
            send(client_socket, "Movido (copia OK, pero no se pudo eliminar origen)\n", 52, 0);
        } else {
            send(client_socket, "Movido exitosamente\n", 20, 0);
        }
        close(client_socket);
        return;
    }

    // ==================== rb ====================
    if (strcmp(cmd, "rb") == 0) {
        char *arg1 = strtok(NULL, " ");
        char *arg2 = strtok(NULL, " ");
        if (!arg1 || strncmp(arg1, "s3://", 5) != 0) {
            send(client_socket, "Uso: rb s3://bucket [--force]\n", 30, 0);
            close(client_socket);
            return;
        }
        int force = 0;
        if (arg2 && strcmp(arg2, "--force") == 0) {
            force = 1;
        }
        char *bucket_name = arg1 + 5;
        // Eliminar barras finales
        char *p = bucket_name + strlen(bucket_name) - 1;
        while (p > bucket_name && *p == '/') { *p = '\0'; p--; }

        // Verificar si el bucket existe
        char path[512];
        snprintf(path, sizeof(path), "./buckets/%s", bucket_name);
        FILE *f = fopen(path, "rb");
        if (!f) {
            send(client_socket, "Bucket no existe.\n", 18, 0);
            close(client_socket);
            return;
        }
        fclose(f);

        if (force) {
            // Eliminar todos los objetos (prefijo vacío)
            int del_result = delete_objects_by_prefix(bucket_name, "");
            if (del_result != 0 && del_result != -1) {
                send(client_socket, "Error al vaciar el bucket.\n", 26, 0);
                close(client_socket);
                return;
            }
            // Eliminar el archivo del bucket
            if (remove(path) == 0) {
                send(client_socket, "Bucket eliminado (con --force).\n", 32, 0);
            } else {
                send(client_socket, "Error al eliminar el archivo del bucket.\n", 40, 0);
            }
        } else {
            // Verificar si el bucket está vacío
            bucket_header_t header;
            f = fopen(path, "rb");
            fread(&header, sizeof(header), 1, f);
            fclose(f);
            if (header.magic != MAGIC_NUMBER) {
                send(client_socket, "Bucket corrupto.\n", 17, 0);
                close(client_socket);
                return;
            }
            // Contar objetos activos
            f = fopen(path, "rb");
            fseek(f, sizeof(header), SEEK_SET);
            int active_count = 0;
            dir_entry_t e;
            for (int i = 0; i < header.entry_count; i++) {
                fread(&e, sizeof(dir_entry_t), 1, f);
                if (!e.is_deleted) active_count++;
            }
            fclose(f);
            if (active_count > 0) {
                send(client_socket, "Bucket no vacío. Use --force para eliminar.\n", 43, 0);
            } else {
                if (remove(path) == 0) {
                    send(client_socket, "Bucket eliminado.\n", 18, 0);
                } else {
                    send(client_socket, "Error al eliminar el bucket.\n", 29, 0);
                }
            }
        }
        close(client_socket);
        return;
    }

    // ==================== Comando no reconocido ====================
    char *resp = "Comando no implementado.\n";
    send(client_socket, resp, strlen(resp), 0);
    close(client_socket);
}

int main(int argc, char *argv[]) {
    mkdir("./buckets", 0777);

    int server_fd, client_fd;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Servidor AWS-S3 escuchando en el puerto %d\n", PORT);

    while (1) {
        if ((client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
            perror("accept");
            continue;
        }
        printf("Cliente conectado desde %s:%d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
        handle_client(client_fd);
    }

    close(server_fd);
    return 0;
}