#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#include "bridge_client.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


static int executable_available(const char *program)
{
    const char *path;
    const char *start;
    const char *end;
    char candidate[1024];

    if (program == NULL || program[0] == '\0')
        return 0;

    if (strchr(program, '/') != NULL)
        return access(program, X_OK) == 0;

    path = getenv("PATH");
    if (path == NULL || path[0] == '\0')
        path = "/usr/local/bin:/usr/bin:/bin";

    start = path;
    for (;;) {
        size_t dir_len;
        end = strchr(start, ':');
        dir_len = end != NULL ? (size_t)(end - start) : strlen(start);
        if (dir_len == 0) {
            if (snprintf(candidate, sizeof(candidate), "./%s", program) < (int)sizeof(candidate) &&
                access(candidate, X_OK) == 0)
                return 1;
        } else if (dir_len + 1 + strlen(program) + 1 <= sizeof(candidate)) {
            memcpy(candidate, start, dir_len);
            candidate[dir_len] = '/';
            strcpy(candidate + dir_len + 1, program);
            if (access(candidate, X_OK) == 0)
                return 1;
        }
        if (end == NULL)
            break;
        start = end + 1;
    }
    return 0;
}

static void sleep_ms(unsigned int ms)
{
    struct timespec delay;
    delay.tv_sec = ms / 1000;
    delay.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&delay, NULL);
}

static void set_socket_io_timeout(int socket_fd)
{
    struct timeval timeout;
    const char *setting = getenv("MOTIF_APPLE_MUSIC_BRIDGE_TIMEOUT_MS");
    long timeout_ms = 3000;
    char *end = NULL;

    if (setting != NULL && setting[0] != '\0') {
        long parsed = strtol(setting, &end, 10);
        if (end != setting && *end == '\0' && parsed >= 250 && parsed <= 60000)
            timeout_ms = parsed;
    }
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

static int parse_http_url(const char *url, char *host, size_t host_size,
                          char *port, size_t port_size, char *path, size_t path_size)
{
    const char *cursor, *slash, *colon;
    size_t host_len, port_len;
    if (url == NULL || strncmp(url, "http://", 7) != 0) return -1;
    cursor = url + 7;
    slash = strchr(cursor, '/');
    if (slash == NULL) slash = cursor + strlen(cursor);
    colon = memchr(cursor, ':', (size_t)(slash - cursor));
    if (colon != NULL) {
        host_len = (size_t)(colon - cursor);
        port_len = (size_t)(slash - colon - 1);
        if (port_len == 0 || port_len >= port_size) return -1;
        memcpy(port, colon + 1, port_len);
        port[port_len] = '\0';
    } else {
        host_len = (size_t)(slash - cursor);
        snprintf(port, port_size, "80");
    }
    if (host_len == 0 || host_len >= host_size) return -1;
    memcpy(host, cursor, host_len);
    host[host_len] = '\0';
    snprintf(path, path_size, "%s", *slash != '\0' ? slash : "/");
    return 0;
}

static int send_all(int socket_fd, const void *buffer, size_t length)
{
    const unsigned char *cursor = (const unsigned char *)buffer;
    while (length > 0) {
        ssize_t written = send(socket_fd, cursor, length, 0);
        if (written <= 0) return -1;
        cursor += written;
        length -= (size_t)written;
    }
    return 0;
}

void bridge_client_init(BridgeClient *client)
{
    const char *url = getenv("MOTIF_APPLE_MUSIC_BRIDGE_URL");
    memset(client, 0, sizeof(*client));
    client->child_pid = -1;
    snprintf(client->base_url, sizeof(client->base_url), "%s",
             (url != NULL && url[0] != '\0') ? url : "http://127.0.0.1:17876");
}

int bridge_client_request(BridgeClient *client, const char *method, const char *path,
                          const char *body, char *response, size_t response_size)
{
    char host[256], port[16], base_path[512], request_path[1024];
    char request[8192], header[4096];
    struct addrinfo hints, *addresses = NULL, *item;
    int socket_fd = -1, status = 0, content_length = -1;
    size_t used = 0;
    ssize_t count;
    char *body_start;

    if (parse_http_url(client->base_url, host, sizeof(host), port, sizeof(port),
                       base_path, sizeof(base_path)) != 0) return -1;
    snprintf(request_path, sizeof(request_path), "%s%s",
             strcmp(base_path, "/") == 0 ? "" : base_path, path);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &addresses) != 0) return -1;
    for (item = addresses; item != NULL; item = item->ai_next) {
        socket_fd = socket(item->ai_family, item->ai_socktype, item->ai_protocol);
        if (socket_fd >= 0) set_socket_io_timeout(socket_fd);
        if (socket_fd >= 0 && connect(socket_fd, item->ai_addr, item->ai_addrlen) == 0) break;
        if (socket_fd >= 0) close(socket_fd);
        socket_fd = -1;
    }
    freeaddrinfo(addresses);
    if (socket_fd < 0) return -1;

    snprintf(request, sizeof(request),
             "%s %s HTTP/1.1\r\nHost: %s:%s\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n%s",
             method, request_path, host, port, body != NULL ? strlen(body) : 0,
             body != NULL ? body : "");
    if (send_all(socket_fd, request, strlen(request)) != 0) {
        close(socket_fd);
        return -1;
    }

    while ((count = recv(socket_fd, response + used,
                         response_size > used + 1 ? response_size - used - 1 : 0, 0)) > 0) {
        used += (size_t)count;
        if (used + 1 >= response_size) break;
    }
    close(socket_fd);
    if (response_size == 0) return -1;
    response[used] = '\0';

    body_start = strstr(response, "\r\n\r\n");
    if (body_start == NULL) return -1;
    if (sscanf(response, "HTTP/%*s %d", &status) != 1) return -1;
    {
        char *length_header = strstr(response, "Content-Length:");
        if (length_header == NULL) length_header = strstr(response, "content-length:");
        if (length_header != NULL) sscanf(length_header, "%*[^:]: %d", &content_length);
    }
    body_start += 4;
    if (content_length >= 0 && (size_t)content_length + 1 < response_size) {
        memmove(response, body_start, (size_t)content_length);
        response[content_length] = '\0';
    } else {
        size_t body_length = used - (size_t)(body_start - response);
        memmove(response, body_start, body_length);
        response[body_length] = '\0';
    }
    return status >= 200 && status < 300 ? 0 : -1;
}

