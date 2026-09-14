#ifndef MOTIF_APPLE_MUSIC_BRIDGE_CLIENT_H
#define MOTIF_APPLE_MUSIC_BRIDGE_CLIENT_H

#include <stddef.h>
#include <sys/types.h>

typedef struct {
    char base_url[512];
    pid_t child_pid;
} BridgeClient;

#define BRIDGE_START_OK 0
#define BRIDGE_START_ERROR -1
#define BRIDGE_START_FAILED BRIDGE_START_ERROR
#define BRIDGE_START_INCOMPATIBLE -2
#define BRIDGE_START_NODE_MISSING -3

void bridge_client_init(BridgeClient *client);
int bridge_client_start(BridgeClient *client);
int bridge_client_wait_ready(BridgeClient *client, unsigned int timeout_ms);
int bridge_client_request(BridgeClient *client, const char *method,
                          const char *path, const char *body,
                          char *response, size_t response_size);
int bridge_client_request_bytes(BridgeClient *client, const char *method,
                                const char *path, const char *body,
                                unsigned char **response,
                                size_t *response_size);
void bridge_client_stop(BridgeClient *client);

#endif
