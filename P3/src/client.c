#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8081
#define BUFFER_SIZE 4096

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <comando> [argumentos...]\n", argv[0]);
        fprintf(stderr, "Ejemplos:\n");
        fprintf(stderr, "  %s ls\n", argv[0]);
        fprintf(stderr, "  %s ls s3://mi-bucket/\n", argv[0]);
        fprintf(stderr, "  %s mb s3://mi-bucket\n", argv[0]);
        fprintf(stderr, "  %s cp archivo.txt s3://mi-bucket/remoto.txt\n", argv[0]);
        fprintf(stderr, "  %s cp s3://mi-bucket/remoto.txt ./descarga.txt\n", argv[0]);
        fprintf(stderr, "  %s rm s3://mi-bucket/remoto.txt\n", argv[0]);
        fprintf(stderr, "  %s rm s3://mi-bucket/carpeta/ --recursive\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Construir el mensaje a enviar (unir todos los argumentos)
    char message[BUFFER_SIZE] = {0};
    for (int i = 1; i < argc; i++) {
        strcat(message, argv[i]);
        if (i < argc - 1) strcat(message, " ");
    }

    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("invalid address");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connection failed");
        return -1;
    }

    // Enviar comando
    send(sock, message, strlen(message), 0);

    // Determinar el tipo de comando
    char *cmd = argv[1];

    // ========== cp subida ==========
    if (strcmp(cmd, "cp") == 0 && argc >= 4 && strncmp(argv[2], "s3://", 5) != 0) {
        // Subida: esperar "READY"
        char ready_buf[10];
        int r = recv(sock, ready_buf, 5, MSG_WAITALL);
        if (r != 5 || strncmp(ready_buf, "READY", 5) != 0) {
            fprintf(stderr, "No se recibió READY del servidor\n");
            close(sock);
            return -1;
        }

        const char *local_file = argv[2];
        FILE *f = fopen(local_file, "rb");
        if (!f) {
            perror("No se pudo abrir el archivo local");
            close(sock);
            return -1;
        }
        fseek(f, 0, SEEK_END);
        long file_size = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *data = malloc(file_size);
        fread(data, 1, file_size, f);
        fclose(f);

        uint32_t net_size = htonl(file_size);
        send(sock, &net_size, 4, 0);
        send(sock, data, file_size, 0);
        free(data);

        // Esperar respuesta final del servidor (OK o error)
        char final_resp[64];
        r = recv(sock, final_resp, sizeof(final_resp) - 1, 0);
        if (r > 0) {
            final_resp[r] = '\0';
            printf("%s", final_resp);
        }
        close(sock);
        return 0;
    }

    // ========== cp bajada ==========
    if (strcmp(cmd, "cp") == 0 && argc >= 4 && strncmp(argv[2], "s3://", 5) == 0) {
        // Leer byte de estado
        unsigned char status;
        int recv_status = recv(sock, &status, 1, MSG_WAITALL);
        if (recv_status != 1) {
            fprintf(stderr, "Error al recibir estado del servidor\n");
            close(sock);
            return -1;
        }

        if (status == 0) {
            // Error: leer mensaje de texto
            char err_buf[256];
            int err_bytes = recv(sock, err_buf, sizeof(err_buf) - 1, 0);
            if (err_bytes > 0) {
                err_buf[err_bytes] = '\0';
                fprintf(stderr, "%s", err_buf);
            } else {
                fprintf(stderr, "Error desconocido del servidor\n");
            }
            close(sock);
            return -1;
        }

        // Éxito: recibir tamaño (4 bytes)
        uint32_t net_size;
        int recv_bytes = recv(sock, &net_size, 4, MSG_WAITALL);
        if (recv_bytes != 4) {
            fprintf(stderr, "Error al recibir tamaño del archivo\n");
            close(sock);
            return -1;
        }
        uint32_t data_size = ntohl(net_size);

        char *data = malloc(data_size);
        if (!data) {
            fprintf(stderr, "Error de memoria\n");
            close(sock);
            return -1;
        }
        int data_received = 0;
        while (data_received < data_size) {
            int r = recv(sock, data + data_received, data_size - data_received, 0);
            if (r <= 0) break;
            data_received += r;
        }
        if (data_received != data_size) {
            fprintf(stderr, "Error al recibir el archivo completo\n");
            free(data);
            close(sock);
            return -1;
        }

        const char *dst_file = argv[3];
        FILE *out = fopen(dst_file, "wb");
        if (!out) {
            perror("Error al crear archivo destino");
            free(data);
            close(sock);
            return -1;
        }
        fwrite(data, 1, data_size, out);
        fclose(out);
        free(data);
        printf("Archivo descargado: %s\n", dst_file);
        close(sock);
        return 0;
    }

    // ========== Comandos simples: ls, mb, rm (sin --recursive) ==========
    // Leer respuesta como texto hasta que se cierre
    char buffer[BUFFER_SIZE];
    int total = 0;
    int r;
    while ((r = recv(sock, buffer + total, BUFFER_SIZE - total - 1, 0)) > 0) {
        total += r;
        if (total >= BUFFER_SIZE - 1) break;
    }
    buffer[total] = '\0';
    printf("%s", buffer);
    close(sock);
    return 0;
}