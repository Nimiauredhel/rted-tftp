#include "networking_common.h"

bool parse_address(char *addr_str, struct in_addr *addr_bin)
{
    return (inet_pton(AF_INET, addr_str, addr_bin) >= 0);
}
