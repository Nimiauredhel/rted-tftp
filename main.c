#include "common.h"
#include "client.h"
#include "server.h"

#define OPERATION_MODES_COUNT 4
#define OPERATION_MODES_MAXLENGTH 8

typedef struct OperationMode
{
    const uint8_t min_argument_count;
    const char input_string[OPERATION_MODES_MAXLENGTH];
    const char description_string[32];
    const char usage_format_string[32];

} OperationMode_t;

static int8_t get_selection_from_args(int argc, char *argv[]);
static void print_usage(const char* process_name);

static const OperationMode_t operation_modes[OPERATION_MODES_COUNT] =
{
    { 2, "serve", "Serve storage folder to clients", "%s %s" },
    { 4, "write", "Write named file to server", "%s %s <server ip> <filename>" },
    { 4, "read", "Read named file from server", "%s %s <server ip> <filename>" },
    { 4, "delete", "Erase named file from server", "%s %s <server ip> <filename>" },
};

int main(int argc, char *argv[])
{
    printf("Welcome to Untitled TFTP App.\n");

    switch (get_selection_from_args(argc, argv))
    {
        case 0:
            server_start();
            break;
        case 1:
            client_wrq(argc, argv);
            break;
        case 2:
            client_rrq(argc, argv);
            break;
        case 3:
            client_drq(argc, argv);
            break;
        case -1:
        default:
            printf("Invalid input!\n");
            print_usage(argv[0]);
            printf("Terminating.\n");
            return EINVAL;
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
            if (0 == strncmp(argv[1], operation_modes[i].input_string, OPERATION_MODES_MAXLENGTH))
            {
                if (argc >= operation_modes[i].min_argument_count)
                {
                    selection = i;
                }
                else
                {
                    printf("Missing arguments for requested operation! Usage:\n");
                    printf(operation_modes[i].usage_format_string, argv[0], operation_modes[i].input_string[i]);
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
        printf(" ~ %s - \n   ", operation_modes[i].description_string);
        printf(operation_modes[i].usage_format_string, process_name, operation_modes[i].input_string);
        printf("\n");
    }
}
