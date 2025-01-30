#include "common.h"
#include "networking_common.h"
#include "tftp_common.h"
#include "client.h"
#include "server.h"

static int8_t get_selection_from_args(int argc, char *argv[]);
static void print_usage(const char* process_name);

int main(int argc, char *argv[])
{
    printf("Welcome to Untitled TFTP App.\n");

    uint8_t selection = get_selection_from_args(argc, argv);

    if (selection < 0)
    {
        printf("Invalid input!\n");
        print_usage(argv[0]);
        printf("Terminating.\n");
        return EINVAL;
    }
    else if (selection == 0)
    {
        server_start();
    }
    else
    {
        TFTPOpcode_t opcode;
        OperationData_t *data;

        switch (selection)
        {
            case 1:
                opcode = TFTP_WRQ;
                break;
            case 2:
                opcode = TFTP_RRQ;
                break;
            case 3:
                opcode = TFTP_DRQ;
                break;
        }

        data = tftp_init_operation_data(opcode, argv[2], argv[3],
                argc > 4 ? argv[4] : NULL,
                argc > 5 ? argv[5] : NULL);
        client_start(data);
    }

    return EXIT_SUCCESS;
}

static int8_t get_selection_from_args(int argc, char *argv[])
{
    int8_t selection = -1;

    if (argc > 1)
    {
        for (uint8_t i = 0; i < OPERATION_MODES_COUNT; i++)
        {
            if (0 == strncmp(argv[1], tftp_operation_modes[i].input_string, OPERATION_MODES_MAXLENGTH))
            {
                if (argc >= tftp_operation_modes[i].min_argument_count)
                {
                    selection = i;
                }
                else
                {
                    printf("Missing arguments for requested operation!\n Usage:\n   ");
                    printf(tftp_operation_modes[i].usage_format_string, argv[0], tftp_operation_modes[i].input_string);
                    printf("\nTerminating.\n");
                    exit(EINVAL);
                }

                break;
            }
        }
    }

    return selection;
}

static void print_usage(const char* process_name)
{
    printf(" Usage:\n");

    for (uint8_t i = 0; i < OPERATION_MODES_COUNT; i++)
    {
        printf(" ~ %s - \n   ", tftp_operation_modes[i].description_string);
        printf(tftp_operation_modes[i].usage_format_string, process_name, tftp_operation_modes[i].input_string);
        printf("\n");
    }
}