int bridge_client_request_bytes(BridgeClient *client, const char *method, const char *path,
                                const char *body, unsigned char **response,
                                size_t *response_size)
{
    char host[256], port[16], base_path[512], request_path[1024];
    char request[8192], header[8192];
    struct addrinfo hints, *addresses = NULL, *item;
    unsigned char *buffer = NULL;
    size_t capacity = 0, used = 0, header_size = 0, body_size = 0;
    int socket_fd = -1, status = 0, content_length = -1;
    ssize_t count;

    (void)header;
    if (response == NULL || response_size == NULL) return -1;
    *response = NULL;
    *response_size = 0;

    if (parse_http_url(client->base_url, host, sizeof(host), port, sizeof(port),
                       base_path, sizeof(base_path)) != 0) return -1;
    snprintf(request_path, sizeof(request_path), "%s%s",
             strcmp(base_path, "/") == 0 ? "" : base_path, path);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &addresses) != 0) return -1;
    for (item = addresses; item != NULL; item = item->ai_next) {
        socket_fd = socket(item->ai_family, item->ai_socktype, item->ai_protocol);
        if (socket_fd >= 0) set_socket_io_timeout(socket_fd);
        if (socket_fd >= 0 && connect(socket_fd, item->ai_addr, item->ai_addrlen) == 0) break;
        if (socket_fd >= 0) close(socket_fd);
        socket_fd = -1;
    }
    freeaddrinfo(addresses);
    if (socket_fd < 0) return -1;

    snprintf(request, sizeof(request),
             "%s %s HTTP/1.1\r\nHost: %s:%s\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n%s",
             method, request_path, host, port, body != NULL ? strlen(body) : 0,
             body != NULL ? body : "");
    if (send_all(socket_fd, request, strlen(request)) != 0) {
        close(socket_fd);
        return -1;
    }

    capacity = 65536;
    buffer = malloc(capacity);
    if (buffer == NULL) {
        close(socket_fd);
        return -1;
    }
    while ((count = recv(socket_fd, buffer + used, capacity - used, 0)) > 0) {
        used += (size_t)count;
        if (used == capacity) {
            unsigned char *grown;
            if (capacity > 16 * 1024 * 1024) break;
            capacity *= 2;
            grown = realloc(buffer, capacity);
            if (grown == NULL) break;
            buffer = grown;
        }
    }
    close(socket_fd);

    {
        unsigned char *marker = NULL;
        size_t i;
        for (i = 3; i < used; ++i) {
            if (buffer[i - 3] == '\r' && buffer[i - 2] == '\n' &&
                buffer[i - 1] == '\r' && buffer[i] == '\n') {
                marker = buffer + i - 3;
                break;
            }
        }
        if (marker == NULL) {
            free(buffer);
            return -1;
        }
        header_size = (size_t)(marker - buffer) + 4;
    }

    if (sscanf((char *)buffer, "HTTP/%*s %d", &status) != 1) {
        free(buffer);
        return -1;
    }
    {
        char *length_header = strstr((char *)buffer, "Content-Length:");
        if (length_header == NULL) length_header = strstr((char *)buffer, "content-length:");
        if (length_header != NULL) sscanf(length_header, "%*[^:]: %d", &content_length);
    }

    body_size = used - header_size;
    if (content_length >= 0 && (size_t)content_length < body_size)
        body_size = (size_t)content_length;

    *response = malloc(body_size ? body_size : 1);
    if (*response == NULL) {
        free(buffer);
        return -1;
    }
    if (body_size != 0) memcpy(*response, buffer + header_size, body_size);
    *response_size = body_size;
    free(buffer);
    return status >= 200 && status < 300 ? 0 : -1;
}

