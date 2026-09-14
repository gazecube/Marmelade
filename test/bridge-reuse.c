#include "bridge_client.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    BridgeClient client;
    int result;
    bridge_client_init(&client);
    result = bridge_client_start(&client);
    if (getenv("EXPECT_INCOMPATIBLE") != NULL) {
        if (result != BRIDGE_START_INCOMPATIBLE) {
            fputs("client accepted an incompatible bridge\n", stderr);
            return 1;
        }
        puts("incompatible bridge rejection test passed");
        return 0;
    }
    if (result != BRIDGE_START_OK) {
        fputs("bridge_client_start failed\n", stderr);
        return 1;
    }
    if (client.managed || client.child_pid != 0) {
        fputs("client spawned a duplicate bridge\n", stderr);
        bridge_client_stop(&client);
        return 1;
    }
    puts("existing bridge reuse test passed");
    return 0;
}
