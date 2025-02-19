#include "common.h"
#include "networking_common.h"
#include "tftp_common.h"
#include "client.h"
#include "server.h"

static int8_t get_selection_from_args(int argc, char *argv[]);
static void print_usage(const char* process_name);
static void exit_bad_input(char *process_name);

int main(int argc, char *argv[])
{
    printf("Welcome to Untitled TFTP App.\n");

    initialize_signal_handler();

    int8_t selection = get_selection_from_args(argc, argv);

    if (selection < 0)
    {
        exit_bad_input(argv[0]);
    }
    else if (selection == 0)
    {
        server_start();
    }
    else if (argc > 3)
    {
        OperationId_t op_id;
        struct in_addr peer_address_bin;
        OperationData_t *data;

        if (!parse_address(argv[2], &peer_address_bin))
        {
            fprintf(stderr, "Failed to parse peer address (%s): %s\n", argv[3], strerror(errno));
            return EXIT_FAILURE;
        }

        printf("Parsed peer address (%s).\n", argv[2]);

        switch (selection)
        {
            case 1:
                op_id = TFTP_OPERATION_SEND;
                break;
            case 2:
                op_id = TFTP_OPERATION_RECEIVE;
                break;
            case 3:
                op_id = TFTP_OPERATION_REQUEST_DELETE;
                break;
            default:
                fputs("client parsed invalid operation id", stderr);
                return EXIT_FAILURE;
        }

        data = tftp_init_operation_data(op_id,
                init_peer_socket_address(peer_address_bin, htons(69)),
                argv[3],
                argc > 4 ? argv[4] : NULL,
                argc > 5 ? argv[5] : NULL);

        if (data == NULL)
        {
            fputs("Failed to initialize operation data. Terminating.\n", stderr);
            return EXIT_FAILURE;
        }
        else
        {
            bool operation_success = client_start_operation(data);
            printf("Operation %s.\n", operation_success ? "completed" : "aborted");
            tftp_free_operation_data(data);
        }
    }
    else
    {
        exit_bad_input(argv[0]);
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
            if (0 == strncmp(argv[1], tftp_operation_modes[i].input_string, OPERATION_MODES_MAX_LENGTH))
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

static void exit_bad_input(char *process_name)
{
    printf("Invalid input!\n");
    print_usage(process_name);
    printf("Terminating.\n");
    exit(EXIT_FAILURE);
}
