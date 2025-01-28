#include "client.h"

static void client_init(void)
{
    init_storage();
}

void client_wrq(int argc, char *argv[])
{
    init_storage();
}
void client_rrq(int argc, char *argv[])
{
    init_storage();
}
void client_drq(int argc, char *argv[])
{
    init_storage();
}
