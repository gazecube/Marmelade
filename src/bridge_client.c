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
    (void)setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    (void)setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

static int write_all(int socket_fd, const char *data, size_t length)
{
    size_t sent = 0;
    while (sent < length) {
        ssize_t count = write(socket_fd, data + sent, length - sent);
        if (count > 0) {
            sent += (size_t)count;
            continue;
        }
        if (count < 0 && errno == EINTR) continue;
        return -1;
    }
    return 0;
}

void bridge_client_init(BridgeClient *client)
{
    const char *url = getenv("MOTIF_APPLE_MUSIC_BRIDGE_URL");
    memset(client, 0, sizeof(*client));
    snprintf(client->host, sizeof(client->host), "%s", "127.0.0.1");
    client->port = 17876;
    client->managed = getenv("MOTIF_APPLE_MUSIC_MANAGE_BRIDGE") == NULL ||
                      strcmp(getenv("MOTIF_APPLE_MUSIC_MANAGE_BRIDGE"), "0") != 0;
    if (url != NULL) {
        char host[256];
        unsigned int port;
        if (sscanf(url, "http://%255[^:]:%u", host, &port) == 2 && port < 65536) {
            snprintf(client->host, sizeof(client->host), "%s", host);
            client->port = (unsigned short)port;
        }
    }
}

int bridge_client_start(BridgeClient *client)
{
    const char *node;
    const char *script;
    char response[512];
    pid_t pid;
    if (!client->managed)
        return 0;

    /* A prior client may already own the bridge. Reuse it only after verifying
       the service identity; never attach to an arbitrary process on the port. */
    if (bridge_client_request(client, "GET", "/v1/status", NULL,
                              response, sizeof(response)) == 0) {
        if (strstr(response, "motif-apple-music-bridge") != NULL &&
            strstr(response, "\"apiVersion\":8") != NULL) {
            client->managed = 0;
            return BRIDGE_START_OK;
        }
        return BRIDGE_START_INCOMPATIBLE;
    }

    node = getenv("MOTIF_APPLE_MUSIC_NODE");
    script = getenv("MOTIF_APPLE_MUSIC_BRIDGE_SCRIPT");
    if (node == NULL) node = "node";
    if (script == NULL) script = "bridge/server.mjs";
    if (!executable_available(node))
        return BRIDGE_START_NODE_MISSING;
    pid = fork();
    if (pid < 0) return BRIDGE_START_ERROR;
    if (pid == 0) {
        execlp(node, node, script, (char *)NULL);
        _exit(127);
    }
    client->child_pid = pid;
    return BRIDGE_START_OK;
}

int bridge_client_request(BridgeClient *client, const char *method,
                          const char *path, const char *body,
                          char *response, size_t response_size)
{
    struct addrinfo hints, *addresses = NULL, *item;
    char port[16], request[4096], incoming[8192];
    const char *payload = body == NULL ? "" : body;
    size_t used = 0;
    int socket_fd = -1, status = -1, header_done = 0, http_status = 0;
    ssize_t count;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(port, sizeof(port), "%u", client->port);
    if (getaddrinfo(client->host, port, &hints, &addresses) != 0) return -1;
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
             "%s %s HTTP/1.1\r\nHost: %s:%u\r\nContent-Type: application/json\r\n"
             "Content-Length: %zu\r\nConnection: close\r\n\r\n%s",
             method, path, client->host, client->port, strlen(payload), payload);
    if (write_all(socket_fd, request, strlen(request)) != 0) goto done;
    while ((count = read(socket_fd, incoming, sizeof(incoming))) > 0) {
        size_t i;
        if (http_status == 0 && (size_t)count >= 12 &&
            memcmp(incoming, "HTTP/", 5) == 0)
            sscanf(incoming, "HTTP/%*s %d", &http_status);
        for (i = 0; i < (size_t)count; ++i) {
            if (!header_done) {
                static const char marker[] = "\r\n\r\n";
                request[used < sizeof(request) ? used : 0] = incoming[i];
                if (used < 3) used++;
                else {
                    request[0] = request[1]; request[1] = request[2];
                    request[2] = request[3]; request[3] = incoming[i];
                    used = 4;
                    if (memcmp(request, marker, 4) == 0) { header_done = 1; used = 0; }
                }
            } else if (used + 1 < response_size) {
                response[used++] = incoming[i];
            }
        }
    }
    if (count < 0 && errno != EINTR) goto done;
    if (response_size > 0) response[used] = '\0';
    status = header_done && http_status >= 200 && http_status < 300 ? 0 : -1;
done:
    close(socket_fd);
    return status;
}

int bridge_client_request_bytes(BridgeClient *client, const char *method,
                                const char *path, const char *body,
                                unsigned char *response, size_t response_size,
                                size_t *response_length)
{
    struct addrinfo hints, *addresses = NULL, *item;
    char port[16], request[4096], headers[8192];
    const char *payload = body == NULL ? "" : body;
    size_t header_used = 0, body_used = 0;
    int socket_fd = -1, status = -1, header_done = 0, http_status = 0, overflow = 0;
    unsigned char incoming[8192];
    ssize_t count;

    if (response_length != NULL) *response_length = 0;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(port, sizeof(port), "%u", client->port);
    if (getaddrinfo(client->host, port, &hints, &addresses) != 0) return -1;
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
             "%s %s HTTP/1.1\r\nHost: %s:%u\r\nContent-Type: application/json\r\n"
             "Content-Length: %zu\r\nConnection: close\r\n\r\n%s",
             method, path, client->host, client->port, strlen(payload), payload);
    if (write_all(socket_fd, request, strlen(request)) != 0) goto done;

    while ((count = read(socket_fd, incoming, sizeof(incoming))) > 0) {
        size_t i;
        for (i = 0; i < (size_t)count; ++i) {
            if (!header_done) {
                if (header_used + 1 >= sizeof(headers)) { overflow = 1; goto done; }
                headers[header_used++] = (char)incoming[i];
                headers[header_used] = '\0';
                if (header_used >= 4 &&
                    memcmp(headers + header_used - 4, "\r\n\r\n", 4) == 0) {
                    header_done = 1;
                    sscanf(headers, "HTTP/%*s %d", &http_status);
                }
            } else {
                if (body_used < response_size) response[body_used] = incoming[i];
                else overflow = 1;
                body_used++;
            }
        }
    }
    if (count < 0 && errno != EINTR) goto done;
    if (response_length != NULL) *response_length = body_used;
    status = header_done && !overflow && http_status >= 200 && http_status < 300 ? 0 : -1;
done:
    if (response_length != NULL) *response_length = body_used;
    close(socket_fd);
    return status;
}

int bridge_client_wait_ready(BridgeClient *client, unsigned int timeout_ms)
{
    unsigned int elapsed;
    char response[512];
    for (elapsed = 0; elapsed < timeout_ms; elapsed += 100) {
        if (bridge_client_request(client, "GET", "/v1/status", NULL,
                                  response, sizeof(response)) == 0)
            return 0;
        sleep_ms(100);
    }
    return -1;
}

void bridge_client_stop(BridgeClient *client)
{
    if (client->managed && client->child_pid > 0) {
        kill(client->child_pid, SIGTERM);
        waitpid(client->child_pid, NULL, 0);
        client->child_pid = 0;
    }
}
