#include "client.h"

void client_wrq(int argc, char *argv[])
{
    // open file to send
    FILE *file = fopen(argv[3], "r");

    // initialize server address with initial (requests) port
    struct sockaddr_in peer_address =
    {
        .sin_family = AF_INET,
        .sin_port = htons(69),
    };

    inet_pton(AF_INET, argv[2], &peer_address.sin_addr);

    // TODO: send rrq and await ack here

    transmit_file(file, TFTP_MODE_NETASCII, 0, peer_address);
}

void client_rrq(int argc, char *argv[])
{
    init_storage();
}

void client_drq(int argc, char *argv[])
{
}
