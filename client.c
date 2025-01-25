#include "client.h"

static void client_cleanup(ClientSideData_t *data)
{
    free(data);
}

static void client_init(ClientSideData_t *data)
{
    init_storage();
}

void client_start(void)
{
    ClientSideData_t *data = malloc(sizeof(ClientSideData_t));
    client_init(data);
    client_cleanup(data);
}
