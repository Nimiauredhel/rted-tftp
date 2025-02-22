#include "networking_common.h"

/**
 * Conveniently wraps the inet_pton() function which converts an IP address
 * to its network binary representation and writes to an existing address struct.
 */
bool parse_address(char *addr_str, struct in_addr *addr_bin)
{
    return (inet_pton(AF_INET, addr_str, addr_bin) >= 0);
}
