#include "network.h"
#include "protocol.h"

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int fd;
    uint64_t local_node_id;
    char host[128];
    char port[32];
} peer_connection_context_t;

typedef struct {
    int listen_fd;
    uint64_t local_node_id;
    char node_name[128];
    char listen_port[32];
} listener_context_t;

static volatile sig_atomic_t keep_running = 1;

static void handle_signal(int signal_number)
{
    (void)signal_number;
    keep_running = 0;
}

static void usage(const char *program)
{
    fprintf(stderr,
            "Uso:\n"
            "  %s <porta-local> <nome-do-no>\n"
            "  %s <porta-local> <nome-do-no> <host-remoto> <porta-remota> <mensagem>\n\n"
            "Exemplo:\n"
            "  %s 9001 peer-a\n"
            "  %s 9002 peer-b 127.0.0.1 9001 \"JOIN de peer-b\"\n",
            program,
            program,
            program,
            program);
}

static char *join_arguments(int argc, char **argv, int start)
{
    int i;
    size_t size = 1;
    char *joined;
    char *cursor;

    for (i = start; i < argc; i++) {
        size += strlen(argv[i]) + 1u;
    }

    joined = calloc(size, 1u);
    if (joined == NULL) {
        return NULL;
    }

    cursor = joined;
    for (i = start; i < argc; i++) {
        size_t len = strlen(argv[i]);
        if (i > start) {
            *cursor = ' ';
            cursor++;
        }
        memcpy(cursor, argv[i], len);
        cursor += len;
    }

    return joined;
}

static void strip_newline(char *text)
{
    size_t length;

    if (text == NULL) {
        return;
    }

    length = strlen(text);
    while (length > 0 && (text[length - 1u] == '\n' || text[length - 1u] == '\r')) {
        text[length - 1u] = '\0';
        length--;
    }
}

static void print_payload(const protocol_message_t *message)
{
    if (message->header.payload_size == 0) {
        printf("payload: <vazio>\n");
        return;
    }

    printf("payload (%" PRIu32 " bytes): ", message->header.payload_size);
    fwrite(message->payload, 1u, message->header.payload_size, stdout);
    printf("\n");
}

static void *peer_connection_thread(void *arg)
{
    peer_connection_context_t *ctx = (peer_connection_context_t *)arg;

    printf("[inbound] conexao aceita de %s:%s\n", ctx->host, ctx->port);

    while (1) {
        protocol_message_t message;
        int rc = protocol_recv_message(ctx->fd, &message);

        if (rc == PROTOCOL_ERR_CLOSED) {
            printf("[inbound] peer remoto %s:%s encerrou a conexao\n", ctx->host, ctx->port);
            break;
        }

        if (rc != PROTOCOL_OK) {
            fprintf(stderr,
                    "[inbound] erro ao receber de %s:%s: %s\n",
                    ctx->host,
                    ctx->port,
                    protocol_error_string(rc));
            break;
        }

        printf("[inbound] header recebido: ");
        protocol_dump_header(stdout, &message.header);
        print_payload(&message);

        {
            const char ack[] = "ACK: mensagem recebida e checksum validado";
            rc = protocol_send_message(ctx->fd,
                                       MSG_ACK,
                                       ctx->local_node_id,
                                       message.header.source_node,
                                       ack,
                                       (uint32_t)strlen(ack));
            if (rc != PROTOCOL_OK) {
                fprintf(stderr, "[inbound] erro ao enviar ACK: %s\n", protocol_error_string(rc));
                protocol_free_message(&message);
                break;
            }
        }

        protocol_free_message(&message);
    }

    network_close(ctx->fd);
    free(ctx);
    return NULL;
}

static int accept_and_spawn(int listen_fd, uint64_t local_node_id)
{
    peer_connection_context_t *ctx = calloc(1u, sizeof(*ctx));
    pthread_t thread;
    int status;

    if (ctx == NULL) {
        fprintf(stderr, "[listen] sem memoria para aceitar novo peer\n");
        return -1;
    }

    ctx->local_node_id = local_node_id;
    ctx->fd = network_accept(listen_fd,
                             ctx->host,
                             sizeof(ctx->host),
                             ctx->port,
                             sizeof(ctx->port));
    if (ctx->fd < 0) {
        free(ctx);
        if (errno != EINTR) {
            fprintf(stderr, "[listen] accept falhou: %s\n", network_last_error());
        }
        return -1;
    }

    status = pthread_create(&thread, NULL, peer_connection_thread, ctx);
    if (status != 0) {
        fprintf(stderr, "[listen] pthread_create falhou: %s\n", strerror(status));
        network_close(ctx->fd);
        free(ctx);
        return -1;
    }

    pthread_detach(thread);
    return 0;
}

