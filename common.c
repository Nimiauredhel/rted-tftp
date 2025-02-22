#include "common.h"

bool should_terminate = false;

static bool random_was_seeded = false;

void initialize_signal_handler(void)
{
    should_terminate = false;

    struct sigaction action;
    explicit_bzero(&action, sizeof(action));
    action.sa_handler = signal_handler;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGHUP, &action, NULL);
}

// calling random_range once to ensure that random is seeded
void initialize_random_seed(void)
{
    usleep(random_range(1234, 5678));
}

void signal_handler(int signum)
{
    switch (signum)
    {
        case SIGINT:
        case SIGTERM:
        case SIGHUP:
            should_terminate = true;
            break;
    }
}

int random_range(int min, int max)
{
    if (!random_was_seeded)
    {
        srand(time(NULL) + getpid());
        random_was_seeded = true;
    }

    int random_number = rand();
    random_number = (random_number % (max - min + 1)) + min;
    return random_number;
}

float seconds_since_clock(struct timespec start_clock)
{
    struct timespec now_clock;
    clock_gettime(CLOCK_MONOTONIC, &now_clock);
    float elapsed_float = (now_clock.tv_nsec - start_clock.tv_nsec) / 1000000000.0;
    elapsed_float += (now_clock.tv_sec - start_clock.tv_sec);
    return elapsed_float;
}
