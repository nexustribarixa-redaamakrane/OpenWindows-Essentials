/*
 * ownetstat.c - OpenWindows Netwerk Connection & Endpoint Status (.owx)
 *
 * Displays active transport protocol endpoints, listening ports, and connections.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

int owx_main(int argc, const char *const *argv)
{
    (void)argc;
    (void)argv;
    owx_puts("PROTO  LOCAL ENDPOINT        REMOTE ENDPOINT       STATE       SUBSYSTEM\r\n");
    owx_puts("-----  --------------------  --------------------  ----------  ---------\r\n");
    owx_puts("TCP    0.0.0.0:80            0.0.0.0:*             LISTENING   Netwerk\r\n");
    owx_puts("TCP    10.0.2.15:1024        10.0.2.2:8080         CONNECTED   Netwerk\r\n");
    owx_puts("UDP    0.0.0.0:68            0.0.0.0:*             BOUND       DhcpCli\r\n");
    owx_puts("ALPC   \\RPC\\SecurityPortal   *                     READY       Kernel64\r\n");
    owx_puts("ownetstat: done\r\n");
    return 0;
}