static void *listener_thread(void *arg)
{
    listener_context_t *ctx = (listener_context_t *)arg;
    int listen_fd = ctx->listen_fd;

    printf("[node] '%s' node_id=0x%016" PRIx64 " escutando na porta %s\n",
           ctx->node_name,
           ctx->local_node_id,
           ctx->listen_port);

    while (keep_running) {
        accept_and_spawn(listen_fd, ctx->local_node_id);
    }

    network_close(listen_fd);
    free(ctx);
    return NULL;
}

static int start_listener(const char *port, const char *node_name, uint64_t node_id)
{
    listener_context_t *ctx = calloc(1u, sizeof(*ctx));
    pthread_t thread;
    int listen_fd;
    int status;

    if (ctx == NULL) {
        fprintf(stderr, "[node] sem memoria para iniciar listener\n");
        return -1;
    }

    listen_fd = network_listen(port, NETWORK_DEFAULT_BACKLOG);
    if (listen_fd < 0) {
        fprintf(stderr, "[node] erro ao iniciar listener: %s\n", network_last_error());
        free(ctx);
        return -1;
    }

    ctx->listen_fd = listen_fd;
    ctx->local_node_id = node_id;
    snprintf(ctx->node_name, sizeof(ctx->node_name), "%s", node_name);
    snprintf(ctx->listen_port, sizeof(ctx->listen_port), "%s", port);

    status = pthread_create(&thread, NULL, listener_thread, ctx);
    if (status != 0) {
        fprintf(stderr, "[node] pthread_create falhou: %s\n", strerror(status));
        network_close(listen_fd);
        free(ctx);
        return -1;
    }

    pthread_detach(thread);
    return 0;
}

static int send_peer_message(const char *host,
                             const char *port,
                             uint64_t node_id,
                             const char *node_name,
                             const char *message_text)
{
    int fd;
    int rc;
    protocol_message_t response;

    fd = network_connect(host, port);
    if (fd < 0) {
        fprintf(stderr, "erro ao conectar: %s\n", network_last_error());
        return 1;
    }

    printf("[outbound] no '%s' node_id=0x%016" PRIx64 "\n", node_name, node_id);
    printf("[outbound] conectado em %s:%s\n", host, port);

    rc = protocol_send_message(fd,
                               MSG_JOIN,
                               node_id,
                               0u,
                               message_text,
                               (uint32_t)strlen(message_text));
    if (rc != PROTOCOL_OK) {
        fprintf(stderr, "[outbound] erro ao enviar mensagem: %s\n", protocol_error_string(rc));
        network_close(fd);
        return 1;
    }

    printf("[outbound] mensagem enviada com framing e checksum\n");

    rc = protocol_recv_message(fd, &response);
    if (rc != PROTOCOL_OK) {
        fprintf(stderr, "[outbound] erro ao receber resposta: %s\n", protocol_error_string(rc));
        network_close(fd);
        return 1;
    }

    printf("[outbound] header recebido: ");
    protocol_dump_header(stdout, &response.header);
    print_payload(&response);

    protocol_free_message(&response);
    network_close(fd);
    return 0;
}

static int run_peer_node(const char *listen_port,
                         const char *node_name,
                         int argc,
                         char **argv)
{
    uint64_t node_id = protocol_node_id_from_string(node_name);
    char line[2048];

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGPIPE, SIG_IGN);

    if (start_listener(listen_port, node_name, node_id) != 0) {
        return 1;
    }

    if (argc > 0) {
        char *initial_message = join_arguments(argc, argv, 2);
        if (initial_message == NULL) {
            fprintf(stderr, "[node] erro: sem memoria para montar mensagem inicial\n");
            return 1;
        }
        (void)send_peer_message(argv[0], argv[1], node_id, node_name, initial_message);
        free(initial_message);
    }

    printf("[node] comandos disponiveis:\n");
    printf("       send <host> <porta> <mensagem>\n");
    printf("       quit\n");

    while (keep_running && fgets(line, sizeof(line), stdin) != NULL) {
        char *command;
        char *host;
        char *port;
        char *message;

        strip_newline(line);

        if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0) {
            keep_running = 0;
            break;
        }

        command = strtok(line, " \t");
        if (command == NULL) {
            continue;
        }

        if (strcmp(command, "send") != 0) {
            fprintf(stderr, "[node] comando desconhecido: %s\n", command);
            continue;
        }

        host = strtok(NULL, " \t");
        port = strtok(NULL, " \t");
        message = strtok(NULL, "");

        if (host == NULL || port == NULL || message == NULL || message[0] == '\0') {
            fprintf(stderr, "[node] uso: send <host> <porta> <mensagem>\n");
            continue;
        }

        while (*message == ' ' || *message == '\t') {
            message++;
        }

        (void)send_peer_message(host, port, node_id, node_name, message);
    }

    keep_running = 0;
    printf("[node] encerrado\n");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 3 && argc < 6) {
        usage(argv[0]);
        return 1;
    }

    return run_peer_node(argv[1], argv[2], argc - 3, argv + 3);
}
