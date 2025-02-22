#ifndef CLIENT_H
#define CLIENT_H

#include "common.h"
#include "networking_common.h"
#include "tftp_common.h"

/**
 * Entry point for the TFTP client.
 * Handles the request, acknowledgement and actual operation
 * on a single thread, and terminates.
 */
bool client_start_operation(OperationData_t *op_data);

#endif
