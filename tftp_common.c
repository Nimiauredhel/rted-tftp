#include "tftp_common.h"

CommonData_t tftp_common_data;
char tftp_mode_strings[TFTP_MODES_COUNT][TFTP_MODES_STRING_LENGTH] =
{
    "netascii",
    "octet"
};

void init_storage(void)
{
    if (mkdir(STORAGE_PATH, 0777) == 0)
    {
        printf("Initialized storage directory at path '%s'.\n", STORAGE_PATH);
    }
    else
    {
        if (errno == EEXIST)
        {
            printf("Existing storage directory detected at path '%s'.\n", STORAGE_PATH);
        }
        else
        {
            perror("Error creating/finding storage directory");
            exit(EXIT_FAILURE);
        }
    }
}