int bridge_client_start(BridgeClient *client)
{
    const char *manage = getenv("MOTIF_APPLE_MUSIC_MANAGE_BRIDGE");
    const char *node = getenv("MOTIF_APPLE_MUSIC_NODE");
    const char *script = getenv("MOTIF_APPLE_MUSIC_BRIDGE_SCRIPT");
    pid_t pid;

    if (manage != NULL && strcmp(manage, "0") == 0) return BRIDGE_START_OK;
    if (node == NULL || node[0] == '\0') node = "node";
    if (script == NULL || script[0] == '\0') script = "bridge/server.mjs";
    if (!executable_available(node)) return BRIDGE_START_NODE_MISSING;

    pid = fork();
    if (pid < 0) return BRIDGE_START_FAILED;
    if (pid == 0) {
        execlp(node, node, script, (char *)NULL);
        _exit(127);
    }
    client->child_pid = pid;
    return BRIDGE_START_OK;
}

int bridge_client_wait_ready(BridgeClient *client, unsigned int timeout_ms)
{
    unsigned int waited = 0;
    char response[2048];
    while (waited < timeout_ms) {
        if (bridge_client_request(client, "GET", "/v1/status", NULL,
                                  response, sizeof(response)) == 0)
            return 0;
        sleep_ms(100);
        waited += 100;
    }
    return -1;
}

void bridge_client_stop(BridgeClient *client)
{
    char response[2048];
    if (client->child_pid <= 0) return;
    bridge_client_request(client, "POST", "/v1/shutdown", "{}", response, sizeof(response));
    kill(client->child_pid, SIGTERM);
    waitpid(client->child_pid, NULL, 0);
    client->child_pid = -1;
}
