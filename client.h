#ifndef CLIENT_H
#define CLIENT_H

#include "common.h"
#include "networking_common.h"
#include "tftp_common.h"

void client_wrq(int argc, char *argv[]);
void client_rrq(int argc, char *argv[]);
void client_drq(int argc, char *argv[]);

#endif
